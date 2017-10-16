#!/usr/bin/env python3

from graph.load_xml import load_graph
from graph.save_xml_stream import save_graph
from graph.save_xml import save_metadata_patch
from graph.metadata import *
import sys
import os
import argparse
import random
from collections import defaultdict


def render_graph_as_metis(graph,dst):
    """ Output is:
    
        idToIndex : mapping from device id to zero-based index in metis graph
    """
    
    mtWeights={}
    hasWeights=False
    for mt in graph.graph_type.message_types.values():
        sys.stderr.write("{}\n".format(mt.metadata))
        w=1
        if mt.metadata!=None and "dt10.partition.weight" in mt.metadata:
            w=mt.metadata["dt10.partition.weight"]
        sys.stderr.write("{}\n".format(w))
        if w != 1:
            hasWeights=True
        assert int(w)==w, "Weights must be integers for metis"
        assert w>=0, "Weights must be non-negative"
        mtWeights[mt.id]=w
    
    idToIndex={}
    indexToId=[]
    idToEdges={  }
    for di in graph.device_instances.values():
        idToIndex[di.id]=len(idToIndex)
        indexToId.append(di.id)
        idToEdges[di.id]=set()

    edges=defaultdict(int) # Map from (idLo,idHi) -> weight
    for ei in graph.edge_instances.values():
        srcId=ei.src_device.id
        dstId=ei.dst_device.id
        
        assert srcId in idToIndex
        assert dstId in idToIndex

        if srcId==dstId:
            continue  # TODO: metis doesn't deal with self-loops
            
        w=mtWeights[ei.message_type.id]
        if w==0:
            continue # Remove edges that have zero weight
        
        idToEdges[srcId].add(dstId)
        idToEdges[dstId].add(srcId)
        
        (lo,hi)=(min(srcId,dstId),max(srcId,dstId))
        
        edges[(lo,hi)]+=w
        
    nSame=0
    for ((a,b),w) in edges.items():
        assert a==b or (b,a) not in edges
        if a==b:
            nSame+=1
        if w==0:
            idToEdges[a].discard(b)
            idToEdges[b].discard(a)
    assert nSame==0, "Metis can't deal with self-loops"
            
    n=len(idToIndex)
    m=len(edges)
    
    dst.write("{} {} 00{}\n".format(n,m, (1 if hasWeights else 0) )) 
    eCount=0
    for id in indexToId:
        first=True
        for e in idToEdges[id]:
            assert id in idToEdges[e]
            
            (lo,hi)=(min(id,e),max(id,e))

            assert (lo,hi) in edges
            
            if not hasWeights:
                dst.write(" {}".format(idToIndex[e]+1)) # one-based vertex indices
            else:
                w=edges[(lo,hi)]
                assert w!=0
                dst.write(" {} {}".format(idToIndex[e]+1,w)) # one-based vertex indices
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

def create_metis_partition(graph,partitions,threads):
    """Create a graph with the given number of partitions, and then assign
    to the given number of threads. If partitions==threads, then we are down
    to one thread per partition (which seems to work poorly). If n*partitions==threads,
    then there are n threads in each partition. Typically we would want partitions==mbox_count
    or partitions==mbox_count*core_count."""
    import tempfile
    
    assert partitions <= threads
    
    if partitions==threads:
        threadToPartition=list(range(threads))
        partitionToThreads=[ [i] for i in range(partitions) ]
    else:
        threadToPartition=[i%partitions for i in range(threads)]
        partitionToThreads=[ [t for t in range(threads) if threadToPartition[t]==p] for p in range(partitions) ]
    
    (metisHandle,metisName)=tempfile.mkstemp(suffix=".metis", prefix=None, dir=None, text=True)
    metisStream=open(metisHandle,mode="wt")
    idToIndex=render_graph_as_metis(graph,metisStream)
    
    import subprocess
    subprocess.run( args=[ "gpmetis", metisName, str(partitions) ], check=True, stdout=sys.stderr )
    
    idToPartition=read_metis_partition(graph,partitions,idToIndex,metisName)
    
    # Allocate devices to threads in a round-robin fashion.
    partitionCounters=[0]*partitions
    def choose_thread(partition):
        counter=partitionCounters[partition]
        partitionCounters[partition]+=1
        pmap=partitionToThreads[partition]
        return pmap[counter % len(pmap)]
    
    idToThread={ id:choose_thread(p) for (id,p) in idToPartition.items() }
    
    counts=[0]*threads
    for p in idToThread.values():
        counts[p]+=1
    
    tagName="dt10.partitions.{}".format(threads)
    
    (key,graphMeta)=create_device_instance_key(tagName, graph.metadata or {})
    graphMeta[tagName]= {
        "counts" : counts,
        "key" : key
    }
    deviceMeta={
        id:{key:idToThread[id]} for id in idToThread
    }
    edgeMeta={}
    
   
    return (graphMeta,deviceMeta,edgeMeta)
    
    

if __name__=="__main__":

    parser=argparse.ArgumentParser("Render a graph as a metis partitioning problem.")
    parser.add_argument('graph', metavar="G", default="-", nargs="?")
    parser.add_argument('partitions', metavar="P", default="2", nargs="?", type=int)
    parser.add_argument('threads', metavar="T", default="1024", nargs="?", type=int)
    parser.add_argument('--patch', default=False, action='store_const', const=True)

    args=parser.parse_args()
    
    partitions=args.partitions
    threads=args.threads

    if args.graph=="-":
        graph=load_graph(sys.stdin, "<stdin>")
    else:
        graph=load_graph(args.graph, args.graph)

    (graphMeta,deviceMeta,edgeMeta)=create_metis_partition(graph, partitions, threads)
    
    dst=sys.stdout
    
    if args.patch:
        save_metadata_patch(graph.id, graphMeta, deviceMeta, edgeMeta, dst)
    else:
        sys.stderr.write("Merging\n")
        merge_metadata_into_graph(graph, graphMeta, deviceMeta, edgeMeta)
        sys.stderr.write("Saving\n")
        save_graph(graph,dst)
