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

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/clocked_izhikevich_fix_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

Ne=80
Ni=20
K=20

if len(sys.argv)>1:
    Ne=int(sys.argv[1])
if len(sys.argv)>2:
    Ni=int(sys.argv[2])
if len(sys.argv)>3:
    K=int(sys.argv[3])

N=Ne+Ni
K=min(N,K)

graphType=graphTypes["clocked_izhikevich_fix"]
neuronType=graphType.device_types["neuron"]
clockType=graphType.device_types["clock"]

instName="sparse_{}_{}_{}".format(Ne,Ni,K)

properties={}
res=GraphInstance(instName, graphType, properties)

clock=DeviceInstance(res, "clock", clockType, {"neuronCount":N})
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

    res.add_edge_instance(EdgeInstance(res,nodes[i],"tick",clock,"tick",None))
    res.add_edge_instance(EdgeInstance(res,clock,"tock",nodes[i],"tock",None))

for dst in range(N):
    free=list(range(N))
    random.shuffle(free)
    for i in range(K):
        src=free[i]
        
        if src<Ne:
            S=0.5*urand()
        else:
            S=-urand()
        ei=EdgeInstance(res, nodes[dst], "input", nodes[src], "fire", {"weight":to_fix(S)} )
        res.add_edge_instance(ei)


save_graph(res,sys.stdout)
