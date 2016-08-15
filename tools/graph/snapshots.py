import xml.sax
import json
import sys

from graph.core import expand_typed_data

class SnapshotReaderEvents(object):
    def __init__(self):
        pass

    def onStartSnapshot(self,graphInstanceId,orchTime,seqNum):
        raise NotImplementedError()

    def onEndSnapshot(self):
        raise NotImplementedError()

    def onDeviceInstance(self,id,state,rts):
        raise NotImplementedError()

    def onEdgeInstance(self,id,state,firings,messages):
        raise NotImplementedError()

def parseSnapshot(src,handler):
    def parse_json(text):
        if text is None or text=="":
            None
        j= json.loads("{"+text+"}")
        return j
    
    class SAXHandler(xml.sax.handler.ContentHandler):
        def __init__(self,handler):
            xml.sax.handler.ContentHandler.__init__(self)
            self.stack=[]
            self.graphType=None
            self.handler=handler
            self.text=""

        def startElement(self,name,attrs):
            if name=="Graph":
                assert len(self.stack)==0
            elif name=="GraphSnapshot":
                assert len(self.stack)==1
                graphTypeId=attrs["graphTypeId"]
                graphInstId=attrs["graphInstId"]
                orchTime=attrs["orchestratorTime"]
                seqNum=attrs["sequenceNumber"]
                self.handler.onStartSnapshot(graphTypeId,graphInstId, orchTime, seqNum)
            elif name=="DevS":
                assert self.stack[-1]=="GraphSnapshot"
                self.devState=None
                self.devId=attrs["id"]
                if "rts" in attrs:
                    self.devRts=int(attrs["rts"])
                else:
                    self.devRts=0
            elif name=="EdgeS":
                assert self.stack[-1]=="GraphSnapshot"
                self.edgeState=None
                self.edgeId=attrs["id"]
                if "firings" in attrs:
                    self.edgeFirings=int(attrs["firings"],16)
                else:
                    self.edgeFirings=0
                self.edgeMessages=[]
                self.edgeState=None
            elif name=="S":
                assert self.stack[-1]=="EdgeS" or self.stack[-1]=="DevS"
                self.text=""
            elif name=="Q":
                assert self.stack[-1]=="EdgeS"
            elif name=="M":
                assert self.stack[-1]=="Q"
            else:
                assert False, "Unexpected element type '"+name+"'"
            self.stack.append(name)

        def endElement(self,name):
            assert self.stack[-1]==name
            self.stack.pop()
            if name=="M":
                message=parse_json(self.text)
                self.edgeMessages.append(message)
                assert self.stack[-1]=="Q"
            elif name=="Q":
                assert self.stack[-1]=="EdgeS"
            elif name=="S":
                if self.stack[-1]=="DevS":
                    self.devState=parse_json(self.text)
                elif self.stack[-1]=="EdgeS":
                    self.edgeState=parse_json(self.text)
                else:
                    assert False
            elif name=="DevS":
                self.handler.onDeviceInstance(self.devId,self.devState,self.devRts)
                self.devType=None
                self.devId=None
                assert self.stack[-1]=="GraphSnapshot"
            elif name=="EdgeS":
                self.handler.onEdgeInstance(self.edgeId,self.edgeState,self.edgeFirings,self.edgeMessages)
                self.edgeType=None
                self.edgeId=None
            elif name=="GraphSnapshot":
                self.handler.onEndSnapshot()
                assert(len(self.stack)==1)
            elif name=="Graph":
                assert(len(self.stack)==0)
            else:
                assert False, "Unknown element type"
            self.text=""

        def characters(self,text):
            self.text=self.text+text

    parser = xml.sax.make_parser()
    saxHandler = SAXHandler(handler)
    parser.setContentHandler(saxHandler)
    parser.parse(src)

def extractSnapshotInstances(graphInstances,src,sink):
    class SnapshotReaderEventsUpdate(SnapshotReaderEvents):
        def __init__(self,graphInstances,sink):
            SnapshotReaderEvents.__init__(self)
            self.graphInstances=graphInstances
            self.selGraphInstance=None
            self.sink=sink

        def onStartSnapshot(self,graphType,graphInstanceId,orchTime,seqNum):
            
            self.selGraphInstance=self.graphInstances[graphInstanceId]
            self.selGraphType=self.selGraphInstance.graph_type

            gi=self.selGraphInstance
            self.orchTime=orchTime
            self.seqNum=seqNum
            self.deviceStates={ id : (None,0) for id in gi.device_instances.keys() } # Tuples of (state,rts)
            self.edgeStates={ id : (None,0,[]) for id in gi.edge_instances.keys() } # tuples of (state,firings,messages)

        def onEndSnapshot(self):
            gi=self.selGraphInstance
            for di in gi.device_instances.values():
                dt=gi.device_instances[di.id].device_type
                val=self.deviceStates[di.id]
                if dt.state and None==val[0]:
                    val=(dt.state.create_default(),val[1])
                    self.deviceStates[di.id]=val

            for ei in gi.edge_instances.values():
                et=gi.edge_instances[ei.id].edge_type
                val=self.edgeStates[ei.id]
                if et.state and not val[0]:
                    assert(val[1]==0 and val[2]==[]) # Must not have seen firings or messages if we didn't get state
                    val=(et.state.create_default(),val[1],val[2])
                    self.edgeStates[ei.id]=val

            self.sink(self.selGraphType, self.selGraphInstance, self.orchTime, self.seqNum, self.deviceStates, self.edgeStates)

            self.selGraphType=None
            self.selGraphInstance=None
            self.deviceStates=None
            self.edgeStates=None
        

        def onDeviceInstance(self,id,state,rts):
            devType=self.selGraphInstance.device_instances[id].device_type
            expanded=expand_typed_data(devType.state,state)
            self.deviceStates[id]=(expanded,rts)

        def onEdgeInstance(self,id,state,firings,messages):
            edgeType=self.selGraphInstance.edge_instances[id].edge_type
            messages=[expand_typed_data(edgeType.message_type, msg) for msg in messages]
            self.edgeStates[id]=(expand_typed_data(edgeType.state,state),firings,messages)

    events=SnapshotReaderEventsUpdate(graphInstances,sink)
    parseSnapshot(src,events)
