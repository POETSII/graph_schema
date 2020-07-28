from graph.core import *

import xml.etree.ElementTree as ET
from lxml import etree

v4_namespace_uri = "https://poets-project.org/schemas/virtual-graph-schema-v4"

from graph.load_xml_v3 import XMLSyntaxError, get_attrib, \
    get_attrib_defaulted, get_attrib_optional, get_attrib_optional_bool, \
    get_child_text

from graph.parse_v4_structs import parse_struct_def_string, parse_struct_init_string, convert_def_to_typed_data_spec

import re
import os
import sys
import json
from typing import *

ns={"p":v4_namespace_uri}

# Precalculate, as these are on inner loop for (DevI|ExtI) and EdgeI
_ns_DevI="{{{}}}DevI".format(ns["p"])
_ns_ExtI="{{{}}}ExtI".format(ns["p"])
_ns_EdgeI="{{{}}}EdgeI".format(ns["p"])

# Precalculate for parsing of the device types (DeviceType | ExternalType )
_ns_DeviceType="{{{}}}DeviceType".format(ns["p"])
_ns_ExternalType="{{{}}}ExternalType".format(ns["p"])

_ns_brackets="{"+v4_namespace_uri+"}"
def deNS(t):
    return t.replace(_ns_brackets,"p:")

def load_struct_spec(parentElt, eltName):
    n=parentElt.find(eltName, ns)
    if n==None:
        raise XMLSyntaxError(f"Missing struct spec node {eltName}", parentElt)
    src=n.text
    df=parse_struct_def_string(src)
    return convert_def_to_typed_data_spec(df)

def load_struct_instance(spec, elt, attr):
    val=get_attrib_optional(elt, attr)
    if val==None:
        return None
    init=parse_struct_init_string(val)
    value=spec.convert_v4_init(init)
    assert(spec.is_refinement_compatible(value))
    return spec.expand(value)


def load_message_type(parent,mtElt):
    id=get_attrib(mtElt,"id")
    try:
        message=load_struct_spec(mtElt, "p:Message")
        return MessageType(parent,id,message,)
    except XMLSyntaxError:
        raise
    except Exception as e:
        raise XMLSyntaxError("Error while parsing message type {}".format(id),mtElt,e)

def load_external_type(graph,dtNode,sourceFile):
    id=get_attrib(dtNode,"id")
    state=None
    properties=load_struct_spec(dtNode, "p:Properties")
    shared_code=[]
    metadata=None
    documentation=None
    dt=DeviceType(graph,id,properties,state,metadata,shared_code, True, documentation)

    for p in dtNode.findall("p:InputPin", ns):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        message_type=graph.message_types[message_type_id]
        state=None # pins of ExternalType cannot have any state, properties, or handlers
        properties=None
        handler=''
        is_application=False # Legacy: must be false, eventually will be removed
        sourceLine=0
        pinMetadata=None

        dt.add_input(name,message_type,is_application,properties,state,pinMetadata, handler,sourceFile,sourceLine)
        sys.stderr.write("      Added external input {}\n".format(name))

    for p in dtNode.findall("p:OutputPin", ns):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        message_type=graph.message_types[message_type_id]
        is_application=False  # Legacy: must be false, eventually will be removed
        handler=''
        sourceLine=0
        pinMetadata=None

        dt.add_output(name,message_type,is_application,pinMetadata,handler,sourceFile,sourceLine)
        sys.stderr.write("      Added external output {}\n".format(name))

    dt.ready_to_send_handler=''
    dt.ready_to_send_source_line=0
    dt.ready_to_send_source_file=None

    return dt

def load_device_type(graph,dtNode,sourceFile):
    id=get_attrib(dtNode,"id")

    try:

        properties=load_struct_spec(dtNode, "p:Properties")
        state=load_struct_spec(dtNode, "p:State")

        shared_code=[]
        tt=get_child_text(dtNode, "p:SharedCode", ns)[0]
        if tt is not None:
            shared_code.append(tt)
        metadata=None
        documentation=None
        dt=DeviceType(graph,id,properties,state,metadata,shared_code,isExternal=False,documentation=documentation)

        for p in dtNode.findall("p:InputPin",ns):
            name=get_attrib(p,"name")
            message_type_id=get_attrib(p,"messageTypeId")
            if message_type_id not in graph.message_types:
                raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
            message_type=graph.message_types[message_type_id]
            # NOTE: application pin support needed for as long as 2to3 is relevant.
            is_application=get_attrib_optional_bool(p,"application") # TODO: REMOVE APPLICATION PIN
            properties=load_struct_spec(p, "p:Properties")
            state=load_struct_spec(p, "p:State")
            pinMetadata=None
            documentation=None
            
            (handler,sourceLine)=get_child_text(p,"p:OnReceive",ns)
            dt.add_input(name,message_type,is_application,properties,state,pinMetadata, handler,sourceFile,sourceLine,documentation)
            #sys.stderr.write("      Added input {}\n".format(name))
            

        for p in dtNode.findall("p:OutputPin",ns):
            name=get_attrib(p,"name")
            message_type_id=get_attrib(p,"messageTypeId")
            if message_type_id not in graph.message_types:
                raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
            is_application=False
            is_indexed=get_attrib_optional_bool(p,"indexed")
            message_type=graph.message_types[message_type_id]
            pinMetadata=None
            (handler,sourceLine)=get_child_text(p,"p:OnSend",ns)
            documentation = None
            dt.add_output(name,message_type,is_application,pinMetadata,handler,sourceFile,sourceLine,documentation,is_indexed)
            #sys.stderr.write("      Added input {}\n".format(name))

        (handler,sourceLine)=get_child_text(dtNode,"p:ReadyToSend",ns)
        dt.ready_to_send_handler=handler
        dt.ready_to_send_source_line=sourceLine
        dt.ready_to_send_source_file=sourceFile

        (handler,sourceLine)=get_child_text(dtNode,"p:OnInit", ns)
        dt.init_handler=handler
        dt.init_source_line=sourceLine
        dt.init_source_file=sourceFile

        (handler,sourceLine)=get_child_text(dtNode,"p:OnHardwareIdle",ns)
        dt.on_hardware_idle_handler=handler
        dt.on_hardware_idle_source_line=sourceLine
        dt.on_hardware_idle_source_file=sourceFile

        (handler,sourceLine)=get_child_text(dtNode,"p:OnDeviceIdle",ns)
        dt.on_device_idle_handler=handler
        dt.on_device_idle_source_line=sourceLine
        dt.on_device_idle_source_file=sourceFile

        return dt
    except:
        sys.stderr.write(f"Exception while loading device type {id}\n")
        raise

def load_graph_type(graphNode, sourcePath):
    deviceTypeTag = "{{{}}}DeviceType".format(ns["p"])
    externalTypeTag = "{{{}}}ExternalType".format(ns["p"])

    id=get_attrib(graphNode,"id")
    sys.stderr.write("  Loading graph type {}\n".format(id))

    properties=load_struct_spec(graphNode, "p:Properties")
    metadata=None
    documentation=None

    shared_code=[]
    tt=get_child_text(graphNode, "p:SharedCode", ns)[0]
    if tt is not None:
        shared_code.append(tt)
    graphType=GraphType(id,properties,metadata,shared_code,documentation)

    for etNode in graphNode.findall("p:MessageTypes/p:*",ns):
        et=load_message_type(graphType,etNode)
        graphType.add_message_type(et)

    for dtNode in graphNode.findall("p:DeviceTypes/p:*",ns):

        if dtNode.tag == deviceTypeTag:
            dt=load_device_type(graphType, dtNode, sourcePath)
            graphType.add_device_type(dt)
            #sys.stderr.write("    Added device type {}\n".format(dt.id))
        elif dtNode.tag == externalTypeTag:
            et=load_external_type(graphType,dtNode,sourcePath)
            graphType.add_device_type(et)
            #sys.stderr.write("    Added external device type {}\n".format(et.id))
        else:
            raise RuntimeError(f"Unknown or unsupported element in DeviceTypes: {dtNode.tag}")

    return graphType

def load_external_instance(graph, eiNode):
    
    id=get_attrib(eiNode,"id")
    external_type_id=get_attrib(eiNode,"type")
    if external_type_id not in graph.graph_type.device_types:
        raise XMLSyntaxError("Unknown external type id {}, known devices = [{}]".format(external_type_id, [d.di for d in graph.graph_type.deivce_types.keys()]), eiNode)
    external_type=graph.graph_type.device_types[external_type_id]

    properties=load_struct_instance(external_type.properties, eiNode, "P")
    metadata=None

    return DeviceInstance(graph,id,external_type,properties,metadata)

def load_device_instance(graph,diNode):

    id=get_attrib(diNode,"id")
    device_type_id=get_attrib(diNode,"type")
    if device_type_id not in graph.graph_type.device_types:
        raise XMLSyntaxError("Unknown device type id {}, known devices = [{}]".format(device_type_id,
                        [d.id for d in graph.graph_type.device_types.keys()]
                    ),
                    diNode
                )
    device_type=graph.graph_type.device_types[device_type_id]

    properties=load_struct_instance(device_type.properties, diNode, "P")
    state=load_struct_instance(device_type.state, diNode, "S")
    state=None
    metadata=None

    return DeviceInstance(graph,id,device_type,properties,state,metadata)


_split_path_re=re.compile("^([^:]+):([^-]+)-([^:]+):([^-]+)$")

def load_edge_instance(graph,eiNode):
    path=eiNode.attrib["path"]
    (dst_device_id,dst_pin_name,src_device_id,src_pin_name)=_split_path_re.match(path).groups()

    send_index=eiNode.attrib.get("sendIndex")

    dst_device=graph.device_instances[dst_device_id]
    src_device=graph.device_instances[src_device_id]

    assert dst_pin_name in dst_device.device_type.inputs, "Couldn't find input pin called '{}' in device type '{}'. Inputs are [{}]".format(dst_pin_name,dst_device.device_type.id, [p for p in dst_device.device_type.inputs])
    assert src_pin_name in src_device.device_type.outputs

    dst_pin_type=dst_device.device_type.inputs[dst_pin_name]
    properties=load_struct_instance(dst_pin_type.properties, eiNode, "P")
    state=load_struct_instance(dst_pin_type.state, eiNode, "S")
    metadata=None
    
    return EdgeInstance(graph,dst_device,dst_pin_name,src_device,src_pin_name,properties,metadata, send_index, state=state)

def load_graph_instance(graphTypes, graphNode):
    devITag = "{{{}}}DevI".format(ns["p"])
    extITag = "{{{}}}ExtI".format(ns["p"])
    edgeITag = "{{{}}}EdgeI".format(ns["p"])

    id=get_attrib(graphNode,"id")
    graphTypeId=get_attrib(graphNode,"graphTypeId")
    graphType=graphTypes[graphTypeId]

    properties=load_struct_instance(graphType.properties, graphNode, "P")
    metadata=None # TODO: Load metadata
    documentation=None

    graph=GraphInstance(id,graphType,properties,metadata, documentation)
    disNode=graphNode.findall("p:DeviceInstances",ns)
    assert(len(disNode)==1)
    for diNode in disNode[0]:
        assert (diNode.tag==devITag) or (diNode.tag==extITag)
        if diNode.tag==devITag:
            di=load_device_instance(graph,diNode)
            graph.add_device_instance(di)
        elif diNode.tag==extITag:
            ei=load_external_instance(graph,diNode)
            graph.add_device_instance(ei)

    eisNode=graphNode.findall("p:EdgeInstances",ns)
    assert(len(eisNode)==1)
    for eiNode in eisNode[0]:
        assert eiNode.tag==edgeITag
        ei=load_edge_instance(graph,eiNode)
        graph.add_edge_instance(ei)

    return graph


def v4_load_graph_types_and_instances(doc : etree.Element , basePath:str) -> (GraphType, Optional[GraphInstance]):
    graphsNode = doc

    graphType=None
    graphInst=None

    try:
        for gtNode in graphsNode.findall("p:GraphType",ns):
            sys.stderr.write("Loading graph type\n")
            assert graphType==None
            graphType=load_graph_type(gtNode, basePath)

        assert graphType, "No GraphType element."

        graphTypes={graphType.id : graphType}

        for giNode in graphsNode.findall("p:GraphInstance",ns):
            sys.stderr.write("Loading graph instance\n")
            assert graphInst is None
            graphInst=load_graph_instance(graphTypes, giNode)
            
        return (graphType,graphInst)

    except XMLSyntaxError as e:
        sys.stderr.write(str(e)+"\n")
        if e.node is not None:
            sys.stderr.write(etree.tostring(e.node, pretty_print = True, encoding='utf-8').decode("utf-8")+"\n")
        raise e

