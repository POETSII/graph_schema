#!/usr/bin/python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph
import sys
import os
import math
import random

urand=random.random

src=sys.argv[1]
(graphTypes,graphInstances)=load_graph_types_and_instances(src)

Ne=80
Ni=20

if len(sys.argv)>2:
    Ne=int(sys.argv[2])
if len(sys.argv)>3:
    Ni=int(sys.argv[3])

N=Ne+Ni

graphType=graphTypes["clocked_izhikevich"]
neuronType=graphType.device_types["neuron"]
clockType=graphType.device_types["clock"]

instName="fully_connected_{}_{}".format(Ne,Ni)

properties={}
res=GraphInstance(instName, graphType, properties)

clock=DeviceInstance(res, "clock", clockType, None, {"neuronCount":N})
res.add_device_instance(clock)

nodes=[None]*N
for i in range(N):
    if i<Ne:
        re=urand()
        a=0.02
        b=0.2
        c=-65+15*re*re
        d=8-6*re*re
    else:
        ri=urand()
        a=0.02+0.08*ri
        b=0.25-0.05*ri
        c=-65
        d=2
    props={
        "a":a, "b":b, "c":c, "d":d, "fanin":N
    }
    nodes[i]=DeviceInstance(res, "n_{}".format(i), neuronType, None, props)
    res.add_device_instance(nodes[i])

    res.add_edge_instance(EdgeInstance(res,nodes[i],"tick",clock,"tick",None))
    res.add_edge_instance(EdgeInstance(res,clock,"tock",nodes[i],"tock",None))

for dst in range(N):
    for src in range(N):
        if src<Ne:
            S=0.5*urand()
        else:
            S=-urand()
        ei=EdgeInstance(res, nodes[dst], "input", nodes[src], "fire", {"weight":S} )
        res.add_edge_instance(ei)


save_graph(res,sys.stdout)
