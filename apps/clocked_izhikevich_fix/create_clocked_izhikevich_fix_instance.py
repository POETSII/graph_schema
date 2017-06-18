#!/usr/bin/python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph
import sys
import os
import math
import random

urand=random.random

def to_fix(x):
    return int(x*65536)
    
def create_fanout(graph,dstDevInstances,dstPortName,dstEdgePropertiesFactory,expanderFactory,srcDevInstance,srcPortName,maxFanOut):
    """
    graph - graph in which fanout happens
    dstDevInstances - list of device instances to send too
    dstPortName - input port to send too
    dstEdgePropertiesFactory - Function from (dstDevInst) -> edgeProperties
    expanderFactory - function from (graph,name) -> devInst
    srcDevInstance - single source device instance
    srcPortName - output port things are coming from
    maxFanIn - Maximum acceptable fanout at any node
    """
    if len(dstDevInstances) <= maxFanOut:
        for d in dstDevInstances:
            props=dstEdgePropertiesFactory(d) if dstEdgePropertiesFactory else None
            ei=EdgeInstance(graph,d,dstPortName,srcDevInstance,srcPortName,props)
            graph.add_edge_instance(ei)
    else:
        baseName="{}_{}_exp".format(srcDevInstance.id,srcPortName)
        unq=0

        def make_name():
            nonlocal unq
            while True:
                name="{}_{}".format(baseName,str(unq))
                if name not in graph.device_instances:
                    return name
                unq=unq+random.randint(0,2**32-1)

        
        todo=list(dstDevInstances)
        done=[]
        while len(todo)>0:
            curr=todo[:maxFanOut]
            todo=todo[maxFanOut:]
            name=make_name()
            exp=expanderFactory(graph,name)
            graph.add_device_instance(exp)
            create_fanout(graph,curr,dstPortName,dstEdgePropertiesFactory,expanderFactory,exp,srcPortName,maxFanOut)
            done.append(exp)
        create_fanout(graph,done,dstPortName,None,expanderFactory,srcDevInstance,srcPortName,maxFanOut)
        
    
def create_fanin(graph,dstDevInst,dstPortName,dstFixup,reducerFactory,srcDevInstances,srcPortName,maxFanIn):
    """
    graph - graph in which reduction happens
    dstDevInst - isntance to reduce to
    dstPortName - input port name on the instance
    dstFixup - function from (dstDevInst,finalFanIn) -> None
    reducerFactory - function from (graph,name,inputCount) -> devInst
    srcDevInstances - array of source device instances
    srcPortName - output to wire up from source instances
    maxFanIn - Maximum acceptable fanin at any node
    """
    todo=list(srcDevInstances)
    
    baseName="{}_{}_red".format(dstDevInst.id,dstPortName)
    unq=0

    def make_name():
        nonlocal unq
        while True:
            name="{}_{}".format(baseName,str(unq))
            if name not in graph.device_instances:
                return name
            unq=unq+random.randint(0,2**32-1)
                
    while len(todo)>maxFanIn:
        done=[]
        while len(todo)>0:
            curr=todo[:maxFanIn]
            todo=todo[maxFanIn:]
            name=make_name()
            red=reducerFactory(graph,name,len(curr))
            assert isinstance(red,DeviceInstance)
            assert srcPortName in red.device_type.outputs
            assert dstPortName in red.device_type.inputs
            graph.add_device_instance(red)
            for s in curr:
                ei=EdgeInstance(graph,red,dstPortName,s,srcPortName)
                graph.add_edge_instance(ei)
            done.append(red)
        todo=done
    
    for s in todo:
        ei=EdgeInstance(graph,dstDevInst,dstPortName,s,srcPortName)
        graph.add_edge_instance(ei)
        
    dstFixup(dstDevInst,len(todo))

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/clocked_izhikevich_fix_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

Ne=80
Ni=20
K=20
maxTicks=10
maxFanIn=8
maxFanOut=4

if len(sys.argv)>1:
    Ne=int(sys.argv[1])
if len(sys.argv)>2:
    Ni=int(sys.argv[2])
if len(sys.argv)>3:
    K=int(sys.argv[3])
if len(sys.argv)>4:
    maxTicks=int(sys.argv[4])

N=Ne+Ni
K=min(N,K)

graphType=graphTypes["clocked_izhikevich_fix"]
neuronType=graphType.device_types["neuron"]
clockType=graphType.device_types["clock"]
clockReducerType=graphType.device_types["tick_fanin"]
spikeExpanderType=graphType.device_types["spike_fanout"]

instName="sparse_{}_{}_{}".format(Ne,Ni,K)


properties={"maxTicks":maxTicks}
res=GraphInstance(instName, graphType, properties)

clock=DeviceInstance(res, "clock", clockType, {"neuronCount":N,"fanin":0})
res.add_device_instance(clock)

nodes=[None]*N
for i in range(N):
    if i<Ne:
        re=urand()
        a=0.02
        b=0.2
        c=-65+15*re*re
        d=8-6*re*re
        Ir=5
    else:
        ri=urand()
        a=0.02+0.08*ri
        b=0.25-0.05*ri
        c=-65
        d=2
        Ir=2
    props={
        "a":to_fix(a), "b":to_fix(b), "c":to_fix(c), "d":to_fix(d), "Ir":to_fix(Ir), "fanin":K, "seed":int(urand()*2**32)
    }
    nodes[i]=DeviceInstance(res, "n_{}".format(i), neuronType, props)
    res.add_device_instance(nodes[i])

    res.add_edge_instance(EdgeInstance(res,nodes[i],"tick_in",clock,"tick_out",None))
    #res.add_edge_instance(EdgeInstance(res,clock,"tick_in",nodes[i],"tick_out",None))
    
def create_clock_reducer(graph,name,fanin):    
    return DeviceInstance(graph,name,clockReducerType,{"fanin":fanin})
    
def fixup_clock(dstDevInst,finalFanIn):
    dstDevInst.properties["fanin"]=finalFanIn
    
create_fanin(
    res,
    clock,"tick_in", fixup_clock, 
    create_clock_reducer,
    nodes,"tick_out",
    maxFanIn
)

srcIToDstI=[[] for i in range(N)]  # Map from src index to array of dest indexes

for dstI in range(N):
    free=list(range(N))
    random.shuffle(free)
    srcs=free[:K]
    
    for si in srcs:
        srcIToDstI[si].append(dstI)

for srcI in range(N):
    src=nodes[srcI]
    dsts=[nodes[dstI] for dstI in srcIToDstI[srcI]]
    
    if srcI<Ne:
        propsSrc=lambda node : {"weight":to_fix(0.5*urand())}
    else:
        propsSrc=lambda node : {"weight":to_fix(-urand())}
    
    create_fanout(res,
        dsts,"spike_in",propsSrc,
        lambda graph,name: DeviceInstance(graph,name,spikeExpanderType,None),
        src,"spike_out",
        maxFanOut
    )
    


#for dst in range(N):
#    free=list(range(N))
#    random.shuffle(free)
#    
#    for i in range(K):
#        src=free[i]
#        
#        if src<Ne:
#            S=0.5*urand()
#        else:
#            S=-urand()
#        ei=EdgeInstance(res, nodes[dst], "spike_in", nodes[src], "spike_out", {"weight":to_fix(S)} )
#        res.add_edge_instance(ei)


save_graph(res,sys.stdout)
