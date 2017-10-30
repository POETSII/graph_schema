from graph.load_xml import load_graph_types_and_instances

from graph.core import *
from graph.events import *
from graph.load_xml import *

import xml.etree.ElementTree as ET
from lxml import etree

import os
import sys
import json
import collections
from typing import *

ns={"p":"https://poets-project.org/schemas/virtual-graph-schema-v2"}

_cp="{{{}}}CP".format(ns["p"])

class CheckpointLog(object):
    def __init__(self,dst):
        self.dst=dst
        self.dst.write("<?xml version='1.0'?>\n")
        self.dst.write('<Graphs xmlns="{}">\n'.format(ns["p"]))
        self.dst.write(' <Checkpoints>\n')
        
    def close(self):
        if self.dst:
            self.dst.write(" </Checkpoints>\n")
            self.dst.write("</Graphs>\n")
            self.dst.close()
            self.dst=None
    
    def add(self,deviceId,key,state):
        ss=json.dumps(state)
        if len(ss)!=0:
            ss=ss[1:-1]
        if ss.find("<")!=-1 or ss.find(">")!=-1:
            self.dst.write('  <CP dev="{}" key="{}"><![CDATA[{}]]></CP>\n'.format(deviceId,key,ss))
        else:
            self.dst.write('  <CP dev="{}" key="{}">{}</CP>\n'.format(deviceId,key,ss))

_jsonType=Union[float,int,Dict[str,Any],List[Any]]

def compare_checkpoint(name:str,ref:_jsonType,got:_jsonType) -> List[str]:
    res=[]
    
    if isinstance(ref,int):
        if not isinstance(got,int):
                res.append("member {}, got value is not an nt.".format(name))
        elif ref!=got:
            res.append("integer member '{}', ref='{}', got='{}'".format(name,ref,got))
    elif isinstance(ref,float):
        if isinstance(got,int):
            got=float(got) # Fix-up any values written without decimal
        if not isinstance(got,float):
                res.append("member {}, got value is not a float.".format(name))
        elif abs(got-ref) > 1e-6:
            res.append("float member '{}', ref='{}', got='{}', err='{}'".format(name,ref,got,ref-got))
    elif isinstance(ref,dict):        
        for (k,rv) in ref.items():
            if not isinstance(got,dict):
                res.append("member {}, got value is not a dict.".format(k))
            elif k not in got:
                res.append("member {}/{} is missing from state.".format(name,k))
            else:
                gv=got[k]
                res.extend(compare_checkpoint(name+"/"+k,rv,gv))    
    elif isinstance(ref,list):
        if not isinstance(got,list):
            res.append("array member '{}', got value is not a list".format(k))
        elif len(ref)!=len(got):
            res.append("array member '{}', lengths are not equal, len(ref)='{}', len(got)='{}'".format(k,len(ref),len(got)))
        else:
            for (i,rvi) in enumerate(ref):
                gvi=got[i]
                res.extend(compare_checkpoint("{}[{}]".format(name,i),rvi,gvi))
    else:
        assert "Unexpected reference value type"
        
    return res


def apply_checkpoints(graphInstPath:str,checkpointPath:str,eventLogPath:str,maxErrors:int=10):
    # Load the graph instance
    
    (types,instances)=load_graph_types_and_instances(graphInstPath, graphInstPath)
    
    if len(instances)!=1:
        raise "Not exactly one instance."
    for i in instances.values():
        graphInst=i
        
    # Load the checkpoints
    
    checkpointTree = etree.parse(checkpointPath)
    checkpointDoc = checkpointTree.getroot()
    checkpointGraphsNode = checkpointDoc;
    
    checkpoints={} # devId-> { key -> state }
    for cpsNode in checkpointGraphsNode.findall("p:Checkpoints",ns):
        sys.stderr.write("Loading checkpoints\n")
        for cpNode in cpsNode:
            if cpNode.tag!=_cp:
                raise "Unknown node type in checkpoint file '{}'".format(cpNode.tag)
            devId=get_attrib(cpNode,"dev")
            key=get_attrib(cpNode,"key")
            stateText=cpNode.text
            state=json.loads("{"+stateText+"}")
            
            checkpoints.setdefault(devId,{})[key]=state
    
    
    # Walk through the events
    sys.stderr.write("Walking events\n")
    
    states={}
    for di in graphInst.device_instances.values():
        states[di.id]=create_default_typed_data(di.device_type.state)
        
    # Track the messages in flight, as if there is a global checkpoint we
    # need to ensure that the event log is in a causal order.
    # Map of message_id -> unreceived_count
    # If received_count is negative, then a message has been received, but
    # has not yet been sent, so we are acausal.
    # If recieved_count is positive, then there are messages which are sent but not yet delivered.
    # If 0, then they have matched up, and the entry should be deleted.
    in_flight=collections.Counter()
    
    # Number of messages with a negative receive count
    # Whenever it is positive, we are acausal
    acausal_count=0
    
    class LogSink(LogWriter):
        def __init__(self):
            self.gotErrors=0
            
        def _doCheck(self, dev, key, got):
            if dev not in checkpoints:
                sys.stderr.write("{}, {} : No reference checkpoints found for checkpointed event.\n".format(e.dev,key))
                self.gotErrors+=1
                return
                
            cps=checkpoints[dev]            
            if key not in cps:
                sys.stderr.write("{}, {} : No reference checkpoint event found.\n".format(dev,key))
                return
            ref=cps[key]
            
            errors=compare_checkpoint("",ref,got)
            if len(errors)>0:
                for err in errors:
                    sys.stderr.write("{}, {} : {}\n".format(dev,key,err))
                sys.stderr.write("ref = {}\n".format(json.dumps(ref,indent="  ")))
                sys.stderr.write("got = {}\n".format(json.dumps(got,indent="  ")))
                self.gotErrors+=1
        
        def checkEvent(self,e):
            
            #sys.stderr.write("{}\n".format(e.dev))
            preState=states[e.dev]
            postState=e.S
            states[e.dev]=postState
            
            for (pre,key) in e.tags:
                is_global=key.startswith("global:")
                if is_global:
                    key=key[len("global:"):]
                    
                    if acausal_count > 0:
                        raise RuntimeError("Attempt to do global checkpoint when event log is acausal (receive before send)."
                
                #Always need to check this device
                got=preState if pre else postState
                self._doCheck(e.dev, key, got)
                
                if is_global:
                    # And only sometimes check everything. If it is a global checkpoint then
                    # we don't worry about pre/post
                    for (dev,state) in states.items():
                        if dev==e.dev:
                            continue
                        self._doCheck(dev, key, state)
                    
            if(self.gotErrors >= maxErrors):
                sys.stderr.write("More than {} errors. Quitting.".format(maxErrors))
                sys.exit(1)

        def onInitEvent(self,e):
            self.checkEvent(e)
            
        def onSendEvent(self,e):
            count=in_flight[e.eventId]
            if count < 0:
                acausal_count-=1
            count+=e.fanout
            assert count>=0
            if count==0:
                del in_flight[e.eventId]
            else:
                in_flight[e.eventId]
                
            self.checkEvent(e)
            
        def onRecvEvent(self,e):
            count=in_flight[e.sendEventId]
            if count==0:
                acausal_count+=1
            count -= 1
            if count==0:
                del in_flight[e.sendEventId]
            else:
                in_flight[e.sendEventId]=count
            
            self.checkEvent(e)

    sink=LogSink()

    parseEvents(eventLogPath,sink)
