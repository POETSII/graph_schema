from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph

import sys
import os
import math
import random

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/apsp_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])
    
d=8
if len(sys.argv)>2:
    d=int(sys.argv[2])


graphType=graphTypes["apsp"]
nodeType=graphType.device_types["node"]
controllerType=graphType.device_types["controller"]


connections={}
for i in range(0,n):
    outgoing = set([ (i+1)%n ]) # Enforce a ring
    for j in range(0,d-1):
        outgoing.add( math.floor(urand()*n) )
        
    connections[i]=outgoing


instName="apsp_{}_{}".format(n,d)

properties=None

res=GraphInstance(instName, graphType, properties)

controller=DeviceInstance(res, "controller", controllerType, {"node_count":len(connections)})
res.add_device_instance(controller)

nodes={}
for i in range(0,n):
    props={"index":i, "degree":len(connections[i])}
    meta={}
    
    nodes[i]=DeviceInstance(res, "n{}".format(i), nodeType, props, meta)
    res.add_device_instance(nodes[i])
    
    prog=EdgeInstance(res,controller,"progress_in",nodes[i],"progress_out")
    res.add_edge_instance(prog)
    
    begin=EdgeInstance(res,nodes[i],"begin_in",controller,"begin_out")
    res.add_edge_instance(begin)
    
        
for i in range(0,n):
    sys.stderr.write(" Edges : Node {} of {}\n".format( i, n) )
    
    srcN=nodes[i]
    for dst in connections[i]:
        props={"w": math.floor(urand()*10)+1 }
        dstN=nodes[dst]
        sys.stderr.write("    dst={}, src={}\n".format(dstN.id,srcN.id))
        ei=EdgeInstance(res,dstN,"din",srcN,"dout",props)
        res.add_edge_instance(ei)
    
save_graph(res,sys.stdout)        
