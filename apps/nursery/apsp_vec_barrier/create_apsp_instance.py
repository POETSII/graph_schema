#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph

import numpy as np

import sys
import os
import math
import random
#import numpy
import copy

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/apsp_vec_barrier_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

K=8

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])
    
d=8
if len(sys.argv)>2:
    d=int(sys.argv[2])

graphType=graphTypes["apsp_vec_barrier"]
vertexType=graphType.device_types["vertex"]
collectorType=graphType.device_types["collector"]

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


first_round_distances_count=32

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

    # Map of dst -> [dist_from_0, dist_from_1, ... ]
    first_round_distances=[ [0]*first_round_distances_count for i in range(n) ]
    second_round_distances=[ [0]*first_round_distances_count for i in range(n) ]
    
    #NOTE: This definition of sum max is the opposite of the previous apsp. So it is the maximum
    # over a destination, not the maximum over a source.
    max_dist=[0 for i in range(n)]
    for src in range(n):
        if (src%100)==0:
            sys.stderr.write("  {} / {}\n".format(src,n))
        dist=sp_ref(src)
        if src < first_round_distances_count and src < K:
            for i in range(n):
                first_round_distances[i][src]=dist[i]
        elif src < first_round_distances_count and src < 2*K:
            for i in range(n):
                second_round_distances[i][src-K]=dist[i]
        sumSumDist+=sum(dist)
        sumMaxDist+=max(dist)

    sys.stderr.write(f"sumMaxDist={sumMaxDist}\n")
    
    return (sumMaxDist,sumSumDist,first_round_distances,second_round_distances)

if len(connections)<=4096:
    sys.stderr.write("Calculating ref dists...\n")
    if len(connections)>=1000:
        sys.stderr.write("   (warning: this will be slow)\n")
    (refSumMaxDist,refSumSumDist,first_round_distances,second_round_distances)=apsp_ref(connections)
    sys.stderr.write("   done\n")
else:
    sys.stderr.write("WARNING: big graph, so not calculating ref dists. Self-check will fail.\n")
    (refSumMaxDist,refSumSumDist,first_round_distances,second_round_distances)=(0,0, [[0]*first_round_distances_count for i in range(n)], [[0]*first_round_distances_count for i in range(n)])
sys.stderr.write("{},{}\n".format(refSumMaxDist,refSumSumDist))


instName="apsp_{}_{}".format(n,d)

properties={"total_vertices":len(connections), "gen_k":K}

res=GraphInstance(instName, graphType, properties)

collector=DeviceInstance(res, "collector", collectorType, {
    
    "refSumMaxDist":refSumMaxDist,
    "refSumSumDist":refSumSumDist
})
res.add_device_instance(collector)

nodes={}
progress=[]
for i in range(0,n):
    frd=first_round_distances[i]
    srd=second_round_distances[i]
    nodes[i]=DeviceInstance(res, "n{}".format(i), vertexType, {"id":i, "debug_1st_round_distances":frd, "debug_2nd_round_distances":srd})
    res.add_device_instance(nodes[i])
    progress.append(nodes[i])

dot_debug=open(f"{instName}.dot", "wt")
dot_debug.write(f"digraph {instName} {{\n")
    
for i in range(0,n):
    if n<1000:
        sys.stderr.write(" Edges : Node {} of {}\n".format( i, n) )
    
    srcN=nodes[i]
    for (dst,w) in connections[i].items():
        props={"w": w }
        dstN=nodes[dst]
        if n<100:
            sys.stderr.write("    dst={}, src={}\n".format(dstN.id,srcN.id))
        ei=EdgeInstance(res,dstN,"share_in",srcN,"share_out",props)
        res.add_edge_instance(ei)

        dot_debug.write(f'  n{i} -> n{dst} [ label="{w}" ];\n ')
        
    res.add_edge_instance(EdgeInstance(res,collector,"stats_in",srcN,"stats_out"))

dot_debug.write("}\n")
    
save_graph(res,sys.stdout)        
