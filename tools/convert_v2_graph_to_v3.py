#!/usr/bin/env python3

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import toNS, save_graph_type, save_graph_instance
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
(types,instances)=load_graph_types_and_instances(source, sourcePath, {"p":"https://poets-project.org/schemas/virtual-graph-schema-v2"})
assert len(types) == 1, "Only one graph type can be converted at once"
for t in types:
    graphType = types[t]

# CONVERT v2 to v3

# Remove __init__ message type if it exists
if "__init__" in graphType.message_types:
    graphType.message_types.pop("__init__")

deviceTypes = graphType.device_types

for dt in deviceTypes:
    if "__init__" in deviceTypes[dt].inputs:
        initPin = deviceTypes[dt].inputs["__init__"]
        deviceTypes[dt].init_handler = initPin.receive_handler
        deviceTypes[dt].inputs.pop("__init__")
        deviceTypes[dt].inputs_by_index.remove(initPin)

# Remove information on whether a pin is an "application pin" or not
for dt in deviceTypes:
    for inputPin in deviceTypes[dt].inputs_by_index:
        if inputPin.is_application:
            inputPin.is_application = False
    for outputPin in deviceTypes[dt].outputs_by_index:
        if outputPin.is_application:
            outputPin.is_application = False

# SAVE XML
nsmap = { None : "https://poets-project.org/schemas/virtual-graph-schema-v3" }
root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)

save_graph_type(root,graphType)
for i in instances:
    save_graph_instance(root, instances[i])

# The following was "borrowed" straight out of graph/save_xml.py
tree = etree.ElementTree(root)
# TODO : Fix this!
if (sys.version_info > (3, 0)):
    # Python3
    tree.write(dest.buffer, pretty_print=True, xml_declaration=True)
else:
    #Python2
    tree.write(dest, pretty_print=True, xml_declaration=True)
# End of "borrowed" code

sys.stderr.write("\nCONVERSION TO v3 COMPLETE\n")
