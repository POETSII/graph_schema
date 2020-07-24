#!/usr/bin/env python3

from graph.load_xml import load_graph
from graph.make_properties import *
import sys
import os
import argparse
import re
import math

parser=argparse.ArgumentParser("Render a graph to dot.")
parser.add_argument('graph', metavar="G", default="-", nargs="?")
parser.add_argument('--device-type-filter', dest="deviceTypeFilter", default=".*")
parser.add_argument('--output', default="graph.dot")

args=parser.parse_args()

shapes=["oval","box","diamond","pentagon","hexagon","septagon", "octagon"]
colors=["blue4","red4","green4"]

reDeviceTypeFilter=re.compile(args.deviceTypeFilter)

deviceTypeToShape={}

if args.graph=="-":
    graph=load_graph(sys.stdin, "<stdin>")
else:
    graph=load_graph(args.graph, args.graph)
    
    
assert graph.metadata!=None, "Graph instance has no metadata, so can't contain a placement."

found=[]
for k in graph.metadata:
    if k.startswith("dt10.partitions."):
        found.append(int(k[16:]))
        
assert len(found)>0, "Didn't find any dt10.partitions.N keys in metadata."
assert len(found)<2, "Found multiple partitions: {}".format(found)

select=found[0]
    
partitionInfoKey="dt10.partitions.{}".format(select)
partitionInfo=graph.metadata["dt10.partitions.{}".format(select)]
key=partitionInfo["key"]

device_to_partition = { di.id:int(di.metadata[key]) for di in graph.device_instances.values() }
for id in graph.device_instances:
    assert id in device_to_partition, "Missing partition for device {}".format(id)


partitions={ device_to_partition[p] for p in device_to_partition }
partition_to_devices = { p:[] for p in partitions }
for (d,p) in device_to_partition.items():
    partition_to_devices[p].append(d)
    



if len(graph.graph_type.device_types) <= len(shapes):
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes.pop(0)
else:
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes[0]


def write_graph(dst, graph,devStates=None,edgeStates=None):
    def out(string):
        print(string,file=dst)

    
    out('digraph "{}"{{'.format(graph.id))
    #out('  sep="+10,10";');
    #out('  overlap=false;');
    out('  spline=true;');
    
    incNodes=set()
    
    for p in partitions:
        out(' subgraph cluster_{} {{'.format(p))

        for id in partition_to_devices[p]:
            di=graph.device_instances[id]
            dt=di.device_type
            
            if not reDeviceTypeFilter.match(dt.id):
                continue
            
            incNodes.add( di.id )

            props=[]
            shape=deviceTypeToShape[di.device_type.id]
            props.append('shape={}'.format(shape))

            meta=di.metadata
            if meta and "x" in meta and "y" in meta:
                props.append('pos="{},{}"'.format(meta["x"],meta["y"]))
                props.append('pin=true')

            out('    "{}" [{}];'.format(di.id, ",".join(props)))
        
        out('  }')

    addLabels=False #len(graph.edge_instances) < 50

    for ei in graph.edge_instances.values():
        if not ei.src_device.id in incNodes:
            continue
        if not ei.dst_device.id in incNodes:
            continue
        
        props={}
        if device_to_partition[ei.src_device.id] == device_to_partition[ei.dst_device.id]:
            props["color"]="green4"
        else:
            props["color"]="red4"

        if addLabels:
            props["headlabel"]=ei.dst_pin.name
            props["taillabel"]=ei.src_pin.name
            props["label"]=ei.message_type.id

        props=",".join([ '{}="{}"'.format(k,v) for (k,v) in props.items()])

        out('  "{}" -> "{}" [{}];'.format(ei.src_device.id,ei.dst_device.id,props))


    out("}")


dst=sys.stdout
if args.output!="-":
    dst=open(args.output,"wt")
write_graph(dst,graph)
