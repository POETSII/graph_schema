#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph

import sys
import os
import math
import random

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/gals_heat_protocol_only_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
maxTime=65
exportDeltaMask=15
if len(sys.argv)>1:
    n=int(sys.argv[1])
if len(sys.argv)>2:
    maxTime=int(sys.argv[2])


assert n>=2


graphType=graphTypes["gals_heat_protocol_only"]
devType=graphType.device_types["cell"]
exitNodeType=graphType.device_types["exit_node"]

instName="heat_protocol_{}_{}".format(n,n)

properties={"maxTime":maxTime}

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        edgex= x==0 or x==n-1
        edgey= y==0 or y==n-1
        meta={"loc":[x,y]}
        props={ "nhood":4 - (1 if edgex else 0) - (1 if edgey else 0) }
        di=DeviceInstance(res,"c_{}_{}".format(x,y), devType, props, None, meta)
        nodes[(x,y)]=di
        res.add_device_instance(di)
        
def add_channel(x,y,dx,dy):
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    props=None  
    ei=EdgeInstance(res,dst,"in", src,"out", props)
    res.add_edge_instance(ei)

for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1

        if y!=0:
            add_channel(x,y, 0, -1)
        if x!=n-1:
            add_channel(x,y, +1, 0)
        if y!=n-1:
            add_channel(x,y, 0, +1)
        if x!=0:
            add_channel(x,y, -1, 0)        

finished=DeviceInstance(res, "finished",exitNodeType, {"fanin":len(nodes)}, None) 
res.add_device_instance(finished)

for (id,di) in nodes.items():
    res.add_edge_instance(EdgeInstance(res,finished,"done",di,"finished"))

save_graph(res,sys.stdout)        
