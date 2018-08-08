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

src=appBase+"/relaxation_heat_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

colours=[
[0,1,3,2],
[0,2,3,1],
[3,2,0,1],
[3,1,0,2]
]

def cell_colour(x,y):
    return colours[x%4][y%4]

urand=random.random

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])

assert n>=3

graphType=graphTypes["relaxation_heat"]
cellType=graphType.device_types["cell"]

instName="rheat_{}_{}".format(n,n)

properties={}

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        meta={"loc":[x,y],"hull":[ [x-0.5,y-0.5], [x+0.5,y-0.5], [x+0.5,y+0.5], [x-0.5,y+0.5] ]}
        boundary=0
        initial=127
        if x==0 and y==0:
            boundary=1
            initial=0
        if x==n-1 and y==-1:
            boundary=1
            initial=255
        colour=cell_colour(x,y)
        
        props={ "boundary":boundary, "initial":initial, "colour":colour }
        di=DeviceInstance(res,"c_{}_{}".format(x,y), cellType, props, meta)
        nodes[(x,y)]=di
        res.add_device_instance(di)
            
def add_channel(x,y,dx,dy):
    dst=nodes[ (x,y) ]
    src=nodes[ ( x+dx, y+dy ) ]
    ei=EdgeInstance(res,dst,"share_in", src,"share_out")
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

save_graph(res,sys.stdout)        
