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
print("{}".format(graph.graph_type.id))
