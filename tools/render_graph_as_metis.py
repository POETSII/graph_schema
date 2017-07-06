#!/usr/bin/python3

from graph.load_xml import load_graph
from graph.save_xml import save_graph, save_metadata_patch
from graph.metadata import *
import sys
import os
import argparse


def render_graph_as_metis(graph,dst):
    """ Output is:
    
        idToIndex : mapping from device id to zero-based index in metis graph
    """
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
    
    dst.write("{} {}\n".format(n,m))  # No   edge or vertex weights
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
        
    dst.flush()

    assert eCount==2*m-nSame, "m={},eCount={}, nSame={}".format(2*m,eCount,nSame)
    
    return idToIndex
    

def read_metis_partition(graph,partitions,idToIndex,graphFileName):
    partitionFileName=graphFileName+".part.{}".format(partitions)
    mapping=[]
    with open(partitionFileName,"rt") as partitionFile:
        for line in partitionFile:
            p=int(line)
            if p<0 or p>=partitions:
                raise RuntimeError("Invalid partition from metis")
            mapping.append(p)
    if len(mapping)!=len(idToIndex):
        raise RuntimeError("Invalid number of mappings from metis")
        
    return { id:mapping[idToIndex[id]] for id in graph.device_instances }

def create_metis_partition(graph,partitions):
    import tempfile
    
    (metisHandle,metisName)=tempfile.mkstemp(suffix=".metis", prefix=None, dir=None, text=True)
    metisStream=open(metisHandle,mode="wt")
    idToIndex=render_graph_as_metis(graph,metisStream)
    
    import subprocess
    subprocess.run( args=[ "gpmetis", metisName, str(partitions) ], check=True, stdout=sys.stderr )
    
    idToPartition=read_metis_partition(graph,partitions,idToIndex,metisName)
    
    counts=[0]*partitions
    for p in idToPartition.values():
        counts[p]+=1
    
    tagName="dt10.partitions.{}".format(partitions)
    
    (key,graphMeta)=create_device_instance_key(tagName, graph.metadata or {})
    graphMeta[tagName]= {
        "counts" : counts
    }
    deviceMeta={
        id:{key:idToPartition[id]} for id in idToPartition
    }
    edgeMeta={}
    
   
    return (graphMeta,deviceMeta,edgeMeta)
    
    

if __name__=="__main__":

    parser=argparse.ArgumentParser("Render a graph as a metis partitioning problem.")
    parser.add_argument('graph', metavar="G", default="-", nargs="?")
    parser.add_argument('partitions', metavar="P", default="2", nargs="?", type=int)
    parser.add_argument('--patch', default=False, action='store_const', const=True)

    args=parser.parse_args()
    
    partitions=args.partitions

    if args.graph=="-":
        graph=load_graph(sys.stdin, "<stdin>")
    else:
        graph=load_graph(args.graph, args.graph)

    (graphMeta,deviceMeta,edgeMeta)=create_metis_partition(graph, partitions)
    
    dst=sys.stdout
    
    if args.patch:
        save_metadata_patch(graph.id, graphMeta, deviceMeta, edgeMeta, dst)
    else:
        sys.stderr.write("Merging\n")
        merge_metadata_into_graph(graph, graphMeta, deviceMeta, edgeMeta)
        sys.stderr.write("Saving\n")
        save_graph(graph,dst)
