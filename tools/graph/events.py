import xml.sax
import json
import sys

import xml.etree.ElementTree as ET
from lxml import etree


from typing import *

from graph.core import expand_typed_data

from graph.load_xml import *

class Event(object):
    def __init__(self, eventId:str, time:float, elapsed:float):
        self.eventId=eventId
        self.time=time
        self.elapsed=elapsed
        
class DeviceEvent(Event):
    def __init__(self,
        eventId:str, time:float, elapsed:float,
        dev:str, rts:int, seq:int, L:List[str], S:Optional[dict]
    ):
        Event.__init__(self, eventId, time, elapsed)
        self.dev=dev
        self.rts=rts
        self.seq=seq
        self.L=L
        self.S=S

class InitEvent(DeviceEvent):
    def __init__(self,
        eventId:str, time:float, elapsed:float,
        dev:str, rts:int, seq:int, L:List[str], S:Optional[dict]
    ):
        DeviceEvent.__init__(self,
            eventId,time,elapsed,
            dev, rts, seq, L, S
        )
        self.type="init"


class MessageEvent(DeviceEvent):
    def __init__(self,
        eventId:str, time:float, elapsed:float,
        dev:str, rts:int, seq:int, L:List[str], S:Optional[dict],
        pin:str
    ):
        DeviceEvent.__init__(self,
            eventId,time,elapsed,
            dev, rts, seq, L, S
        )
        self.pin=pin
        
class SendEvent(DeviceEvent):
    def __init__(self,
        eventId:str, time:float, elapsed:float,
        dev:str, rts:int, seq:int, L:List[str], S:Optional[dict],
        pin:str,
        cancel:bool, fanout:int, M:Optional[dict]
    ):
        MessageEvent.__init__(self,
            eventId,time,elapsed,
            dev, rts, seq, L, S,
            pin
        )
        self.cancel=cancel
        self.fanout=fanout
        self.M=M
        self.type="send"

class RecvEvent(DeviceEvent):
    def __init__(self,
        eventId:str, time:float, elapsed:float,
        dev:str, rts:int, seq:int, L:List[str], S:Optional[dict],
        pin:str,
        sendEventId:str
    ):
        MessageEvent.__init__(self,
            eventId,time,elapsed,
            dev, rts, seq, L, S,
            pin
        )
        self.sendEventId=sendEventId
        self.type="recv"

class LogWriter(object):
    def __init__(self):
        pass

    def onInitEvent(self,initEvent):
        raise NotImplementedError
    
    def onSendEvent(self,sendEvent):
        raise NotImplementedError
    
    def onRecvEvent(self,sendEvent):
        raise NotImplementedError

def extractInitEvent(n,writer):
    eventId=get_attrib(n,"eventId")
    time=float(get_attrib(n,"time"))
    elapsed=float(get_attrib(n,"elapsed"))
    dev=get_attrib(n,"dev")
    rts=int(get_attrib(n,"rts"),0)
    seq=int(get_attrib(n,"seq"))
    
    L=[]
    for l in n.findall("p:L",ns):
        L.append(l.text)
    
    S=n.find("p:S",ns)
    if S is not None and S.text is not None:
        #print(S.text)
        S=json.loads("{"+S.text+"}")
    
    e=InitEvent(
        eventId, time, elapsed,
        dev, rts, seq, L, S
    )
    writer.onInitEvent(e)
    
def extractSendEvent(n,writer):
    eventId=get_attrib(n,"eventId")
    time=float(get_attrib(n,"time"))
    elapsed=float(get_attrib(n,"elapsed"))
    dev=get_attrib(n,"dev")
    rts=int(get_attrib(n,"rts"),0)
    seq=int(get_attrib(n,"seq"))
    
    L=[]
    for l in n.findall("p:L",ns):
        L.append(l.text)
    
    S=n.find("p:S",ns)
    if S is not None and S.text is not None:
        S=json.loads("{"+S.text+"}")
        
    pin=get_attrib(n,"pin")
    cancel=bool(get_attrib(n,"cancel"))
    fanout=int(get_attrib(n,"fanout"))
    
    M=n.find("p:M",ns)
    if M is not None:
        M=json.loads("{"+M.text+"}")
    
    writer.onSendEvent( SendEvent(
        eventId, time, elapsed,
        dev, rts, seq, L, S,
        pin,
        cancel, fanout, M
    ) )

def extractRecvEvent(n,writer):
    eventId=get_attrib(n,"eventId")
    time=float(get_attrib(n,"time"))
    elapsed=float(get_attrib(n,"elapsed"))
    dev=get_attrib(n,"dev")
    rts=int(get_attrib(n,"rts"),0)
    seq=int(get_attrib(n,"seq"))
    
    L=[]
    for l in n.findall("p:L",ns):
        L.append(l.text)
    
    S=n.find("p:S",ns)
    if S is not None and S.text is not None:
        S=json.loads("{"+S.text+"}")
        
    pin=get_attrib(n,"pin")
    sendEventId=get_attrib(n,"sendEventId")
    
    writer.onRecvEvent( RecvEvent(
        eventId, time, elapsed,
        dev, rts, seq, L, S,
        pin,
        sendEventId
    ) )


def extractEvent(n,writer):
    if deNS(n.tag)=="p:InitEvent":
        extractInitEvent(n,writer)
    elif deNS(n.tag)=="p:SendEvent":
        extractSendEvent(n,writer)
    elif deNS(n.tag)=="p:RecvEvent":
        extractRecvEvent(n,writer)
    else:
        raise XMLSyntaxError("DIdn't understand node type.", n)

def parseEvents(src,writer):
    tree = etree.parse(src)
    doc = tree.getroot()
    eventsNode = doc;

    if deNS(eventsNode.tag)!="p:GraphLog":
        raise XMLSyntaxError("Expected GraphLog tag.", eventsNode)

    for e in eventsNode.findall("p:*",ns):
        extractEvent(e,writer)

        
