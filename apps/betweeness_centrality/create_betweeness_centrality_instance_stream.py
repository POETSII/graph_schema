#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import make_xml_stream_builder

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
    sys.stderr.write("Warning: clamping time to 1023.\n")
    t=1023

iw=1
if len(sys.argv)>4:
    iw=int(sys.argv[4])

assert iw > 0
    

graphType=graphTypes["betweeness_centrality"]
nodeType=graphType.device_types["node"]
collectorType=graphType.device_types["collector"]

# We create a ring to make it fully connected, then add d-1 random edges
sys.stderr.write("Writing graph\n")

    
instName="betweeness_centrality_{}_{}_{}_{}".format(n,d,t,iw)

properties={
    "max_steps" : t,
    "initial_walks" : iw
}

sink=make_xml_stream_builder(sys.stdout)
sink.hint_total_devices(n+1)
sink.hint_total_edges(n*d)
sink.begin_graph_instance(instName, graphType, properties=properties)

nodes=[]
for i in range(0,n):

    props={"node_id":i, "degree":d, "seed":math.floor(urand()*2**32)}
    
    nodes.append(sink.add_device_instance(f"n{i}", nodeType, properties=props))

collector=sink.add_device_instance("c", collectorType, properties={"graph_size":n})

sink.end_device_instances()
    
for i in range(0,n):
    # We create a ring to make it fully connected, then add d-1 random edges
    edges=set([ (i+1)%n ] + random.sample(range(n), d-1) )
    while len(edges)<d:
        edges.add(random.randint(0, n-1))
    # Randomly choose between explicit and implicit edge indices
    if urand()<0.5:
        for e in edges:
            sink.add_edge_instance(nodes[e], "walk_arrive", nodes[i], "walk_continue")
    else:
        for (j,e) in enumerate(edges):
            sink.add_edge_instance(nodes[e], "walk_arrive", nodes[i], "walk_continue", send_index=j)
    
    sink.add_edge_instance(collector, "finished_walk", nodes[i], "walk_finish")
    
sink.end_graph_instance()       
