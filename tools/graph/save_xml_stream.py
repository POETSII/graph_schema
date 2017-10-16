from graph.core import *
from graph.save_xml import save_graph_type,toNS

from lxml import etree

import os
import sys
import json

ns={"p":"https://poets-project.org/schemas/virtual-graph-schema-v2"}



def write_edge_instance(dst,ei):
    properties=ei.properties
    properties=properties and json.dumps(properties)
    properties=properties and properties[1:-1]

    metadata=ei.metadata
    metadata=metadata and json.dumps(metadata)
    metadata=metadata and metadata[1:-1]

    if metadata or properties:
        dst.write('   <EdgeI path="{}">'.format(ei.id))
        if properties:
            dst.write('<P>{}</P>'.format(properties))
        if metadata:
            dst.write('<M>{}</M>'.format(metadata))
        dst.write("</EdgeI>\n")
    else:
        dst.write('   <EdgeI path="{}" />\n'.format(ei.id))


def write_device_instance(dst,di):
    properties=di.properties
    properties=properties and json.dumps(properties)
    properties=properties and properties[1:-1]

    metadata=di.metadata
    metadata=metadata and json.dumps(metadata)
    metadata=metadata and metadata[1:-1]

    if metadata or properties:
        dst.write('   <DevI id="{}" type="{}">\n'.format(di.id,di.device_type.id))
        if properties:
            dst.write('<P>{}</P>'.format(properties))
        if metadata:
            dst.write('<M>{}</M>'.format(metadata))
        dst.write("</DevI>\n")
    else:
        dst.write('   <DevI id="{}" type="{}" />\n'.format(di.id,di.device_type.id))
        


def save_graph(graph,dst):
    ns="https://poets-project.org/schemas/virtual-graph-schema-v2"
    nsmap = { None : ns }
    root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)

    sys.stderr.write("save_graph: Constructing graph type tree\n")
    graphTypeNode=save_graph_type(root,graph.graph_type)

    graphTypeText=etree.tostring(graphTypeNode,pretty_print=True).decode("utf8")

    dst.write("<?xml version='1.0'?>\n")
    dst.write('<Graphs xmlns="{}">\n'.format(ns))

    dst.write(graphTypeText)

    dst.write(' <GraphInstance id="{}" graphTypeId="{}">\n'.format(graph.id,graph.graph_type.id))
    if graph.properties:
        dst.write('   <Properties>\n')
        dst.write(json.dumps(graph.properties,indent=2)[1:-1])
        dst.write('\n')
        dst.write('   </Properties>\n')
    if graph.metadata:
        dst.write('   <MetaData>\n')
        dst.write(json.dumps(graph.metadata,indent=2)[1:-1])
        dst.write('\n')
        dst.write('   </MetaData>\n')
    dst.write('  <DeviceInstances>\n')
    for di in graph.device_instances.values():
        write_device_instance(dst,di)
    dst.write('  </DeviceInstances>\n')
    dst.write('  <EdgeInstances>\n')
    for ei in graph.edge_instances.values():
        write_edge_instance(dst,ei)
    dst.write('  </EdgeInstances>\n')
    dst.write(' </GraphInstance>\n')

    dst.write('</Graphs>\n')
