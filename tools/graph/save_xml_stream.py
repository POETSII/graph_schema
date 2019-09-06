from graph.core import *
from graph.save_xml import save_graph_type,toNS

from lxml import etree

import os
import sys

try:
    import ujson as json
    is_ujson=True
except:
    import json
    is_ujson=False

ns={"p":"https://poets-project.org/schemas/virtual-graph-schema-v3"}



def write_edge_instance(dst,ei):
    properties=ei.properties
    metadata=ei.metadata
    send_index=ei.send_index
    if properties or metadata or (send_index is not None):
        if send_index is not None:
            dst.write('   <EdgeI path="{}" sendIndex="{}">'.format(ei.id, send_index))
        else:
            dst.write('   <EdgeI path="{}">'.format(ei.id))
        if properties:
            properties=json.dumps(properties)[1:-1]
            dst.write('<P>{}</P>'.format(properties))
        if metadata:
            metadata=json.dumps(metadata)[1:-1]
            dst.write('<M>{}</M>'.format(metadata))
        dst.write("</EdgeI>\n")
    else:
        dst.write('   <EdgeI path="{}" />\n'.format(ei.id))


def write_device_instance(dst,di):
    properties=di.properties
    metadata=di.metadata
    state=di.state

    if properties or metadata or state:
        dst.write('   <DevI id="{}" type="{}">'.format(di.id,di.device_type.id))
        if properties:
            try:
                properties=json.dumps(properties)
            except:
                sys.stderr.write("properties for device '{}' = {}\n".format(di.id, properties))
                raise
            properties=properties[1:-1]
            dst.write('<P>{}</P>'.format(properties))
        if state:
            try:
                state=json.dumps(state)
            except:
                sys.stderr.write("state for device '{}' = {}\n".format(di.id, state))
                raise
            state=state[1:-1]
            dst.write('<S>{}</S>'.format(state))
        if metadata:
            metadata=json.dumps(metadata)[1:-1]
            dst.write('<M>{}</M>'.format(metadata))
        dst.write("</DevI>\n")
    else:
        dst.write('   <DevI id="{}" type="{}" />\n'.format(di.id,di.device_type.id))

import graph.save_xml_stream_v4 as _save_xml_v4

def save_graph(graph,dst):
    version=None
    if "GRAPH_SCHEMA_DEFAULT_XML_OUTPUT_VERSION" in os.environ:
        version=int(os.environ["GRAPH_SCHEMA_DEFAULT_XML_OUTPUT_VERSION"])
    if version is None:
        version=3

    sys.stderr.write(f"version={version}\n")
    
    if version==4:
        _save_xml_v4.save_graph(graph,dst)
        return
    assert version==3

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
        ns="https://poets-project.org/schemas/virtual-graph-schema-v3"
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
            if is_ujson:
                dst.write(json.dumps(graph.properties)[1:-1])
            else:
                dst.write(json.dumps(graph.properties,indent=2)[1:-1])
            dst.write('\n')
            dst.write('   </Properties>\n')
        if graph.metadata:
            dst.write('   <MetaData>\n')
            if is_ujson:
                dst.write(json.dumps(graph.metadata)[1:-1])
            else:
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
