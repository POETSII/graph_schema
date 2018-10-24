from typing import *
import json

from protocol import *

class ConnectionState(Enum):
    CONNECTED = 0
    BOUND = 1
    RUNNING = 3
    FINISHED = 5           # halt has been delivered
    ERRORED = 6

class UpwardConnection:
    def __init__(self, connection:JSONClientProxy):
        self._connection=connection
        self._state=ConnectionState.CONNECTED
        self._devices=set() # : Set[str]
        self._pending_polls=[] # type: List[ Pair[str,int] ]
        self._incoming_edges=None # type: Optional[Map[ Endpoint, List[Endpoint] ]]

    def bind(self, owned_devices:Sequence[str], owner="user", owner_cookie:Optional[str]=None, graph_type:str="*", graph_instance:str="*"):
        assert self._state==ConnectionState.CONNECTED
        try:
            results=self._connection.call("bind", {
                "owner":owner,
                "owner_cookie":owner_cookie,
                "graph_type":graph_type,
                "graph_instance":graph_instance,
                "owned_devices":list(owned_devices)
            })
            self._graph_type=results["graph_type"]
            self._graph_instance=results["graph_instance"]
            self._incoming_edges=results["incoming_edges"]
            self._incoming_edges={
                Endpoint(src) : [ Endpoint(dst) for dst in dests]
                for (src,dests) in self._incoming_edges.items()
            }
            self._state=ConnectionState.BOUND
        except:
            self._state=ConnectionState.ERRORED
            raise

    @property
    def incoming_edges(self):
        assert self._incoming_edges
        return self._incoming_edges

    def run(self):
        assert self._state==ConnectionState.BOUND
        try:
            results=self._connection.call("run")
            self._state=ConnectionState.RUNNING
        except:
            self._state=ConnectionState.ERRORED
            raise
    
    def poll(self, max_events:Optional[int]=None) -> Sequence[Event]:
        assert self._state==ConnectionState.RUNNING
        try:
            results=self._connection.call("poll", {"max_events":max_events})
            events=json_objects_to_events(results["events"])
            if contains_halt(events):
                self._state=ConnectionState.FINISHED
            return events
        except:
            self._state=ConnectionState.ERRORED
            raise

    def send(self, messages:Sequence[MulticastMessage]):
        assert self._state==ConnectionState.RUNNING
        try:
            self._connection.call("send", {"messages":events_to_json_objects(messages)})
        except:
            self._state=ConnectionState.ERRORED
            raise

    def halt(self, code:int, message:Optional[str]=None):
        assert self._state==ConnectionState.RUNNING
        try:
            self._connection.call("halt", {"code":code, "message":message})
        except:
            self._state=ConnectionState.ERRORED
            raise

    def close(self):
        self._connection.close()
        self._state=ConnectionState.ERRORED
