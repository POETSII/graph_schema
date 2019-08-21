#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import XmlV3StreamGraphBuilder
import sys
import os
import math
import random


import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/gals_heat_fix_noedge_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
maxTime=65
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
alpha=1

dt=h*h / (4*alpha) * 0.5


assert h*h/(4*alpha) >= dt

_fix_max=2**31-1
_fix_min=-_fix_max
_fix_scale=2**24
def toFix(x):
    r=int(x*_fix_scale)
    assert _fix_min < r < _fix_max, "Fixed point overflow converting {}, r={}".format(x,r)
    return r

leakage=1

weightOther = dt*alpha/(h*h)
weightSelf = (1.0 - 4*weightOther)
weightSelfFix = toFix(weightSelf)

weightOther = weightOther * leakage
weightOtherFix = toFix(weightOther)

graphType=graphTypes["gals_heat_fix_noedge"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]
exitNodeType=graphType.device_types["exit_node"]

instName="heat_{}_{}".format(n,n)

properties={
    "maxTime":maxTime,
    "exportDeltaMask":exportDeltaMask,
    "wSelf":weightSelfFix,
    "wOther":weightOtherFix
}

sink=XmlV3StreamGraphBuilder(sys.stdout)
assert sink.can_interleave
sink.begin_graph_instance(instName, graphType, properties=properties)

nodes={}

for x in range(0,n):
    for y in range(0,n):
        meta={"loc":[x,y]}
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if x==n//2 and y==n//2:
            props={ "updateDelta":toFix(5.0*dt), "updateMax":toFix(5.0), "neighbours":4 }
            di=sink.add_device_instance(f"v_{x}_{y}", dirichletType, properties=props, metadata=meta)
            nodes[(x,y)]=di
        elif edgeX != edgeY:
            props={ "updateDelta":toFix(10.0*dt*(x/n)), "updateMax":toFix(1.0), "neighbours":1 }
            di=sink.add_device_instance(f"v_{x}_{y}", dirichletType, properties=props, metadata=meta)
            nodes[(x,y)]=di
        elif not (edgeX or edgeY):
            props={ "iv":toFix(urand()*2-1), "nhood":4}
            di=sink.add_device_instance(f"c_{x}_{y}", devType, properties=props, metadata=meta)
            nodes[(x,y)]=di

finished=sink.add_device_instance("finished",exitNodeType, properties={"fanin":len(nodes)}) 

sink.end_device_instances()
            
def add_channel(x,y,dx,dy):
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    sink.add_edge_instance(dst,"in", src,"out")

for x in range(0,n):
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

for (id,di) in nodes.items():
    sink.add_edge_instance(finished,"done",di,"finished")

sink.end_graph_instance()