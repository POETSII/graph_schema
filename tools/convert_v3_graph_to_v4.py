#!/usr/bin/env python3

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_v4 import save_graph
from lxml import etree

import sys
import os

source=sys.stdin
sourcePath="[XML-file]"

if len(sys.argv) != 2:
    raise RuntimeError("This converts exactly one XML file. Please provide one path to an XML file")

sys.stderr.write("Reading graph type from '{}'\n".format(sys.argv[1]))
source=open(sys.argv[1],"rt")
sourcePath=os.path.abspath(sys.argv[1])

dest=sys.stdout
destPath="[XML-file]"

# LOAD XML
(types,instances)=load_graph_types_and_instances(source, sourcePath, {"p":"https://poets-project.org/schemas/virtual-graph-schema-v3"}, True)
assert len(types) == 1, "Exactly one graph type can be converted at once, this contains {} graph types".format(len(types))
assert len(types) == 1, "Exactly one graph instance can be converted at once, this contains {} graph instances".format(len(instances))

for i in instances.values():
    instance=i
    break

save_graph(instance, dest)