#!/usr/bin/env python3

from graph.load_xml import load_graph_types_and_instances
from graph.write_cpp import render_graph_as_cpp

from graph.expand_code import expand_graph_type_source

import sys
import os

source=sys.stdin
sourcePath="[graph-type-file]"

asHeader=False

sys.stderr.write("{}\n".format(sys.argv))

ia=1

if ia < len(sys.argv) and sys.argv[ia]=="--header":
    asHeader=True
    ia+=1

if ia < len(sys.argv):
    if sys.argv[ia]!="-":
        sys.stderr.write("Reading graph type from '{}'\n".format(sys.argv[ia]))
        source=open(sys.argv[ia],"rt")
        sourcePath=os.path.abspath(sys.argv[1])
        sys.stderr.write("Using absolute path '{}' for pre-processor directives\n".format(sourcePath))
    ia+=1

dest=sys.stdout
destPath="[graph-cxx-prototype-file]"

if ia < len(sys.argv):
    if sys.argv[ia]!="-":
        sys.stderr.write("Writing graph prototype to '{}'\n".format(sys.argv[ia]))
        dest=open(sys.argv[ia],"wt")
        destPath=os.path.abspath(sys.argv[ia])
        sys.stderr.write("Using absolute path '{}' for pre-processor directives\n".format(destPath))
    ia+=1

(types,instances)=load_graph_types_and_instances(source, sourcePath)

if len(types)!=1:
    raise RuntimeError("File did not contain exactly one graph type.")

graph=None
for g in types.values():
    graph=g
    break

expand_graph_type_source(graph,sourcePath)

class OutputWithPreProcLineNum:
    def __init__(self,dest,destName):
        self.dest=dest
        self.destName=destName
        self.lineNum=1

    def write(self,msg):
        for line in msg.splitlines():
            if line.strip()=="__POETS_REVERT_PREPROC_DETOUR__":
                dest.write('#line {} "{}"\n'.format(self.lineNum, self.destName))
            else:
                dest.write(line+"\n")
            self.lineNum+=1


render_graph_as_cpp(graph, OutputWithPreProcLineNum(dest, destPath), destPath)
