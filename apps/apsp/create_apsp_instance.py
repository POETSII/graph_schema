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
branchType=graphType.device_types["branch"]


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
progress=[]
for i in range(0,n):
    props={"index":i, "degree":len(connections[i])}
    meta={}
    
    nodes[i]=DeviceInstance(res, "n{}".format(i), nodeType, props, meta)
    res.add_device_instance(nodes[i])
    
    begin=EdgeInstance(res,nodes[i],"begin_in",controller,"begin_out")
    res.add_edge_instance(begin)
    
    progress.append(nodes[i])
    
for i in range(0,n):
    sys.stderr.write(" Edges : Node {} of {}\n".format( i, n) )
    
    srcN=nodes[i]
    for dst in connections[i]:
        props={"w": math.floor(urand()*10)+1 }
        dstN=nodes[dst]
        sys.stderr.write("    dst={}, src={}\n".format(dstN.id,srcN.id))
        ei=EdgeInstance(res,dstN,"din",srcN,"dout",props)
        res.add_edge_instance(ei)
        
depth=0
while len(progress)>1:
    
    next=[]
    
    sys.stderr.write("  len(progress) == {}\n".format(len(progress)))
    
    if(len(progress)%2):
        next.append(progress.pop())
        
    assert 0==(len(progress)%2)
    
    
    for i in range(0,len(progress),2):
        src1=progress[i]
        src2=progress[i+1]
        branch=DeviceInstance(res, "b_{}_{}".format(depth,i), branchType, None, None)
        res.add_device_instance(branch)
        next.append(branch)
        
        res.add_edge_instance(EdgeInstance(res,branch,"progress_in_left",src1,"progress_out"))
        res.add_edge_instance(EdgeInstance(res,branch,"progress_in_right",src2,"progress_out"))

    depth=depth+1        
    progress=next

res.add_edge_instance(EdgeInstance(res,controller,"progress_in",progress[0],"progress_out"))
    
save_graph(res,sys.stdout)        
