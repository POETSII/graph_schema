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
