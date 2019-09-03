#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import XmlV3StreamGraphBuilder

import sys
import os
import math
import random

urand=random.random

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/pulsed_izhikevich_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

Ne=80
Ni=20
K=20

max_time=1000

fanout=4

if len(sys.argv)>1:
    Ne=int(sys.argv[1])
if len(sys.argv)>2:
    Ni=int(sys.argv[2])
if len(sys.argv)>3:
    K=int(sys.argv[3])
if len(sys.argv)>4:
    max_time=int(sys.argv[4])

N=Ne+Ni
K=min(N,K)

graphType=graphTypes["pulsed_izhikevich"]
neuronType=graphType.device_types["neuron"]
syncType=graphType.device_types["sync_node"]

sink=XmlV3StreamGraphBuilder(sys.stdout)
assert sink.can_interleave

instName="sparse_{}_{}_{}".format(Ne,Ni,K)

properties={"max_t":max_time}
sink.begin_graph_instance(instName, graphType, properties=properties)


neurons=[None]*N

for i in range(0,N):
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
    a=round(a*10000)/10000
    b=round(b*10000)/10000
    c=round(c*10000)/10000
    d=round(d*10000)/10000
    props={
        "a":a, "b":b, "c":c, "d":d, "Ir":Ir
    }
    state={
        "rng":int(urand()*2**32)
    }
    neurons[i]=sink.add_device_instance(f"n{i:x}", neuronType, properties=props, state=state)

assert len(neurons) > 1, "Must have more than one neuron"

todo=list(neurons)
random.shuffle(todo) # Decorrelate inhib from excit

level=0
while len(todo)>1:
    acc=[]
    while len(todo)>0:
        chunk=todo[-fanout:]
        todo=todo[:-fanout]

        degree=len(chunk)

        properties={"degree":len(chunk)}

        # Unless this is the root node, there is one connection down still to come
        if len(acc)>0 or len(todo)>0:
            properties["degree"]+=1
        else:
            properties["is_root"]=1

        sync=sink.add_device_instance(f"s_{level:x}_{len(acc):x}", syncType, properties=properties)
        acc.append(sync)

        for c in chunk:
            sink.add_edge_instance(c, "tick", sync, "tock")
            sink.add_edge_instance(sync, "tick", c, "tock")
    
    todo=acc
    sys.stderr.write(f"level={level}, len(todo)={len(todo)}\n")
    level+=1



sink.end_device_instances()

# Inter-neuron wiring
for dst in range(N):
    for src in random.sample(range(N), K):
        if src<Ne:
            S=0.5*urand()
        else:
            S=-urand()
        S=round(S*256)
        sink.add_edge_instance(neurons[dst], "in", neurons[src], "fire", properties={"w":S} )



sink.end_graph_instance()