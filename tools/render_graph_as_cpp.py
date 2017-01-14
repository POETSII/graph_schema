from graph.load_xml import load_graph_types_and_instances
from graph.write_cpp import render_graph_as_cpp

import sys
import os

source=sys.stdin
sourcePath="[graph-type-file]"

sys.stderr.write("{}\n".format(sys.argv))

if len(sys.argv)>1:
    if sys.argv[1]!="-":
        sys.stderr.write("Reading graph type from '{}'\n".format(sys.argv[1]))
        source=open(sys.argv[1],"rt")
        sourcePath=os.path.abspath(sys.argv[1])
        sys.stderr.write("Using absolute path '{}' for pre-processor directives\n".format(sourcePath))

dest=sys.stdout
destPath="[graph-cxx-prototype-file]"

if len(sys.argv)>2:
    if sys.argv[2]!="-":
        sys.stderr.write("Writing graph prototype to '{}'\n".format(sys.argv[1]))
        dest=open(sys.argv[2],"wt")
        destPath=os.path.abspath(sys.argv[2])
        sys.stderr.write("Using absolute path '{}' for pre-processor directives\n".format(destPath))

(types,instances)=load_graph_types_and_instances(source, sourcePath)

if len(types)!=1:
    raise RuntimeError("File did not contain exactly one graph type.")

graph=None
for g in types.values():
    graph=g
    break
    
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
