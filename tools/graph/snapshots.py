

class SnapshotReaderEvents(object):
    def __init__(self,graphTypes={},graphInstances={}):
        self.graphTypes=graphTypes
        self.graphInstances=graphInstances
        for di in graphInstances.values():
            if di.id not in self.graphTypes:
                self.graphTypes[di.id]=id
        self.currGraphInstance=None

    def _setGraphInstance(self,graphInstance):

    def getGraphType(self, id):
        return self.graphTypes[id]

    def getGraphInstance(self, id):
        self.currGraphInstance=self.graphInstances[id]
        return self.currGraphInstance

    def getEdgeType(self,id):
        return self.currGraphInstance.edges[id].edge_type

    def getDeviceType(self,id):
        return self.currGraphInstance.devices[id].edge_type

    def onStartSnapshot(self,graphType,graphInstanceid,orchTime,seqNum):
        raise NotImplementedError()

    def onEndSnapshot(self):
        raise NotImplementedError()

    def onDeviceInstance(self,devType,id,state,rts):
        raise NotImplementedError()

    def onEdgeInstance(self,edgeType,id,state,firings,messages):
        raise NotImplementedError()

class SnapshotReaderEventsDumper(SnapshotReaderEvents):
    def __init__(self,graphTypes,graphInstances):
        SnapshotReaderEvents.__init__(self)
        self.graphTypes=graphTypes
        self.graphInstances=graphInstances
        self.selGraphInstance=None

    def getGraphType(self, id):


    def getEdgeType(self,id):
        raise NotImplementedError()

    def getDeviceType(self,id):
        raise NotImplementedError()

    def onStartSnapshot(self,graphType,graphInstanceid,orchTime,seqNum):
        raise NotImplementedError()

    def onEndSnapshot(self):
        raise NotImplementedError()

    def onDeviceInstance(self,devType,id,state,rts):
        raise NotImplementedError()

    def onEdgeInstance(self,edgeType,id,state,firings,messages):
        raise NotImplementedError()

def parseSnapshot(src,handler):
    class SAXHandler(xml.sax.handler.ContentHandler):
        def __init__(self):
            xml.sax.handler.ContentHandler.__init__(self)
            self.stack=[]
            self.graphType=None

        def startElement(self,name,attrs):
            if name=="GraphSnapshot":
                assert len(self.stack)==0
                self.graphType=self.handler.getGraphType(attrs["graphTypeId"])
                graphInstId=attrs["graphInstId"]
                orchTime=attrs["orchestratorTime"]
                seqNum=attrs["sequenceNumber"]
                self.handler.onStartSnapshot(self.graphType, graphInstId, orchTime, seqNum)
            elif name=="DevS":
                assert self.stack[-1]=="GraphSnapshot"
                self.devState=None
                self.devId=attrs["id"]
            elif name=="EdgeS":
                assert self.stack[-1]=="GraphSnapshot"
                self.edgeState=None
                self.edgeId=attrs["id"]
                self.edgeMessages=[]
                self.edgeType=self.handler.getEdgeType(self.edgeid)
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
                message=self.parse_json(self.edgeType.message_type, self.text)
                self.edgeMessages.append(message)
                assert self.stack[-1]=="Q"
            elif name=="Q":
                assert self.stack[-1]=="EdgeS"
            elif name=="S":
                if self.stack[-1]=="DevS":
                    self.devState=self.parse_json(self.devType.state_type, self.text)
                elif self.stack[-1]=="EdgeS":
                    self.edgeState=self.parse_json(self.edgeType.state_type, self.text)
                else:
                    assert False
            elif name=="EdgeS":
                if self.devState is None and self.devType.state_type is not None:
                    self.devState=self.devType.state_type.create_default()
                self.handler.onDeviceInstance(self.devType,self.devId,self.devState,self.devRTS)
                self.devType=None
                self.devId=None
                assert self.stack[-1]=="GraphSnapshot"
            elif name=="DevS":
                if self.edgeState is None and self.edgeType.state_type is not None:
                    self.edgeState=self.edgeType.state_type.create_default()
                self.handler.onEdgeInstance(self.edgeType,self.edgeId,self.edgeFirings,self.edgeMessages)
                self.edgeType=None
                self.edgeId=None
            elif name=="GraphSnapshot":
                self.handler.onEndSnapshot()
                assert(len(self.stack)==0)
            else:
                assert False, "Unknown element type"

        def characters(self,text):
            self.text=self.text+text
