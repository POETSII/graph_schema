#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import XmlV3StreamGraphBuilder

import sys
import os
import math
import random
#import numpy
import copy

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/storm_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])
    
d=n//2
if len(sys.argv)>2:
    d=int(sys.argv[2])
    
w=n
if len(sys.argv)>2:
    w=int(sys.argv[2])
    
assert( d <= w )


graphType=graphTypes["storm"]
nodeType=graphType.device_types["node"]

def make_random(n,d,w):
    connections={}
    for i in range(0,n):
        connections[i] = [ (i+j)%n for j in random.sample(range(w),d)]
    return connections
    
sys.stderr.write("Creating connections\n")
wideConnections=make_random(n,d,w)

instName="storm_{}_{}_{}".format(n,d,w)

properties=None

sink=XmlV3StreamGraphBuilder(sys.stdout)
sink.begin_graph_instance(instName, graphType, properties=properties)

nodes=[]

sys.stderr.write("Creating devices\n")
progress=[]
for i in range(0,n):
    wide=wideConnections[i]
    props={"isRoot":1 if i==0 else 0, "degree":len(wideConnections[i])}
    
    nodes.append(sink.add_device_instance(f"n{i}", nodeType, properties=props))

sink.end_device_instances()

sys.stderr.write("Creating edges\n")
for i in range(0,n):
    sink.add_edge_instance(nodes[(i+1)%n], "credit", nodes[i], "narrow")
    
    for di in wideConnections[i]:
        sink.add_edge_instance(nodes[di], "credit", nodes[i], "wide")

sink.end_graph_instance()
