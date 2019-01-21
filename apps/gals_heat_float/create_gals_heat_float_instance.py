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

src=appBase+"/gals_heat_float_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
maxTime=650000
exportDeltaMask=15
if len(sys.argv)>1:
    n=int(sys.argv[1])
if len(sys.argv)>2:
    maxTime=int(sys.argv[2])
if len(sys.argv)>3:
    exportDeltaMask=int(sys.argv[3])

assert math.log2(exportDeltaMask+1).is_integer()

assert n>=3

h=1.0/n
alpha=1.0

dt=h*h / (4*alpha) * 0.5


assert h*h/(4*alpha) >= dt

leakage=1.0

weightOther = dt*alpha/(h*h)
weightSelf = (1.0 - 4*weightOther)

weightOther = weightOther * leakage

graphType=graphTypes["gals_heat_float"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]
exitNodeType=graphType.device_types["exit_node"]

instName="heat_{}_{}".format(n,n)

properties={"exportDeltaMask":exportDeltaMask,"maxTime":maxTime}

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        meta={"loc":[x,y]}
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if x==n//2 and y==n//2:
            props={ "iv":1.0, "neighbours":4, "wSelf":weightSelf}
            di=DeviceInstance(res,"v_{}_{}".format(x,y), dirichletType, props, meta)
            nodes[(x,y)]=di
            res.add_device_instance(di)
        elif edgeX != edgeY:
            props={ "iv":1.0, "neighbours":1, "wSelf":weightSelf}
            di=DeviceInstance(res,"v_{}_{}".format(x,y), dirichletType, props, meta)
            nodes[(x,y)]=di
            res.add_device_instance(di)
        elif not (edgeX or edgeY):
            #props={ "iv":(urand()*2-1), "nhood":4, "wSelf":weightSelfFix }
            props={ "iv":(urand()*2-1), "nhood":4, "wSelf":weightSelf}
            di=DeviceInstance(res,"c_{}_{}".format(x,y), devType, props, meta)
            nodes[(x,y)]=di
            res.add_device_instance(di)
            
def add_channel(x,y,dx,dy):
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    if dst.device_type.id=="cell":
        props={"w":weightOther}
    else:
        props={"w":weightOther}  
    ei=EdgeInstance(res,dst,"in", src,"out", props)
    res.add_edge_instance(ei)

for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if edgeX and edgeY:
            continue

        if y!=0 and not edgeX:
            add_channel(x,y, 0, -1)
        if x!=n-1 and not edgeY:
            add_channel(x,y, +1, 0)
        if y!=n-1 and not edgeX:
            add_channel(x,y, 0, +1)
        if x!=0 and not edgeY:
            add_channel(x,y, -1, 0)        

finished=DeviceInstance(res, "finished",exitNodeType, {"fanin":len(nodes)}, None) 
res.add_device_instance(finished)

for (id,di) in nodes.items():
    res.add_edge_instance(EdgeInstance(res,finished,"done",di,"finished"))

save_graph(res,sys.stdout)        
