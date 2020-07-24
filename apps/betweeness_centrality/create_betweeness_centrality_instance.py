#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph

import sys
import os
import math
import random
#import numpy
import copy

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/betweeness_centrality_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])

d=8
if len(sys.argv)>2:
    d=int(sys.argv[2])

assert 1 <= d <= n

t=n
if len(sys.argv)>3:
    t=int(sys.argv[3])

if t>=1024:
    t=1023

iw=1
if len(sys.argv)>4:
    iw=int(sys.argv[4])

assert iw > 0
    

graphType=graphTypes["betweeness_centrality"]
nodeType=graphType.device_types["node"]
collectorType=graphType.device_types["collector"]

# We create a ring to make it fully connected, then add d-1 random edges
edges=[set([ (i+1)%n ] + random.sample(range(n), d-1) ) for i in range(n) ]

    
instName="betweeness_centrality_{}_{}_{}_{}".format(n,d,t,iw)

properties={
    "max_steps" : t,
    "initial_walks" : iw
}

res=GraphInstance(instName, graphType, properties)

nodes=[]
for i in range(0,n):

    props={"node_id":i, "degree":len(edges[i]), "seed":math.floor(urand()*2**32)}
    
    nodes.append(DeviceInstance(res, "n{}".format(i), nodeType, props))
    res.add_device_instance(nodes[i])

collector=DeviceInstance(res, "c", collectorType, {"graph_size":n})
res.add_device_instance(collector)
    
for i in range(0,n):
    sys.stderr.write(" Edges : Node {} of {}\n".format( i, n) )

    # Randomly choose between explicit and implicit edge indices    
    if urand()<0.5:
        for e in edges[i]:
            res.add_edge_instance(EdgeInstance(res, nodes[e], "walk_arrive", nodes[i], "walk_continue"))
    else:
        for (j,e) in enumerate(edges[i]):
            res.add_edge_instance(EdgeInstance(res, nodes[e], "walk_arrive", nodes[i], "walk_continue", send_index=j))
    
    res.add_edge_instance(EdgeInstance(res, collector, "finished_walk", nodes[i], "walk_finish"))
    
    
save_graph(res,sys.stdout)        
