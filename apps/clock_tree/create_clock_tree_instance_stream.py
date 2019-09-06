#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph
import sys
import os
import math
from graph.build_xml_stream import make_xml_stream_builder


import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/clock_tree_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

d=4
b=2
maxTicks=100
if len(sys.argv)>1:
    d=int(sys.argv[1])
if len(sys.argv)>2:
    b=int(sys.argv[2])
if len(sys.argv)>3:
    maxTicks=int(sys.argv[3])

graphType=graphTypes["clock_tree"]
rootType=graphType.device_types["root"]
branchType=graphType.device_types["branch"]
leafType=graphType.device_types["leaf"]
#listenerType=graphType.device_types["output_listener"]

instName="clock_{}_{}".format(d,b)

properties={"max_ticks":maxTicks}

sink=make_xml_stream_builder(sys.stdout,require_interleave=True)
assert sink.can_interleave

nodes={}

def create(prefix, parent,depth):
    node=None
    if depth==0:
        node=sink.add_device_instance(f"{prefix}_leaf", leafType)
        sink.add_edge_instance(node,"tick_in",parent,"tick_out")
        sink.add_edge_instance(parent,"ack_in",node,"ack_out")
    else:
        if depth==d:
            node=sink.add_device_instance(prefix, rootType, properties={"fanout":b})
        else:
            node=sink.add_device_instance(prefix, branchType, properties={"fanout":b})
        for i in range(b):
            child=create(f"{prefix}_{i}", node, depth-1)

        if depth!=d:
            sink.add_edge_instance(node,"tick_in", parent, "tick_out")
            sink.add_edge_instance(parent,"ack_in", node,"ack_out")

    return node


sink.begin_graph_instance(instName, graphType, properties=properties)
root=create("root",None,d)
sink.end_device_instances()
sink.end_graph_instance()