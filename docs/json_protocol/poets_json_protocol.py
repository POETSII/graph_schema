from typing import *
from io import BytesIO, StringIO
import json
import multiprocessing
from enum import Enum, unique

from json_rpc_helpers import *

@unique
class ProtocolError(Enum):
    ConnectionFinished = -1     # this method failed because the connection has moved into the FINISHED state.
    ConnectionErrored = -2      # this method failed because the connection has encountered an unrecoverable error.
    
    InvalidDevice = -3          # a method referenced a device which is not known to the server.
    InvalidEndpoint = -4        # a method referenced an endpoint which does not appear to exist.
    InvalidDirection = -5       # a method used an endpoint in the wrong direction (e.g. send from an input endpoint).
    
    GraphTypeMismatch = -6      # client tried to connect to a graph that doesn't match what the server has.
    GraphInstanceMismatch = -7  # client tried to connect to a graph instance that doesn't match what the server has.
    InvalidOwner = -9           #  The owner passed to `bind` is invalid or unknown.
    InvalidCookie = -10         # The owner_cookie passed to `bind` is invalid or unknown.

class Endpoint:
    __slots__=["device","port","_full"]
    def __init__(self, pathOrDevice:str, port:Union[str,None]=None):
        if port==None:
            self._full=pathOrDevice
            (self.device,self.port)=pathOrDevice.split(":")
        else:
            self._full="{}:{}".format(pathOrDevice,port)
            self.device=pathOrDevice
            self.port=port

    def __eq__(self,o):
        return self._full==o._full

    def __ne__(self,o):
        return self._full!=o._full
    
    def __str__(self):
        return self._full


class Event:
    __slots__=["type"]

    def __init__(self, type:str):
        self.type=type
    
    def toJSON(self) -> JSON:
        raise NotImplementedError()

class Message(Event):
    __slots__=["data"]

    def __init__(self, data):
        super().__init__("msg")
        self.data=data

class Halt(Event):
    __slots__=["code","message"]

    def __init__(self, code:int, message:Optional[str]=None):
        super().__init__("halt")
        self.code=code
        self.message=message

    def toJSON(self) -> JSON:
        return {"type":"halt", "code":self.code, "message":self.message}


class MulticastMessage(Message): 
    __slots__=["src"]

    def __init__(self, src:Union[str,Endpoint], data:Optional[JSON]=None):
        super().__init__(data)
        if isinstance(src,Endpoint):
            self.src=src
        else:
            self.src=Endpoint(src)

    def toJSON(self) -> JSON:
        if self.data:
            return {"src":str(self.src),"data":self.data}
        else:
            return {"src":str(self.src)}

    def __str__(self) -> str:
        return json.dumps(self.toJSON())

    def __eq__(self,o):
        assert isinstance(o,MulticastMessage)
        return self.src==o.src and self.data==o.data
    
    def __ne__(self,o):
        return not self==o

def json_object_to_event(object:JSONObject) -> Event:
    type=object.get("type","msg")
    if type=="msg":
        return MulticastMessage(src=Endpoint(object["src"]), data=object.get("data"))
    elif type=="halt":
        return Halt(code=int(object["code"]), message=object.get("message"))
    else:
        raise RuntimeError("Unknown event type '{}'".format(type))

def json_objects_to_events(objects:JSONArray) -> Sequence[Event]:
    return [ json_object_to_event(o) for o in objects ]

def events_to_json_objects(events : Sequence[Event]) -> JSONArray:
    return [ e.toJSON() for e in events ]

def contains_halt(events:Sequence[Event]) -> bool:
    for e in events:
        if isinstance(e,Halt):
            return True
    return False

class ConnectionState(Enum):
    CONNECTED = 0
    BOUND = 1
    RUN_REQUESTED = 2      # Bind has been called, but not yet completed
    RUNNING = 3
    HALT_REQUESTED = 4     # halt has been called, but not yet delivered 
    FINISHED = 5           # halt has been delivered
    ERRORED = 6

#class ConnectResults(NamedTuple):
#    graph_type: str
#    graph_instance: str
#    incoming_edges: Dict[Endpoint,List[Endpoint]]

ConnectResults=NamedTuple('ConnectResults',[
    ('graph_type',str),
    ('graph_instance',str),
    ('incoming_edges', Dict[Endpoint,List[Endpoint]])
])

class DownwardConnectionEvents:
    """Callback events for a particle connection."""
    
    def on_connect(self, connection:"DownwardConnection",
            owner:str, owner_cookie:str, graph_type:str, graph_instance:str, owned_devices:Set[str]
            ) -> ConnectResults :
        raise NotImplementedError

    def on_run(self, connection:"DownwardConnection") -> bool:
        """Indicates that the client has blocked on `run`. The event handler should return True to
        complete the call and unblock the client, or False to keep the call open."""
        raise NotImplementedError

    def on_send(self, connection:"DownwardConnection", messages:List[MulticastMessage]):
        raise NotImplementedError

    def on_poll(self, connection:"DownwardConnection",
            max_messages:Optional[int]
        ) -> Sequence[Event] :
        raise NotImplementedError

    def on_halt(self, connection:"DownwardConnection", code:int, message:Optional[str]):
        raise NotImplementedError


class DownwardConnection:
    
    def __init__(self, connection:JSONServerPull):
        self._connection=connection
        self._state=ConnectionState.CONNECTED
        self._devices=set() # : Set[str]
        self._pending_polls=[] # type: List[ Pair[str,int] ]

    def _do_events_CONNECTED(self, events:DownwardConnectionEvents) -> bool:
        print("CONNECTED")
        (method,id,params)=self._connection.try_begin(valid_methods=["bind"])
        if method==None:
            print("  nothing")
            return False
        try:
            assert method=="bind"
            self._owner=params.get("owner", "user")
            self._owner_cookie=params.get("owner", None)
            graph_type=params.get("graph_type", "*")
            graph_instance=params.get("graph_instance", "*")
            self._devices=set(params.get("owned_devices",[]))
            (self.graph_type,self.graph_instance,self.incoming_edges)=events.on_connect(self, self._owner, self._owner_cookie, graph_type, graph_instance, self._devices)
            self._connection.complete(id, {
                "magic":"POETS-external-JSON-server",
                "graph_type":self.graph_type,
                "graph_instance":self.graph_instance,
                "incoming_edges" : self.incoming_edges
            })
            self._state=ConnectionState.BOUND
            return True
        except Exception as e:
            self._state=ConnectionState.ERRORED
            self._connection.error(id, e)
            raise

    def _do_events_BOUND(self, events:DownwardConnectionEvents) -> bool:
        print("BOUND")
        
        (method,self._bind_id,params)=self._connection.try_begin(valid_methods=["run"])
        if method==None:
            return False
        self._state=ConnectionState.RUN_REQUESTED
        return True

    def _do_events_RUN_REQUESTED(self, events:DownwardConnectionEvents) -> bool:
        print("RUN_REQUESTED")
        try:
            if not events.on_run(self):
                return False
        except Exception as e:
            self._state=ConnectionState.ERRORED
            self._connection.error(self._bind_id, e)
            raise
        assert self._bind_id
        self._connection.complete(self._bind_id)
        self._state=ConnectionState.RUNNING
        return True

    def _do_events_async_poll(self, events:DownwardConnectionEvents) -> bool:
        if len(self._pending_polls)==0:
            return False 

        # There is at least one outstanding poll request
        (id,max_events)=self._pending_polls[0]
        events=events.on_poll(self, max_events)
        if len(events)==0:
            return False

        assert len(events)<=max_events
        self._pending_polls.pop(0)
        self._connection.complete(id, { "events":events_to_json_objects(events) })
        # Check for halt messages
        if contains_halt(events):
            for (prev_id,_) in self._pending_polls:
                self._connection.complete(prev_id, { "events":[] } )
            self._pending_polls=[]
            self._state=ConnectionState.FINISHED
        return True

    def _do_events_RUNNING(self, events:DownwardConnectionEvents) -> bool:
        print("RUNNING")
        (method,id,params)=self._connection.try_begin(valid_methods=["poll","send","halt"])
        if method:
            if method=="send": 
                events.on_send(self, json_objects_to_events(params.get("messages",[])))
                self._connection.complete(id)
            elif method=="poll":
                max_messages=params.get("max_messages",2**32)
                is_async=params.get("async", False)
                if not is_async:
                    # Have to deal with it immediately
                    events=events.on_poll(self,max_messages)
                    self._connection.complete(id, { "events" : events_to_json_objects(events) })
                    if contains_halt(events):
                        for (prev_id,_) in self._pending_polls:
                            self._connection.complete(prev_id, {"events":[]} )
                        self._pending_polls=[]
                else:
                    # Will deal with it later
                    self._pending_polls.append( (id,max_messages) )
            elif method=="halt":
                # Note that we don't transition to FINISHED ourselves, the controller will do it.
                events.on_halt(self, params["code"], params.get("message",None) )
                self._state=ConnectionState.HALT_REQUESTED
                self._connection.complete(id)
            else:
                assert False, "Invalid method in this state"
            return True
        else:
            # There was no message to process, so see if we can finish some
            return self._do_events_async_poll(events)
            
    def _do_events_HALT_REQUESTED(self, events:DownwardConnectionEvents) -> bool:
        print("HALT_REQUESTED")
        (method,id,params)=self._connection.try_begin(valid_methods=["poll"])
        if method:
            if method=="poll":
                max_messages=params.get("max_messages",2**32)
                is_async=params.get("async", False)
                if not is_async:
                    # Have to deal with it immediately
                    events=events.on_poll(self,max_messages)
                    self._connection.complete(id, { "events" : events_to_json_objects(events) })   
                    if contains_halt(events):
                        self._state=ConnectionState.FINISHED
                        for (prev_id,_) in self._pending_polls:
                            self._connection.complete(id, {"events":[]} )
                else:
                    # Will deal with it later
                    self._pending_polls.append( (id,max_send) )
            else:
                assert False, "Invalid method in this state"
            return True
        else:
            # There was no message to process, so see if we can finish some
            return self._do_events_async_poll(events)

    def _do_events_FINISHED(self, events:DownwardConnectionEvents) -> bool:
        (method,id,params)=self._connection.try_begin()
        if method:
            self._connection._error(id, ProtocolError.ConnectionFinished, "Connection has finished due to halt.")
            return True
        


    def do_events(self, events:DownwardConnectionEvents) -> bool:
        """Handle any pending events on the connections."""
        try:
            if self._state==ConnectionState.CONNECTED:
                return self._do_events_CONNECTED(events)
            elif self._state==ConnectionState.BOUND:
                return self._do_events_BOUND(events)
            elif self._state==ConnectionState.RUN_REQUESTED:
                return self._do_events_RUN_REQUESTED(events)
            elif self._state==ConnectionState.RUNNING:
                return self._do_events_RUNNING(events)
            elif self._state==ConnectionState.HALT_REQUESTED:
                return self._do_events_HALT_REQUESTED(events)
            elif self._state==ConnectionState.FINISHED:
                return self._do_events_FINISHED(events)
            elif self._state==ConnectionState.ERRORED:
                return self._do_events_ERRORED(events)
            else:
                raise RuntimeError("Invalid state")
        except EOFError:
            return False

    def state(self) -> ConnectionState:
        return self._state

    def devices(self) -> Set[str] :
        return self._devices

import unittest

class TestStates(unittest.TestCase):
    ref_graph_type="gt0"
    ref_graph_instance="gi0"
    ref_incoming_edges={"int0:out":["ext0:in"]}

    class EventsDump(DownwardConnectionEvents):
        def __init__(self):
            self.received=[]
            self.to_send=[]

        def on_connect(self, connection, owner, owner_cookie, graph_type, graph_instance, owned_devices) :
            print(owned_devices)
            assert owned_devices==set(["ext0"])
            return ConnectResults(TestStates.ref_graph_type, TestStates.ref_graph_instance, TestStates.ref_incoming_edges)

        def on_run(self, connection) :
            return True

        def on_send(self, connection, messages:List[MulticastMessage]):
            self.received += messages

        def on_poll(self, connection, max_messages):
            if len(self.to_send)>0:
                todo=min(max_messages,len(self.to_send))
                res=self.to_send[:todo]
                self.to_send=self.to_send[todo:]
                return res
            else:
                return []

        def on_halt(self, connection, code, message):
            self.to_send.append(Halt(code,message))

    def test_connect(self):
        msg1=MulticastMessage("ext0:out")
        msg2=MulticastMessage("ext0:out", [10])
        msg3=MulticastMessage("ext0:out", [11])
        input=[
            {"jsonrpc":"2.0","id":"id0","method":"bind", "params":{ "owned_devices":["ext0"] }},
            {"jsonrpc":"2.0","id":"id1","method":"run"},
            {"jsonrpc":"2.0","id":"id2","method":"send", "params":{ "messages":[ msg1.toJSON() ]  }},
            {"jsonrpc":"2.0","id":"id3","method":"send", "params":{ "messages":[ msg2.toJSON(), msg3.toJSON() ]  }},
            {"jsonrpc":"2.0","id":"id4","method":"poll", "params":{ "max_messages":1} },
            {"jsonrpc":"2.0","id":"id5","method":"poll" },
            {"jsonrpc":"2.0","id":"id6","method":"poll" },
            {"jsonrpc":"2.0","id":"id7","method":"halt", "params":{"code":10} },
            {"jsonrpc":"2.0","id":"id8","method":"poll"},
            {"jsonrpc":"2.0","id":"id9","method":"poll"}
        ]
        channel=JSONRawChannelOnArrays(input)
        connection=JSONServerPull(channel)

        ev=TestStates.EventsDump()
        ev.to_send += [msg1,msg2,msg3]
        dc=DownwardConnection(connection)
        while dc.do_events(ev):
            pass
        
        self.assertEqual(len(channel.output), 10)

        bind_response=channel.output[0]
        print(bind_response)
        self.assertEqual(bind_response["result"]["graph_type"], self.ref_graph_type)
        self.assertEqual(bind_response["result"]["graph_instance"], self.ref_graph_instance)
        self.assertEqual(bind_response["result"]["incoming_edges"], self.ref_incoming_edges)
            
        run_response=channel.output[1]

        send_response=channel.output[2]
        self.assertEqual(ev.received[0],msg1)

        send_response=channel.output[3]
        self.assertEqual(ev.received[1],msg2)
        self.assertEqual(ev.received[2],msg3)

        poll_result=channel.output[4]["result"]
        self.assertEqual(len(poll_result["events"]),1)

        poll_result=channel.output[5]["result"]
        self.assertEqual(len(poll_result["events"]),2)

        poll_result=channel.output[6]["result"]
        self.assertEqual(len(poll_result["events"]),0)

        halt_result=channel.output[7]["result"]

        poll_result=channel.output[8]["result"]
        self.assertEqual(len(poll_result["events"]),1)
        self.assertTrue( contains_halt(json_objects_to_events(poll_result["events"]) ) )

        poll_error=channel.output[9]["error"]
        self.assertEqual(poll_error["code"], ProtocolError.ConnectionFinished )


class TestStates2(unittest.TestCase):
    ref_graph_type="gt0"
    ref_graph_instance="gi0"
    ref_incoming_edges={"int0:out":["ext0:in"]}

    class EventsDump(DownwardConnectionEvents):
        def __init__(self):
            self.received=[]
            self.to_send=[]
            self.enable_run=False

        def on_connect(self, connection, owner, owner_cookie, graph_type, graph_instance, owned_devices) :
            print(owned_devices)
            assert owned_devices==set(["ext0","ext1"])
            return ConnectResults(TestStates.ref_graph_type, TestStates.ref_graph_instance, TestStates.ref_incoming_edges)

        def on_run(self, connection) :
            return self.enable_run

        def on_send(self, connection, messages:List[MulticastMessage]):
            self.received += messages

        def on_poll(self, connection, max_messages):
            if len(self.to_send)>0:
                todo=min(max_messages,len(self.to_send))
                res=self.to_send[:todo]
                self.to_send=self.to_send[todo:]
                return res
            else:
                return []

        def on_halt(self, connection, code, message):
            self.to_send.append(Halt(code,message))

    def test_connect(self):
        msg1=MulticastMessage("ext0:out")
        msg2=MulticastMessage("ext0:out", [10])
        msg3=MulticastMessage("ext0:out", [11])

        (c2s,s2c)=multiprocessing.connection.Pipe()

        channel=JSONRawChannelOnPipe(s2c,s2c)
        connection=JSONServerPull(channel)

        ev=TestStates2.EventsDump()
        dc=DownwardConnection(connection)

        def run():
            while dc.do_events(ev):
                pass

        run()

        # Standard bind
        c2s.send({"jsonrpc":"2.0","id":"id0","method":"bind", "params":{ "owned_devices":["ext0","ext1"] }})
        run()
        bind_response=c2s.recv()
        print(bind_response)
        self.assertEqual(bind_response["result"]["graph_type"], self.ref_graph_type)
        self.assertEqual(bind_response["result"]["graph_instance"], self.ref_graph_instance)
        self.assertEqual(bind_response["result"]["incoming_edges"], self.ref_incoming_edges)

        # Run
        c2s.send({"jsonrpc":"2.0","id":"id1","method":"run"})
        run()
        self.assertFalse(c2s.poll())
        ev.enable_run=True
        run()
        self.assertTrue(c2s.poll())
        run_result=c2s.recv()["result"]

        # Send one message
        c2s.send({"jsonrpc":"2.0","id":"id2","method":"send", "params":{ "messages":[ msg1.toJSON() ]  }})
        run()
        self.assertTrue(c2s.poll())
        run_result=c2s.recv()["result"]
        self.assertEqual(ev.received[0], msg1)

        # Poll for messages, expect none
        c2s.send({"jsonrpc":"2.0","id":"id4","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 0)

        # Inject a message, poll for messages, expect one
        ev.to_send.append(msg2)
        c2s.send({"jsonrpc":"2.0","id":"id5","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 1)
        self.assertEqual( poll_result["events"][0], msg2.toJSON())

        # Poll for messages, expect none
        c2s.send({"jsonrpc":"2.0","id":"id10","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 0)

        # Poll for messages async, expect no return yet
        c2s.send({"jsonrpc":"2.0","id":"idAsync","method":"poll", "params":{ "async":True, "max_messages":1} })
        run()
        self.assertFalse(c2s.poll())

        # Overlap send with outstanding poll
        c2s.send({"jsonrpc":"2.0","id":"id20","method":"send", "params":{ "messages":[ msg3.toJSON() ]  }})
        run()
        self.assertTrue(c2s.poll())
        run_result=c2s.recv()["result"]
        self.assertEqual(ev.received[1], msg3)
        
        # Synchronous poll, expect response with zero messages
        c2s.send({"jsonrpc":"2.0","id":"id21","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 0)

        # Inject message, expect outstanding response to complete
        ev.to_send.append(msg2)
        run()
        self.assertTrue(c2s.poll())
        poll_response=c2s.recv()
        self.assertEqual(poll_response["id"], "idAsync")

        # Poll for messages async, expect no return yet
        c2s.send({"jsonrpc":"2.0","id":"idAsync2","method":"poll", "params":{ "async":True, "max_messages":1} })
        run()
        self.assertFalse(c2s.poll())

        # Queue up another async
        c2s.send({"jsonrpc":"2.0","id":"idAsync3","method":"poll", "params":{ "async":True, "max_messages":1} })
        run()
        self.assertFalse(c2s.poll())

        # Inject halt, expect halt to complete first
        c2s.send({"jsonrpc":"2.0","id":"id100","method":"halt", "params":{ "code":0} })
        run()
        halt_response=c2s.recv()
        self.assertEqual(halt_response["id"],"id100")

        # First async poll should complete with halt message
        self.assertTrue(c2s.poll())
        poll_response=c2s.recv()
        self.assertEqual(poll_response["id"], "idAsync2")
        self.assertEqual(len(poll_response["result"]["events"]), 1)
        self.assertEqual(poll_response["result"]["events"][0]["type"], "halt")
        self.assertEqual(poll_response["result"]["events"][0]["code"], 0)

        # Second async poll completes with zero messages
        self.assertTrue(c2s.poll())
        poll_response=c2s.recv()
        self.assertEqual(poll_response["id"], "idAsync3")
        self.assertEqual(len(poll_response["result"]["events"]), 0)

if __name__ == '__main__':
    unittest.main()