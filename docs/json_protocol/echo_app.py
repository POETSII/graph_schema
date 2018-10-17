import io
import sys

from server_connection import *

RecvHook=Callable[[Endpoint,MulticastMessage],None]

class PseudoServer:
    class ConnectionInfo:
        def __init__(self, connection:DownwardConnection):
            self.connection=connection
            self.outgoing=[] # type: List[MulticastMessage]
            self.halt=None

        def post(self, message:MulticastMessage):
            self.outgoing.append(message)

    class Events(DownwardConnectionEvents):
        def __init__(self, parent:"PseudoServer"):
            self._parent=parent
            self._connection_to_info={} # type: Map[DownwardConnection,ConnectionInfo]

        def on_connect(self, connection, owner, owner_cookie, graph_type, graph_instance, owned_devices:Sequence[str]) :
            parent=self._parent
            self.requireParameterNone(owner_cookie, "bind.owner_cookie")
            self.requireParameterWildcardOrEqual(graph_type, parent.graph_type, "bind.graph_type")
            self.requireParameterWildcardOrEqual(graph_instance, parent.graph_instance, "bind.graph_instance")

            for e in owned_devices:
                self.require(e in parent._externals, "No external device called {}", e)
                self.require(e not in parent._externals_connected, "A connection has already bound to external {}", e)

            info=PseudoServer.ConnectionInfo(connection)
            self._connection_to_info[connection]=info
            
            # Create a map of endpoints to endpoints within the connection
            incoming_edges={} # type: Dict[str,Sequence[str]]
            for e in owned_devices:
                for (p,srcs) in parent._dst_to_src.get(e,{}).items():
                    for src in srcs:
                        incoming_edges.setdefault(src.full,[]).append(Endpoint(e,p).full)
                        parent._external_routes.setdefault(src,set()).add(info)

            
            # Record that each external is connected
            parent._externals_connected |= set(owned_devices)
                
            return ConnectResults(parent.graph_type, parent.graph_instance, incoming_edges)

        def on_run(self, connection) :
            parent=self._parent
            return len(parent._externals_connected)==len(parent._externals)

        def on_send(self, connection, messages:List[MulticastMessage]):
            parent=self._parent
            info=self._connection_to_info[connection]
            assert info.halt==None
            for m in messages:
                parent._route_message(m)

        def on_poll(self, connection, max_messages):
            assert max_messages > 0
            parent=self._parent
            info=self._connection_to_info[connection]
            assert not info.halt
            todo=min(max_messages, len(info.outgoing))
            res=info.outgoing[:todo]
            info.outgoing=info.outgoing[todo:]
            if parent._halt and len(res) < max_messages:
                info.halt=parent._halt
                res.append(parent._halt)
            return res

        def on_halt(self, connection, code, message):
            parent=self._parent
            if not parent._halt:
                parent._halt=Halt(code, message)

    def __init__(self, graph_type:str, graph_instance:str):
        self.graph_type=graph_type
        self.graph_instance=graph_instance
        self._devices=set() # type: Set[Str]
        self._externals=set() # type: Set[Str]
        self._internals=set() # type: Set[Str]
        self._connections=[] # type: List[Connection]
        self._internal_bindings={} # type: Dict[Str,Tuple[Endpoint,RecvHook]]
        self._externals_connected=set() # type: Set[str]
        self._external_routes={}   # type: Dict[Endpoint,Set[ConnectionInfo]]
        self._src_to_dst={}  #type:  Dict[Endpoint,Set[Endpoint]]
        self._dst_to_src={}  #type:  Dict[str,Dict[str,Set[Endpoint]]]
        self._default_hook=lambda dst,msg: self.default_recv_hook(dst,msg)
        self._running=False
        self._halt=False
        self._events=PseudoServer.Events(self)

    def default_recv_hook(self, dst:Endpoint, msg:MulticastMessage):
        raise NotImplementedError()

    def add_external(self, id:str):
        assert id not in self._devices
        assert not self._running
        self._externals.add(id)
        self._devices.add(id)
    
    def add_internal(self, id:str, on_recv:Optional[RecvHook]=None):
        assert id not in self._devices
        assert not self._running
        self._internals.add(id)
        self._devices.add(id)
        self._internal_bindings[id]=on_recv or self._default_hook

    def add_route(self, dst:Endpoint, src:Endpoint):
        assert dst.device in self._devices
        assert src.device in self._devices
        assert not self._running
        
        self._src_to_dst.setdefault(src,set()).add(dst)
        self._dst_to_src.setdefault(dst.device,{}).setdefault(dst.port,set()).add(src)

    def add_connection(self, connection:DownwardConnection):
        self._connections.append(connection)

    def _route_message(self, msg:MulticastMessage):
        assert self._running
        for dst in self._src_to_dst.get(msg.src,set()):
            if dst.device in self._internals:
                self._internal_bindings[dst.device](dst,msg)
        for connection in self._external_routes.get(msg.src,[]):
            connection.post(msg)


    def send(self, msg:MulticastMessage):
        assert msg.src.device in self._internals
        self._route_message(msg)

    def run(self, event : threading.Event):
        print("running")
        assert not self._running
        self._running=True
        try:
            while not event.is_set():
                for c in self._connections:
                    c.do_events(self._events)
        except EOFError:
            return


def create_echo_app_server(count:int):
    server=PseudoServer("echo", "echo_{}".format(count))

    def on_msg(dst:Endpoint, msg:MulticastMessage):
        msg=MulticastMessage(Endpoint(dst.device,"out"),msg.data)
        server.send(msg)

    for i in range(count):
        internal="int{}".format(i)
        external="ext{}".format(i)
        server.add_internal(internal, on_msg)
        server.add_external(external)
        server.add_route(Endpoint(internal,"in"),Endpoint(external,"out"))
        server.add_route(Endpoint(external,"in"),Endpoint(internal,"out"))

    return server


import unittest


if __name__=="__main__":
    count=1
    if len(sys.argv)>1:
        count=int(sys.argv[1])

    server=create_echo_app_server(count)

    timeout=1.0
    expectTimeout=False
    
    channel=JSONRawChannelOnStreams(sys.stdin, sys.stdout)
    pull=JSONServerPull(channel)
    dc=DownwardConnection(pull)

    server.add_connection(dc)

    event=threading.Event()
    worker=threading.Thread(target=lambda: server.run(event))
    worker.isDaemon=True
    worker.start()
    worker.join(timeout)
    timedOut=False
    if worker.is_alive:
        event.set()
        worker.join()
        timedOut=True
    if timedOut == expectTimeout:
        sys.exit(0)
    else:
        sys.exit(1)
