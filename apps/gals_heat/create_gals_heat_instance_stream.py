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

src=appBase+"/gals_heat_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])


max_time=65
if len(sys.argv)>2:
    max_time=int(sys.argv[2])

assert n>=3

h=1.0/n
alpha=1

dt=h*h / (4*alpha) * 0.5


assert h*h/(4*alpha) >= dt

leakage=1

weightOther = dt*alpha/(h*h)
weightSelf = (1.0 - 4*weightOther)

weightOther = weightOther * leakage

graphType=graphTypes["gals_heat"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]
exitNodeType=graphType.device_types["exit_node"]

instName="heat_{}_{}".format(n,n)

properties={"maxTime":max_time}

sink=make_xml_stream_builder(sys.stdout,require_interleave=True)
assert sink.can_interleave

sink.hint_total_devices(n*n-4+1)
sink.hint_total_edges((n-2)*n*2 + (n-2)*n*2 + (n-1) )

sink.begin_graph_instance(instName, graphType, properties=properties)

nodes={} # (x,y) -> str
boundaries=set() # Set of strings


for x in range(0,n):
    for y in range(0,n):
        meta={"loc":[x,y],"hull":[ [x-0.5,y-0.5], [x+0.5,y-0.5], [x+0.5,y+0.5], [x-0.5,y+0.5] ]}
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if x==n//2 and y==n//2:
            props={ "bias":0, "amplitude":1.0, "phase":1.5, "frequency": 100*dt, "neighbours":4 }
            id=sink.add_device_instance(f"v_{x}_{y}", dirichletType, properties=props, metadata=meta)
            boundaries.add(id)
            nodes[(x,y)]=id
        elif edgeX != edgeY:
            props={ "bias":0, "amplitude":1.0, "phase":1, "frequency": 70*dt*((x/float(n))+(y/float(n))), "neighbours":1 }
            id=sink.add_device_instance(f"v_{x}_{y}", dirichletType, properties=props, metadata=meta)
            boundaries.add(id)
            nodes[(x,y)]=id
        elif not (edgeX or edgeY):
            props={ "iv":urand()*2-1, "nhood":4, "wSelf":weightSelf }
            id=sink.add_device_instance(f"c_{x}_{y}", devType, properties=props, metadata=meta)
            nodes[(x,y)]=id
            
def add_channel(x,y,dx,dy):
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    if dst not in boundaries:
        props={"w":weightOther}
    else:
        props=None  
    sink.add_edge_instance(dst,"in", src,"out", properties=props)

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

finished=sink.add_device_instance("finished",exitNodeType, properties={"fanin":len(nodes)}) 

sink.end_device_instances()

for (id,di) in nodes.items():
    sink.add_edge_instance(finished,"done",di,"finished")

sink.end_graph_instance()