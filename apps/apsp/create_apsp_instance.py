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
branchType=graphType.device_types["control_fanout"]

def make_random(n,d):
    connections={}
    for i in range(0,n):
        outgoing = set([ (i+1)%n ]) # Enforce a ring
        for j in range(0,d-1):
            outgoing.add( math.floor(urand()*n) )
            
        connections[i]={ o:math.floor(urand()*10)+1 for o in outgoing } 
    return connections


def make_2x2_ref():
    #     n0
    #    /  \
    # 2 v    ^ 3
    #    \  /
    #     n1
    #
    #  src(n0):  dist=[0,2], sum=2, max=2
    #  src(n1):  dist=[3,0], sum=3, max=3
    #  sumSumDist=5, sumMaxDist=5
    return {
        0 : { 1:2 },
        1 : { 0:3 }
    }


if n==2 and d==2:
    connections=make_2x2_ref()
else:
    connections=make_random(n,d)

# TODO : This has appalling scaling...
def apsp_ref(conn):
    n=len(conn)

    def sp_ref(x):
        dist=[math.inf]*n
        dist[x]=0
        
        prev=[-math.inf]*n
        while prev!=dist:
            prev=list(dist)
            for src in range(n):
                for (dst,w) in conn[src].items():
                    dist[dst]=min(dist[dst], dist[src]+w)
        return dist
        
    sumMaxDist=0
    sumSumDist=0
    
    for i in range(n):
        dist=sp_ref(i)
        sumSumDist+=sum(dist)
        sumMaxDist+=max(dist)
    
    return (sumMaxDist,sumSumDist)
    
(refSumMaxDist,refSumSumDist)=apsp_ref(connections)
sys.stderr.write("{},{}\n".format(refSumMaxDist,refSumSumDist))


instName="apsp_{}_{}".format(n,d)

properties=None

res=GraphInstance(instName, graphType, properties)

controller=DeviceInstance(res, "controller", controllerType, {
    "node_count":len(connections),
    "refSumMaxDist":refSumMaxDist,
    "refSumSumDist":refSumSumDist
})
res.add_device_instance(controller)

nodes={}
progress=[]
for i in range(0,n):
    props={"index":i, "degree":len(connections[i])}
    meta={}
    
    nodes[i]=DeviceInstance(res, "n{}".format(i), nodeType, props, meta)
    res.add_device_instance(nodes[i])
    
    progress.append(nodes[i])
    
for i in range(0,n):
    sys.stderr.write(" Edges : Node {} of {}\n".format( i, n) )
    
    srcN=nodes[i]
    for (dst,w) in connections[i].items():
        props={"w": w }
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
        branch=DeviceInstance(res, "b_{}_{}".format(depth,i), branchType, {"degree":2}, None)
        res.add_device_instance(branch)
        next.append(branch)
        
        res.add_edge_instance(EdgeInstance(res,branch,"response_in",src1,"response_out"))
        res.add_edge_instance(EdgeInstance(res,branch,"response_in",src2,"response_out"))
        
        res.add_edge_instance(EdgeInstance(res,src1,"request_in",branch,"request_out"))
        res.add_edge_instance(EdgeInstance(res,src2,"request_in",branch,"request_out"))

    depth=depth+1        
    progress=next

res.add_edge_instance(EdgeInstance(res,progress[0],"request_in",controller,"request_out"))
res.add_edge_instance(EdgeInstance(res,controller,"response_in",progress[0],"response_out"))
    
save_graph(res,sys.stdout)        
