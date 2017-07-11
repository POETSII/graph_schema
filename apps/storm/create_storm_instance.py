from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph

import sys
import os
import math
import random
import numpy
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
    
wideConnections=make_random(n,d,w)

instName="storm_{}_{}_{}".format(n,d,w)

properties=None

res=GraphInstance(instName, graphType, properties)

nodes=[]

progress=[]
for i in range(0,n):
    wide=wideConnections[i]
    props={"isRoot":1 if i==0 else 0, "degree":len(wideConnections[i])}
    
    nodes.append(DeviceInstance(res, "n{}".format(i), nodeType, props))
    res.add_device_instance(nodes[i])
    
for i in range(0,n):
    sys.stderr.write(" Edges : Node {} of {}\n".format( i, n) )
    
    res.add_edge_instance(EdgeInstance(res, nodes[(i+1)%n], "credit", nodes[i], "narrow"))
    
    for di in wideConnections[i]:
        res.add_edge_instance(EdgeInstance(res, nodes[di], "credit", nodes[i], "wide"))
    
save_graph(res,sys.stdout)        
