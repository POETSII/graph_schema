#!/usr/bin/env python3

from graph.load_xml import load_graph_type
import sys
import os

if len(sys.argv)>1:
    src=sys.argv[1]
    basePath=os.path.dirname(src)
else:
    src=sys.stdin
    basePath=os.getcwd()

graph_type=load_graph_type(src,basePath)
print("{}".format(graph_type.id))
