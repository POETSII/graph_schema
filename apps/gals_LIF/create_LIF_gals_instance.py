#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph
import sys
import os
import math
import random

urand=random.random

src=sys.argv[1]
(graphTypes,graphInstances)=load_graph_types_and_instances(src, "<stdin>")

Ne=80
Ni=20
K=20
max_t=1000

if len(sys.argv)>2:
    Ne=int(sys.argv[2])
if len(sys.argv)>3:
    Ni=int(sys.argv[3])
if len(sys.argv)>4:
    K=int(sys.argv[4])
if len(sys.argv)>5:
    max_t=int(sys.argv[5])

N=Ne+Ni
K=min(N,K)

graphType=graphTypes["gals_LIF"]
neuronType=graphType.device_types["neuron"]

instName="sparse_{}_{}_{}".format(Ne,Ni,K)

properties={"max_t":max_t}
res=GraphInstance(instName, graphType, properties)

#Adding model of LIF (leaky integrated and fire) neuron:

R = 1 # resistance (kOhm)
Cm = 10 # capacitance (uF)
tau_m = R*Cm # time constant (msec)

nodes=[None]*N
for i in range(N):
    if i<Ne:
        re=urand()
        Ir=5*re
        r_tau_m=1/tau_m
        U_rest= -65
    else:
        ri=urand()
        Ir=2*ri
        r_tau_m=1/tau_m
        U_rest= -65
    props={
         "r_tau_m":r_tau_m, "R":R, "U_rest":U_rest, "Ir":Ir, "fanin":K
    }
    nodes[i]=DeviceInstance(res, "n_{}".format(i), neuronType, props)
    res.add_device_instance(nodes[i])

for dst in range(N):
    free=list(range(N))
    random.shuffle(free)
    for i in range(K):
        src=free[i]

        if src<Ne:
            S=0.5*urand()
        else:
            S=-urand()
        ei=EdgeInstance(res, nodes[dst], "input", nodes[src], "fire", {"weight":S} )
        res.add_edge_instance(ei)


save_graph(res,sys.stdout)
