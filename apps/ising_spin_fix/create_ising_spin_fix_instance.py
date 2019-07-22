#!/usr/bin/env python3

from graph.core import *
from graph.expand_code import expand_graph_type_source
from create_standard_probabilities import get_probs

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph
import sys
import os
import math
import subprocess


import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/ising_spin_fix_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

endTime=100
T=1
J=1
H=0

n=4
if len(sys.argv)>1:
    n=int(sys.argv[1])
if len(sys.argv)>2:
    T=float(sys.argv[2])
if len(sys.argv)>3:
    endTime=float(sys.argv[3])

sys.stderr.write(f"T={T}, J={J}, H={H}\n")

ref_outputs=None
if n*n*endTime < 10000000:
    sys.stderr.write("Calculating reference output.\n")
    try:
        ref_path=os.path.join(appBase, "ising_spin_fix_ref")
        res=subprocess.run([ref_path,str(n),str(T),str(endTime),str(J),str(H)], stdout=subprocess.PIPE, universal_newlines=True)
        if res.returncode!=0:
            sys.stderr.write("Got non-zero return code from ising_spin_fix_ref, graph will not be self-checking\n")
        else:
            sys.stderr.write("Parsing ref grid\n")
            ref_outputs={}
            grid=str(res.stdout)
            sys.stderr.write(f"{grid}")
            lines=grid.splitlines()
            sys.stderr.write("Checking number of lines\n")
            assert len(lines)==n, f"Expected {n} lines, but got {len(lines)}"
            for (y,line) in enumerate(lines):
                sys.stderr.write(f"Line {y}\n")
                line=line.strip()
                assert len(line)==n
                for (x,val) in enumerate(line):
                    assert val=='+' or val=='-'
                    ref_outputs[(x,y)]=+1 if val=='+' else -1
    except Exception as e:
        sys.stderr.write("Error occurred while generating references, Graph will not be self-checking.\n")
        sys.stderr.write(str(e)+"\n\n")
        ref_outputs=None

graphType=graphTypes["ising_spin_fix"]
expand_graph_type_source( graphType, src ) # Embed header

devType=graphType.device_types["cell"]
finishedType=graphType.device_types["exit_node"]



instName="heat_{}_{}".format(n,n)

# probs=[0]*10
# for i in range(0,5):
#     for j in range(0,2):
#         index = i + 5*j; #'" index == 0,1,... ,9 "/
#         my_spin = 2*j - 1;
#         sum_nei = 2*i - 4;
#         d_E = 2.*(J * my_spin * sum_nei + H * my_spin);
#         x = math.exp(-d_E/T)
#         p=x/(1.+x)
#         probs[index]=math.floor(p * 2**32);
#         sys.stderr.write("prob[{}] = {} = p\n".format(index,probs[index], p))

probs=get_probs(T,J,H)

properties={
    "endTime":int(endTime * 2**20),
    "width":n,
    "height":n,
    "probabilities":probs
    }

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        devProps={"x":x, "y":y}
        if ref_outputs is not None:
            devProps["ref_final_spin"]=ref_outputs[(x,y)]
        di=DeviceInstance(res,"n_{}_{}".format(x,y), devType, devProps)
        nodes[(x,y)]=di
        res.add_device_instance(di)
        
def add_channel(x,y,dx,dy,dir):
    edgeProps={"direction":dir}
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    ei=EdgeInstance(res,dst,"in", src,"out", edgeProps)
    res.add_edge_instance(ei)


for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        centre=nodes[(x,y)]
        add_channel(x,y, 0, -1, 1)
        add_channel(x,y, +1, 0, 2)
        if n>2:
            add_channel(x,y, 0, +1, 3)
        if n>2:
            add_channel(x,y, -1, 0, 4)        
        
finished=DeviceInstance(res, "f", finishedType, {"fanin":len(nodes)})
res.add_device_instance(finished)

for (id,di) in nodes.items():
    ei=EdgeInstance(res,finished,"finished",di,"finished")
    res.add_edge_instance(ei)

save_graph(res,sys.stdout)        
