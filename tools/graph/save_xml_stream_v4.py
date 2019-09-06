from graph.core import *
from graph.save_xml_v4 import save_graph_type,toNS
from graph.save_xml_v4 import convert_json_init_to_c_init

from lxml import etree

import os
import sys

try:
    import ujson as json
    is_ujson=True
except:
    import json
    is_ujson=False

ns={"p":"https://poets-project.org/schemas/virtual-graph-schema-v4"}

def _write_typed_data_value(type, value, name, dst):
    if value is not None:
        assert type.is_refinement_compatible(value)
        value=type.expand(value)
        if value != {}:
            value=convert_json_init_to_c_init(type, value)
            dst.write(f' {name}="{value}"')

def write_edge_instance(dst,ei:EdgeInstance):
    properties=ei.properties
    state=ei.state
    send_index=ei.send_index
    if properties or (send_index is not None):
        ip=ei.dst_pin
        dst.write(f'    <EdgeI path="{ei.id}"')
        if send_index is not None:
            dst.write(f' sendIndex="{send_index}"')
        if properties:
            _write_typed_data_value(ip.properties, properties, "P", dst)
        if state:
            _write_typed_data_value(ip.state, state, "S", dst)
        dst.write("/>\n")
    else:
        dst.write(f'   <EdgeI path="{ei.id}" />\n')


def write_device_instance(dst,di:DeviceInstance):
    properties=di.properties
    state=di.state

    dt=di.device_type
    if properties or state:
        dst.write(f'   <DevI id="{di.id}" type="{dt.id}"')
        if properties:
            _write_typed_data_value(dt.properties, properties, "P", dst)
        if state:
            _write_typed_data_value(dt.state, state, "S", dst)
        dst.write(" />\n")
    else:
        dst.write('   <DevI id="{}" type="{}" />\n'.format(di.id,dt.id))



def save_graph(graph,dst):
    if isinstance(dst, str):
        if dst.endswith(".gz"):
            import gzip
            # TODO: this does compression on this thread, slowing things down. Spawn another process?
            # Compress to level 6, as there is not much difference with 9, and it is a fair bit faster
            with gzip.open(dst, 'wt', compresslevel=6) as dstFile:
                save_graph(graph,dstFile)
        else:
            with open(dst,"wt") as dstFile:
                assert not isinstance(dstFile,str)
                save_graph(graph,dstFile)
    else:
        assert not isinstance(dst,str)
        ns="https://poets-project.org/schemas/virtual-graph-schema-v4"
        nsmap = { None : ns }
        root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)

        sys.stderr.write("save_graph: Constructing graph type tree\n")
        graphTypeNode=save_graph_type(root,graph.graph_type)

        graphTypeText=etree.tostring(graphTypeNode,pretty_print=True).decode("utf8")

        dst.write("<?xml version='1.0'?>\n")
        dst.write('<Graphs xmlns="{}" formatMinorVersion="0">\n'.format(ns))

        dst.write(graphTypeText)

        dst.write(' <GraphInstance id="{}" graphTypeId="{}" \n'.format(graph.id,graph.graph_type.id))
        if graph.properties:
            _write_typed_data_value(graph.graph_type.properties, graph.properties, "P", dst)
        dst.write(' >\n')
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
