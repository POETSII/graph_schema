#!/usr/bin/python3

from graph.load_xml import load_graph
from graph.snapshots import extractSnapshotInstances
import sys
import os
import argparse

parser=argparse.ArgumentParser("Render a graph as a metis partitioning problem.")
parser.add_argument('graph', metavar="G", default="-", nargs="?")

args=parser.parse_args()

if args.graph=="-":
    graph=load_graph(sys.stdin, "<stdin>")
else:
    graph=load_graph(args.graph, args.graph)

idToIndex={}
indexToId=[]
idToEdges={  }
for di in graph.device_instances.values():
    idToIndex[di.id]=len(idToIndex)
    indexToId.append(di.id)
    idToEdges[di.id]=set()

edges=set()
for ei in graph.edge_instances.values():
    assert ei.src_device.id in idToIndex
    assert ei.dst_device.id in idToIndex

    if ei.src_device.id==ei.dst_device.id:
        continue  # TODO: metis doesn't deal with self-loops
    
    idToEdges[ei.src_device.id].add(ei.dst_device.id)
    idToEdges[ei.dst_device.id].add(ei.src_device.id)
    if ei.src_device.id < ei.dst_device.id:
        edges.add( (ei.src_device.id,ei.dst_device.id) )
    else:
        edges.add( (ei.dst_device.id,ei.src_device.id) )

nSame=0
for (a,b) in edges:
    assert a==b or (b,a) not in edges
    if a==b:
        nSame+=1
assert nSame==0, "Metis can't deal with self-loops"
        
n=len(idToIndex)
m=len(edges)

dst=sys.stdout

dst.write("{} {}\n".format(n,m))  # No edge or vertex weights
eCount=0
for id in indexToId:
    first=True
    for e in idToEdges[id]:
        assert id in idToEdges[e]

        if id < e:
            assert (id,e) in edges
        else:
            assert (e,id) in edges
        
        if first:
            first=False
        else:
            dst.write(" ")
        dst.write("{}".format(idToIndex[e]+1)) # one-based vertex indices
        eCount=eCount+1
    dst.write("\n")

assert eCount==2*m-nSame, "m={},eCount={}, nSame={}".format(2*m,eCount,nSame)
