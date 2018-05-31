#!/usr/bin/env python3

from graph.load_xml import load_graph_types_and_instances
from graph.cpp_type_map import CppTypeMap


import sys
import os

source=sys.stdin
sourcePath="[graph-type-file]"

asHeader=False

sys.stderr.write("{}\n".format(sys.argv))

ia=1

if ia < len(sys.argv):
    if sys.argv[ia]!="-":
        sys.stderr.write("Reading graph type from '{}'\n".format(sys.argv[ia]))
        source=open(sys.argv[ia],"rt")
        sourcePath=os.path.abspath(sys.argv[1])
    ia+=1

dest=sys.stdout
destPath="[graph-cxx-prototype-file]"

if ia < len(sys.argv):
    if sys.argv[ia]!="-":
        sys.stderr.write("Writing graph types to '{}'\n".format(sys.argv[ia]))
        dest=open(sys.argv[ia],"wt")
        destPath=os.path.abspath(sys.argv[ia])
    ia+=1

(types,instances)=load_graph_types_and_instances(source, sourcePath)

if len(types)!=1:
    raise RuntimeError("File did not contain exactly one graph type.")

graph=None
for g in types.values():
    graph=g
    break

map=CppTypeMap(graph)
print(map.defs)
