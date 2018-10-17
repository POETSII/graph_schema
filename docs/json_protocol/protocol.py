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

    @property
    def full(self):
        return self._full

    def __eq__(self,o):
        return self._full==o._full

    def __ne__(self,o):
        return self._full!=o._full
    
    def __str__(self):
        return self._full

    def __hash__(self):
        return hash(self._full)


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

ConnectResults=NamedTuple('ConnectResults',[
    ('graph_type',str),
    ('graph_instance',str),
    ('incoming_edges', Dict[Endpoint,List[Endpoint]])
])
