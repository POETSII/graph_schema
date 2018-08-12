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

assert n>=2

graphType=graphTypes["relaxation_heat"]
cellType=graphType.device_types["cell"]
mergerType=graphType.device_types["merger"]
rootType=graphType.device_types["root"]

instName="rheat_{}_{}".format(n,n)

properties={}

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        meta={"loc":[x,y],"hull":[ [x-0.5,y-0.5], [x+0.5,y-0.5], [x+0.5,y+0.5], [x-0.5,y+0.5] ]}
        boundary=0
        initial=0
        if x==0 and y==0:
            boundary=1
            initial=-127
        if x==n-1 and y==n-1:
            boundary=1
            initial=127
        colour=cell_colour(x,y)
        
        props={ "boundary":boundary, "initial":initial, "colour":colour, "neighbours":0 } # We will update neighbours later
        di=DeviceInstance(res,"c_{}_{}".format(x,y), cellType, props, meta)
        nodes[(x,y)]=di
        res.add_device_instance(di)
            
def add_channel(dx,dy,sx,sy):
    dst=nodes[ (dx,dy) ]
    src=nodes[ ( sx, sy ) ]
    ei=EdgeInstance(res,dst,"share_in", src,"share_out")
    res.add_edge_instance(ei)
    src.properties["neighbours"]+=1

for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        if x!=0:    add_channel(x-1,y,x,y)
        if y!=0:    add_channel(x,y-1,x,y)
        if x!=n-1:    add_channel(x+1,y,x,y)
        if y!=n-1:    add_channel(x,y+1,x,y)


############################################################
## Termination tree

to_merge=list(nodes.values())

depth=0
while len(to_merge)>1:
    to_merge_next=[]
    offset=0
    while len(to_merge)>0:
        children=to_merge[:2] # Grab the first two (or one)
        to_merge=to_merge[2:]
        merger=DeviceInstance(res,"m_{}_{}".format(depth,offset), mergerType, {"degree":len(children)})
        res.add_device_instance(merger)

        for i in range(len(children)):
            children[i].properties["termination_index"]=i # Give children a unique contiguous index
            ei=EdgeInstance(res,merger,"termination_in",children[i],"termination_out")
            res.add_edge_instance(ei)

        to_merge_next.append(merger)
        offset+=len(children)

    to_merge=to_merge_next
    depth+=1

root=DeviceInstance(res,"root", rootType, {"totalCells":len(nodes)})
res.add_device_instance(root)
res.add_edge_instance(EdgeInstance(res, root,"termination_in",to_merge[0],"termination_out"))

save_graph(res,sys.stdout)        
