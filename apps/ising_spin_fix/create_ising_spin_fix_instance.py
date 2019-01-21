#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph
import sys
import os
import math


import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/ising_spin_fix_graph_type.xml"
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

graphType=graphTypes["ising_spin_fix"]
devType=graphType.device_types["cell"]
finishedType=graphType.device_types["exit_node"]



instName="heat_{}_{}".format(n,n)

probs=[0]*10
for i in range(0,5):
    for j in range(0,2):
        index = i + 5*j; #'" index == 0,1,... ,9 "/
        my_spin = 2*j - 1;
        sum_nei = 2*i - 4;
        d_E = 2.*(J * my_spin * sum_nei + H * my_spin);
        x = math.exp(-d_E/T)
        p=x/(1.+x)
        probs[index]=math.floor(p * 2**32);
        sys.stderr.write("prob[{}] = {} = p\n".format(index,probs[index], p))

properties={
    "endTime":int(endTime * 2**20),
    "width":n,
    "height":n,
    "probabilities":probs
    }

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        devProps={"x":x, "y":y}
        di=DeviceInstance(res,"n_{}_{}".format(x,y), devType, devProps)
        nodes[(x,y)]=di
        res.add_device_instance(di)
        
def add_channel(x,y,dx,dy,dir):
    edgeProps={"direction":dir}
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    ei=EdgeInstance(res,dst,"in", src,"out", edgeProps)
    res.add_edge_instance(ei)


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
        
finished=DeviceInstance(res, "f", finishedType, {"fanin":len(nodes)})
res.add_device_instance(finished)

for (id,di) in nodes.items():
    ei=EdgeInstance(res,finished,"finished",di,"finished")
    res.add_edge_instance(ei)

save_graph(res,sys.stdout)        
