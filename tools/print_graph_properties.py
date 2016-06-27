from graph.load_xml import load_graph
import sys
import os

graph=load_graph(sys.stdin)
print("graph '{}'".format(graph.id))
print("edge type count = {}".format(len(graph.graph_type.edge_types)))
print("device type count = {}".format(len(graph.graph_type.device_types)))
print("device instance count = {}".format(len(graph.device_instances)))
print("edge instance count = {}".format(len(graph.edge_instances)))
