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

src=appBase+"/clocked_izhikevich_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

Ne=80
Ni=20
K=20

max_time=1000

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

graphType=graphTypes["clocked_izhikevich"]
neuronType=graphType.device_types["neuron"]
clockType=graphType.device_types["clock"]

sink=XmlV3StreamGraphBuilder(sys.stdout)
assert sink.can_interleave
sink.hint_total_devices(N+1)
sink.hint_total_edges(N*2 + N * K)

instName="sparse_{}_{}_{}".format(Ne,Ni,K)

properties={"max_t":max_time}
sink.begin_graph_instance(instName, graphType, properties=properties)

clock=sink.add_device_instance("clock", clockType, properties={"neuronCount":N})

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
        "a":a, "b":b, "c":c, "d":d, "Ir":Ir, "fanin":K, "seed":int(urand()*2**32)
    }
    nodes[i]=sink.add_device_instance(f"n_{i}", neuronType, properties=props)
    
    sink.add_edge_instance(nodes[i],"tick",clock,"tick")
    sink.add_edge_instance(clock,"tock",nodes[i],"tock")

sink.end_device_instances()

for dst in range(N):
    for src in random.sample(range(N), K):
        if src<Ne:
            S=0.5*urand()
        else:
            S=-urand()
        sink.add_edge_instance(nodes[dst], "input", nodes[src], "fire", properties={"weight":S} )

sink.end_graph_instance()