#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import make_xml_stream_builder

import sys
import os
import math


import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/ising_spin_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

endTime=100
T=1
J=1
H=0

n=4
if len(sys.argv)>1:
    n=int(sys.argv[1])
if len(sys.argv)>2:
    T=float(sys.argv[2])
if len(sys.argv)>3:
    endTime=float(sys.argv[3])
    

graphType=graphTypes["ising_spin"]
devType=graphType.device_types["cell"]



instName="heat_{}_{}".format(n,n)

probs=[0]*10
for i in range(0,5):
    for j in range(0,2):
        index = i + 5*j; #'" index == 0,1,... ,9 "/
        my_spin = 2*j - 1;
        sum_nei = 2*i - 4;
        d_E = 2.*(J * my_spin * sum_nei + H * my_spin);
        x = math.exp(-d_E/T)
        probs[index]=x/(1.+x);
        sys.stderr.write("prob[{}] = {}\n".format(index,probs[index]))

properties={
    "endTime":endTime,
    "width":n,
    "height":n,
    "probabilities":probs
    }

sink=make_xml_stream_builder(sys.stdout,require_interleave=True)
assert sink.can_interleave

sink.begin_graph_instance(instName, graphType, properties=properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        devProps={"x":x, "y":y}
        di=sink.add_device_instance(f"n_{x}_{y}", devType, properties=devProps)
        nodes[(x,y)]=di

sink.end_device_instances()
        
def add_channel(x,y,dx,dy,dir):
    edgeProps={"direction":dir}
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    sink.add_edge_instance(dst,"in", src,"out", properties=edgeProps)


for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        centre=nodes[(x,y)]
        add_channel(x,y, 0, -1, 1)
        add_channel(x,y, +1, 0, 2)
        if n>2:
            add_channel(x,y, 0, +1, 3)
        if n>2:
            add_channel(x,y, -1, 0, 4)        
        
sink.end_graph_instance()       
