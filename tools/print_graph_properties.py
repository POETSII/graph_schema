#!/usr/bin/env python3

from graph.load_xml import load_graph
import sys
import os

if len(sys.argv)>1:
    src=sys.argv[1]
    basePath=os.path.dirname(src)
else:
    src=sys.stdin
    basePath=os.getcwd()

graph=load_graph(src,basePath)
print("graph '{}'".format(graph.id))
print("message type count = {}".format(len(graph.graph_type.message_types)))
print("device type count = {}".format(len(graph.graph_type.device_types)))
print("device instance count = {}".format(len(graph.device_instances)))
print("edge instance count = {}".format(len(graph.edge_instances)))
