from enum import Enum

class Direction:
    CLIENT_TO_SERVER = 0      # upwards
    SERVER_TO_CLIENT = 1      # downwards

# This is pretty much just JSON without strings
PayloadType = Union[
    None,
    int,
    float,
    List["PayloadType"],
    Dict[str,"PayloadType"]
]

# Needs newer python
#class ExternalMessage(NamedTuple):
#    srcDev : str
#    srcPort : str
#    payload : PayloadType
#    dstDev : Union[str,None] = None
#    dstPort : Union[str,None] = None


class ExternalMessage:
    def __init__(self, srcDev:str, srcPort:str, payload:PayloadType, dstDev:Union[str,None], srcDev:Union[str,None]):
        self.srcDev=srcDev
        self.srcPort=srcPort
        self.payload=payload
        self.dstDev=dstDev
        self.dstPort=dstPort
    
    @property
    def is_multicast(self):
        return self.dstDev==None

    @property
    def is_unicast(self):
        return self.dstDev!=None

class ExternalChannel:
    def __init__(self,direction:Direction):
        self.direction=direction

    @property
    def is_writeable(self) -> bool:
        raise NotImplementedError

    def write(self, msg:ExternalMessage):
        raise NotImplementedError

    @property
    def is_readable(self) -> bool:
        raise NotImplementedError

    def read(self) -> ExternalMessage:
        raise NotImplementedError



class ClientRouter:
    class RoutingTable:
        def __init__(self,srcDev,srcPort):
            self.srcDev=srcDev
            self.srcPort=srcPort
            self.local_fanout=set()
            self.has_non_local=False
    
    def _get_route(self,srcDev,srcPort):
        key=(srcDev,srcPort)
        route=self.routes.get(key)
        if not route:
            route=RoutingTable(srcDev,srcPort)
            routes[key]=route
        return route

    def __init__(self):
        self.local_devices=set()
        self.routes={}

    def add_local_device(self,devId):
        self.locals.add(devId)

    def add_edge(self,dstDev,dstPort,srcDev,srcPort):
        dstIsLocal=dstDev in self.local_devices
        srcIsLocal=srcDev in self.local_devices
        if not (dstIsLocal or srcIsLocal):
            return
        route=self._get_route(srcDev,srcPort)
        if dstIsLocal:
            route.local_fanout.insert( (dstDev,dstPort) )
        else:
            route.has_non_local=True

    def route_multicast(self, srcDev,srcPort, msg, deliverLocal, sendToServer ):
        route=self.routes[ (srcDev,srcPort) ]
        for (dstDev,dstPort) in route.local_fanout:
            deliverLocal( dstDev,dstPort, msg )
        if route.has_non_local:
            sendToServer( msg )

    def route_unicast(self, dstDev,dstPort, msg, deliverLocal, sendToServer ):
        if dstDev in self.local_devices:
            deliverLocal(dstDev,dstPort,msg)
        else:
            sendToServer(msg)

class ServerRouter:
    class RoutingTable:
        def __init__(self,srcDev,srcPort):
            self.srcDev=srcDev
            self.srcPort=srcPort
            self.local_fanout=set()
            self.non_local_fanout={} # (int,int) -> set(clientId)

    def _get_route(self,srcDev,srcPort):
        key=(srcDev,srcPort)
        route=self.routes.get(key)
        if not route:
            route=RoutingTable(srcDev,srcPort)
            routes[key]=route
        return route
        

    def __init__(self):
        self.clients={}
        self.device_to_client={}
        self.routes={}

    def add_local_device(self,devId):
        self.device_to_external[devId]=None

    def add_non_local_device(self,clientId,devId):
        assert clientId!=None
        self.device_to_client[devId]=clientId
    
    def add_edge(self,dstDev,dstPort,srcDev,srcPort):
        route=self._get_route(srcDev,srcPort)
        dstLoc=self.device_to_client[dstDev]
        srcLoc=self.device_to_client[srcDev]
        if dstLoc==None:
            route.local_fanout.insert( (dstDev,dstPort) )
        elif dstLoc==srcLoc:
            # Do nothing, it will be handled internally
        else:
            route.non_local_fanout.add( dstLoc )

    def route_multicast(self, srcDev,srcPort, msg, deliverLocal, sendToClient ):
        key=(srcDev,srcPort)
        route=self.routes[ key ]
        for (dstDev,dstPort) in route.local_fanout:
            deliverLocal( dstDev,dstPort, msg )
        for clientId in route.non_local_fanout[ key ]:
            sendToClient( clientId, msg )

    def route_unicast(self, dstDev,dstPort, msg, deliverLocal, sendToServer ):
        clientId=self.device_to_client[dstDev]
        if clientId==None:
            deliverLocal(dstDev,dstPort,msg)
        else:
            sendToClient(clientId, msg)
