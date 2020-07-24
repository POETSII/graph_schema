#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import make_xml_stream_builder
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

dt=h*h / (16*alpha) * 0.5


assert h*h/(16*alpha) >= dt

_fix_max=2**31-1
_fix_min=-_fix_max
_fix_scale=2**24
def toFix(x):
    r=int(x*_fix_scale)
    assert _fix_min < r < _fix_max, "Fixed point overflow converting {}, r={}".format(x,r)
    return r

leakage=1

weightOther = dt*alpha/(h*h)
weightSelf = (1.0 - 6*weightOther)
weightSelfFix = toFix(weightSelf)

weightOther = weightOther * leakage
weightOtherFix = toFix(weightOther)

graphType=graphTypes["gals_heat_fix_noedge"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]
exitNodeType=graphType.device_types["exit_node"]

instName="heat_{}_{}_{}".format(n,n,n)

properties={
    "maxTime":maxTime,
    "exportDeltaMask":exportDeltaMask,
    "wSelf":weightSelfFix,
    "wOther":weightOtherFix
}

sink=make_xml_stream_builder(sys.stdout, require_interleave=True)
assert sink.can_interleave
sink.begin_graph_instance(instName, graphType, properties=properties)

nodes={}

for x in range(0,n):
    for y in range(0,n):
        for z in range(0,n):
            edgeX = x==0 or x==n-1
            edgeY = y==0 or y==n-1
            edgeZ = z==0 or z==n-1
            if x==n//2 and y==n//2 and z==n//2:
                props={ "updateDelta":toFix(5.0*dt), "updateMax":toFix(5.0), "neighbours":6 }
                di=sink.add_device_instance(f"v_{x}_{y}_{z}", dirichletType, properties=props)
                nodes[(x,y,z)]=di
            elif not (edgeX or edgeY or edgeZ):
                props={ "iv":toFix(urand()*2-1), "nhood":6}
                di=sink.add_device_instance(f"c_{x}_{y}_{z}", devType, properties=props)
                nodes[(x,y,z)]=di
            elif not ( (edgeX and edgeY) or (edgeX and edgeZ) or (edgeY and edgeZ) ):
                props={ "updateDelta":toFix(10.0*dt*(x/n)), "updateMax":toFix(1.0), "neighbours":1 }
                di=sink.add_device_instance(f"v_{x}_{y}_{z}", dirichletType, properties=props)
                nodes[(x,y,z)]=di

finished=sink.add_device_instance("finished",exitNodeType, properties={"fanin":len(nodes)}) 

sink.end_device_instances()
            
def add_channel(x,y,z,dx,dy,dz):
    dst=nodes[ (x,y,z) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n, (z+dz+n)%n ) ]
    sink.add_edge_instance(dst,"in", src,"out")

for x in range(0,n):
    edgeX = x==0 or x==n-1
    for y in range(0,n):
        edgeY = y==0 or y==n-1
        if edgeX and edgeY:
            continue
        for z in range(0,n):
            edgeZ = z==0 or z==n-1
            
            if (edgeX and edgeZ) or (edgeY and edgeZ):
                continue

            if not (edgeX or edgeY):
                if z!=0:
                    add_channel(x,y,z, 0, 0, -1)
                if z!=n-1:
                    add_channel(x,y,z, 0, 0, +1)

            if not (edgeX or edgeZ):
                if y!=0:
                    add_channel(x,y,z, 0, -1, 0)
                if y!=n-1:
                    add_channel(x,y,z, 0, +1, 0)

            if not (edgeY or edgeZ):
                if x!=0:
                    add_channel(x,y,z, -1, 0, 0)
                if x!=n-1:
                    add_channel(x,y,z, +1, 0, 0)


for (id,di) in nodes.items():
    sink.add_edge_instance(finished,"done",di,"finished")

sink.end_graph_instance()