from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph
import sys
import os
import math



src=sys.argv[1]
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

d=4
b=2
if len(sys.argv)>2:
    d=int(sys.argv[2])
if len(sys.argv)>3:
    b=int(sys.argv[3])

graphType=graphTypes["clock_tree"]
rootType=graphType.device_types["root"]
branchType=graphType.device_types["branch"]
leafType=graphType.device_types["leaf"]

instName="clock_{}_{}".format(d,b)

properties=None
res=GraphInstance(instName, graphType, properties)

nodes={}

def create(prefix, parent,depth):
    node=None
    if depth==0:
        node=DeviceInstance(res, "{}_leaf".format(prefix), leafType, None, None)
        res.add_device_instance(node)
        res.add_edge_instance(EdgeInstance(res,node,"tick_in",parent,"tick_out", None))
        res.add_edge_instance(EdgeInstance(res,parent,"ack_in",node,"ack_out",None))
    else:
        if depth==d:
            node=DeviceInstance(res, prefix, rootType, None, {"fanout":b})
        else:
            node=DeviceInstance(res, prefix, branchType, None, {"fanout":b})
        res.add_device_instance(node)
        for i in range(b):
            child=create("{}_{}".format(prefix,i), node, depth-1)

        if depth!=d:
            res.add_edge_instance(EdgeInstance(res,node,"tick_in", parent, "tick_out", None))
            res.add_edge_instance(EdgeInstance(res,parent,"ack_in", node,"ack_out", None))

    return node


create("root",None,d)

save_graph(res,sys.stdout)
