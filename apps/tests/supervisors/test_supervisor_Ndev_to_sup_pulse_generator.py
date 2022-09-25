#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_v4 import save_graph
import sys
import os
import math
from graph.build_xml_stream import make_xml_stream_builder

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/test_supervisor_Ndev_to_sup_pulse_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

graphType=graphTypes["test_supervisor_Ndev_to_sup_pulse_gt"]
devType=graphType.device_types["dev"]

n=100
reps=100

if len(sys.argv)>1:
    n=int(sys.argv[1])
if len(sys.argv)>2:
    reps=int(sys.argv[2])

instName=f"test_supervisor_Ndev_to_sup_pulse_gt_n{n}_reps{reps}"

properties={"reps":reps,"n":n}

sink=make_xml_stream_builder(sys.stdout, xml_version=4)

sink.begin_graph_instance(instName, graphType, properties=properties)

nodes={}

for i in range(0,n):
    sink.add_device_instance(f"d{i}", devType)

sink.end_device_instances()
sink.end_graph_instance()