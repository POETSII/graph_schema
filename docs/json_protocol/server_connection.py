from typing import *
from io import BytesIO, StringIO
import json
import multiprocessing
from enum import Enum, unique

from json_rpc_helpers import *

from protocol import *


class ConnectionState(Enum):
    CONNECTED = 0
    BOUND = 1
    RUN_REQUESTED = 2      # Bind has been called, but not yet completed
    RUNNING = 3
    HALT_REQUESTED = 4     # halt has been called, but not yet delivered 
    FINISHED = 5           # halt has been delivered
    ERRORED = 6

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
            print(params)
            print("Owned devices: {}".format(params.get("owned_devices")))
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
        return False
        


    def do_events(self, events:DownwardConnectionEvents) -> bool:
        """Handle any pending events on the connections."""
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
        
    def state(self) -> ConnectionState:
        return self._state

    def devices(self) -> Set[str] :
        return self._devices
