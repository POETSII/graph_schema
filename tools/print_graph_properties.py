#!/usr/bin/env python3

from graph.load_xml import load_graph_instance
import sys
import os

if len(sys.argv)>1:
    src=os.path.abspath(sys.argv[1])
    basePath=src
else:
    src=sys.stdin
    basePath=os.path.join(os.getcwd(),"__xml_source_name_not_known__")

graph=load_graph_instance(src,basePath)
print("graph '{}'".format(graph.id))
print("message type count = {}".format(len(graph.graph_type.message_types)))
print("device type count = {}".format(len(graph.graph_type.device_types)))
print("device instance count = {}".format(len(graph.device_instances)))
print("edge instance count = {}".format(len(graph.edge_instances)))
