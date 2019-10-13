#!/usr/bin/env python3

from graph.core import *
from graph.expand_code import expand_graph_type_source
from create_standard_probabilities import get_probs

from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import make_xml_stream_builder
import sys
import os
import math
import subprocess


import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/ising_spin_fix_ext_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

T=1
J=1
H=0

n=4
if len(sys.argv)>1:
    n=int(sys.argv[1])
if len(sys.argv)>2:
    T=float(sys.argv[2])

slice_step=2**18

sys.stderr.write(f"T={T}, J={J}, H={H}\n")

ref_outputs=None

graphType=graphTypes["ising_spin_fix_ext"]
expand_graph_type_source( graphType, src ) # Embed header

devType=graphType.device_types["cell"]
externalType=graphType.device_types["sink"]



instName="ising_spin_fix_ext_{}_{}".format(n,n)

probs=get_probs(T,J,H)

properties={
    "width":n,
    "height":n,
    "probabilities":probs,
    "slice_step":slice_step
    }

sink=make_xml_stream_builder(sys.stdout,require_interleave=True)
sink.begin_graph_instance(instName, graphType, properties=properties)

nodes={}

for x in range(0,n):
    for y in range(0,n):
        devProps={"x":x, "y":y, "tag":(x%3)+3*(y%3)}
        di=sink.add_device_instance(f"n_{x}_{y}".format(x,y), devType, properties=devProps)
        nodes[(x,y)]=di

external=sink.add_device_instance(f"e", externalType)

sink.end_device_instances()    

def add_channel(x,y,dx,dy,dir):
    edgeProps={"direction":dir}
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    sink.add_edge_instance(dst,"in", src,"out", properties=edgeProps)
    

for x in range(0,n):
    for y in range(0,n):
        centre=nodes[(x,y)]
        add_channel(x,y, 0, -1, 1)
        add_channel(x,y, +1, 0, 2)
        if n>2:
            add_channel(x,y, 0, +1, 3)
        if n>2:
            add_channel(x,y, -1, 0, 4)        
        sink.add_edge_instance(external,"pixel_input",centre,"pixel_out")
        sink.add_edge_instance(centre, "pixel_window", external, "chunk_advance")
        

sink.end_graph_instance()