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

src=appBase+"/relaxation_heat_noterm_graph_type.xml"
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

max_steps=10

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])

assert n>=2

graphType=graphTypes["relaxation_heat_noterm"]
cellType=graphType.device_types["cell"]
mergerType=graphType.device_types["merger"]
rootType=graphType.device_types["root"]
fanoutType=graphType.device_types["fanout"]

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
        colour=cell_colour(x,y)
        
        props={ "initial_boundary":boundary, "initial_heat":initial, "colour":colour, "neighbours":0, "x":x, "y":y } # We will update neighbours later
        di=DeviceInstance(res,"c_{}_{}".format(x,y), cellType, props, None, meta)
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

for d in nodes.values():
    d.properties["multiplier"]=int( 2**16 / (d.properties["neighbours"]) )


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

root=DeviceInstance(res,"root", rootType, {"totalCells":len(nodes), "width":n, "height":n, "max_steps":max_steps})
res.add_device_instance(root)
res.add_edge_instance(EdgeInstance(res, root,"termination_in",to_merge[0],"termination_out"))

# TODO : use a fanout tree
if False:
    for d in nodes.values():
        res.add_edge_instance(EdgeInstance(res, d,"modification_in",root,"modification_out"))
else:
    def recurse_fanout(todo,src,srcPin,xBegin,xEnd,yBegin,yEnd, idx):
        # Do it directly for small values
        if len(todo)<=4:
            for d in todo.values():
                res.add_edge_instance(EdgeInstance(res, d,"modification_in",src,srcPin))
            return

        xR=xEnd-xBegin
        yR=yEnd-yBegin

        if xR >= yR:
            split_axis=0
            split_value=(xBegin+xEnd)//2
            decision=lambda x,y: 0 if x<split_value else 1
        else:
            split_axis=1
            split_value=(yBegin+yEnd)//2
            decision=lambda x,y: 0 if y<split_value else 1

        left={ (x,y):d for ((x,y),d) in todo.items() if decision(x,y)==0 }
        right={ (x,y):d for ((x,y),d) in todo.items() if decision(x,y)==1 }

        dev=DeviceInstance(res, "f_{}".format(idx), fanoutType, {"split_axis":split_axis,"split_val":split_value})
        res.add_device_instance(dev)
        res.add_edge_instance(EdgeInstance(res, dev,"modification_in", src,srcPin))

        if xR >= yR:
            recurse_fanout(left, dev, "modification_out_left", xBegin, split_value, yBegin, yEnd, idx*2)
            recurse_fanout(right, dev,"modification_out_right", split_value, xEnd, yBegin, yEnd, idx*2+1)
        else:
            recurse_fanout(left, dev, "modification_out_left",xBegin, xEnd, yBegin, split_value, idx*2)
            recurse_fanout(right, dev, "modification_out_right",xBegin, xEnd, split_value, yEnd, idx*2+1)

    recurse_fanout(nodes,root,"modification_out", 0, n, 0, n, 1)

res.add_edge_instance(EdgeInstance(res, root,"tick_in",root,"tick_out"))

save_graph(res,sys.stdout)        
