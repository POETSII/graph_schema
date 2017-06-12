from graph.core import *

import xml.etree.ElementTree as ET
from lxml import etree

import os
import sys
import json

ns={"p":"http://TODO.org/POETS/virtual-graph-schema-v1"}

def deNS(t):
    tt=t.replace("{"+ns["p"]+"}","p:")
    #print("{} -> {}".format(t,tt))
    return tt


class XMLSyntaxError(Exception):
    def __init__(self,msg,node,outer=None):
        if node==None:
            Exception.__init__(self, "Parse error at line <unknown> : {}".format(msg))
        else:
            Exception.__init__(self, "Parse error at line {} : {} in element {}".format(node.sourceline,msg,str(node)))
        self.node=node
        self.outer=outer


def get_attrib(node,name):
    if name not in node.attrib:
        raise XMLSyntaxError("No attribute called {}".format(name), node)
    return node.attrib[name]

def get_attrib_optional(node,name):
    if name not in node.attrib:
        return None
    return node.attrib[name]

def get_attrib_defaulted(node,name,default):
    if name not in node.attrib:
        return default
    return node.attrib[name]

def get_child_text(node,name):
    n=node.find(name,ns)
    if n is None:
        raise XMLSyntaxError("No child text node called {}".format(name),node)
    text=n.text
    line=n.sourceline
    return (text,line)


def load_typed_data_spec(dt):
    name=get_attrib(dt,"name")

    tag=deNS(dt.tag)
    if tag=="p:Tuple":
        elts=[]
        for eltNode in dt.findall("p:*",ns): # Anything from this namespace must be a member
            elt=load_typed_data_spec(eltNode)
            elts.append(elt)
        return TupleTypedDataSpec(name,elts)
    elif tag=="p:Array":
        length=int(get_attrib(dt,"length"))
        type=get_attrib_optional(dt, "type")
        if type:
            type=ScalarTypedDataSpec("_",type)
        else:
            raise RuntimeError("Haven't implemented arrays of non-scalar types yet.")
        return ArrayTypedDataSpec(name,length,type)
    elif tag=="p:Scalar":
        type=get_attrib(dt, "type")
        default=get_attrib_optional(dt, "default")
        return ScalarTypedDataSpec(name,type,default)
    else:
        raise XMLSyntaxError("Unknown data type '{}'.".format(tag), dt)

def load_typed_data_instance(dt,spec):
    if dt is None:
        return None
    value=json.loads(dt.text)
    assert(spec.is_refinement_compatible(value))
    return spec.expand(value)


def load_struct_spec(name, members):
    elts=[]
    for eltNode in members.findall("p:*",ns): # Anything from this namespace must be a member
        elt=load_typed_data_spec(eltNode)
        elts.append(elt)
    return TupleTypedDataSpec(name, elts)

def load_struct_instance(spec,dt):
    if dt is None:
        return None
    # Content is just a JSON dictionary, without the surrounding brackets
    text="{"+dt.text+"}"
    value=json.loads(text)
    assert(spec.is_refinement_compatible(value))
    return spec.expand(value)

def load_metadata(parent, name):
    metadata={}
    metadataNode=parent.find(name,ns)
    if metadataNode is not None and metadataNode.text is not None:
        metadata=json.loads("{"+metadataNode.text+"}")

    return metadata


def load_message_type(parent,dt):
    id=get_attrib(dt,"id")
    try:
        message=None
        messageNode=dt.find("p:Message",ns)
        if messageNode is not None:
            message=load_struct_spec(id+"_message", messageNode)

        metadata=load_metadata(dt, "p:MetaData")

        return MessageType(parent,id,message,metadata)
    except XMLSyntaxError:
            raise
    except Exception as e:
        raise XMLSyntaxError("Error while parsing message {}".format(id),dt,e)


def load_device_type(graph,dtNode,sourceFile):
    id=get_attrib(dtNode,"id")

    state=None
    stateNode=dtNode.find("p:State",ns)
    if stateNode is not None:
        state=load_struct_spec(id+"_state", stateNode)

    properties=None
    propertiesNode=dtNode.find("p:Properties",ns)
    if propertiesNode is not None:
        properties=load_struct_spec(id+"_properties", propertiesNode)

    metadata=load_metadata(dtNode,"p:MetaData")

    shared_code=[]
    for s in dtNode.findall("p:SharedCode",ns):
        shared_code.append(s.text)

    dt=DeviceType(graph,id,properties,state,metadata,shared_code)

    for p in dtNode.findall("p:InputPort",ns):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        message_type=graph.message_types[message_type_id]

        try:
            state=None
            stateNode=p.find("p:State",ns)
            if stateNode is not None:
                state=load_struct_spec(id+"_state", stateNode)
        except Exception as e:
            raise XMLSyntaxError("Error while parsing state of pin {} : {}".format(id,e),p,e)

        try:
            properties=None
            propertiesNode=p.find("p:Properties",ns)
            if propertiesNode is not None:
                properties=load_struct_spec(id+"_properties", propertiesNode)
        except Exception as e:
            raise XMLSyntaxError("Error while parsing properties of pin {} : {}".format(id,e),p,e)

        pinMetadata=load_metadata(p,"p:MetaData")

        (handler,sourceLine)=get_child_text(p,"p:OnReceive")
        dt.add_input(name,message_type,properties,state,pinMetadata, handler,sourceFile,sourceLine)
        sys.stderr.write("      Added input {}\n".format(name))

    for p in dtNode.findall("p:OutputPort",ns):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        message_type=graph.message_types[message_type_id]
        pinMetadata=load_metadata(p,"p:MetaData")
        (handler,sourceLine)=get_child_text(p,"p:OnSend")
        dt.add_output(name,message_type,pinMetadata,handler,sourceFile,sourceLine)

    (handler,sourceLine)=get_child_text(dtNode,"p:ReadyToSend")
    dt.ready_to_send_handler=handler
    dt.ready_to_send_source_line=sourceLine
    dt.ready_to_send_source_file=sourceFile

    return dt

def load_graph_type(graphNode, sourcePath):
    id=get_attrib(graphNode,"id")
    sys.stderr.write("  Loading graph type {}\n".format(id))

    properties=None
    propertiesNode=graphNode.find("p:Properties",ns)
    if propertiesNode is not None:
        properties=load_struct_spec(id+"_properties", propertiesNode)

    metadata=load_metadata(graphNode,"p:MetaData")

    shared_code=[]
    for n in graphNode.findall("p:SharedCode",ns):
        shared_code.append(n.text)

    graphType=GraphType(id,properties,metadata,shared_code)

    for etNode in graphNode.findall("p:MessageTypes/p:*",ns):
        et=load_message_type(graphType,etNode)
        graphType.add_message_type(et)

    for dtNode in graphNode.findall("p:DeviceTypes/p:*",ns):
        dt=load_device_type(graphType,dtNode, sourcePath)
        graphType.add_device_type(dt)
        sys.stderr.write("    Added device type {}\n".format(dt.id))

    return graphType

def load_graph_type_reference(graphNode,basePath):
    id=get_attrib(graphNode,"id")

    src=get_attrib_optional(graphNode, "src")
    if src:
        fullSrc=os.path.join(basePath, src)
        print("  basePath = {}, src = {}, fullPath = {}".format(basePath,src,fullSrc))

        tree = etree.parse(fullSrc)
        doc = tree.getroot()
        graphsNode = doc;

        for gtNode in graphsNode.findall("p:GraphType",ns):
            gt=load_graph_type(gtNode, fullSrc)
            if gt.id==id:
                return gt

        raise XMLSyntaxError("Couldn't load graph type '{}' from src '{}'".format(id,src))
    else:
        return GraphTypeReference(id)


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

    properties=None
    propertiesNode=diNode.find("p:P",ns)
    if propertiesNode is not None:
        properties=load_struct_instance(device_type.properties, propertiesNode)

    metadata=load_metadata(diNode,"p:M")

    return DeviceInstance(graph,id,device_type,properties,metadata)

def split_endpoint(endpoint,node):
    parts=endpoint.split(':')
    if len(parts)!=2:
        raise XMLSyntaxError("Path does not contain exactly two elements",node)
    return (parts[0],parts[1])


def split_path(path,node):
    """Splits a path up into (dstDevice,dstPort,srcDevice,srcPort)"""
    parts=path.split('-')
    if len(parts)!=2:
        raise XMLSyntaxError("Path does not contain exactly two endpoints",node)
    return split_endpoint(parts[0],node)+split_endpoint(parts[1],node)

def load_edge_instance(graph,eiNode):
    path=get_attrib_optional(eiNode,"path")
    if path:
        (dst_device_id,dst_port_name,src_device_id,src_port_name)=split_path(path,eiNode)
    else:
        dst_device_id=get_attrib(eiNode,"dstDeviceId")
        dst_port_name=get_attrib(eiNode,"dstPortName")
        src_device_id=get_attrib(eiNode,"srcDeviceId")
        src_port_name=get_attrib(eiNode,"srcPortName")
    
    dst_device=graph.device_instances[dst_device_id]
    src_device=graph.device_instances[src_device_id]
    
    assert dst_port_name in dst_device.device_type.inputs, "Couldn't find input port called '{}' in device type '{}'. Inputs are [{}]".format(dst_port_name,dst_device.device_type.id, [p.name for p in dst_device.device_type.inputs])
    assert src_port_name in src_device.device_type.outputs
    

    properties=None
    propertiesNode=eiNode.find("p:P",ns)
    if propertiesNode is not None:
        spec=dst_device.device_type.inputs[dst_port_name].properties
        properties=load_struct_instance(spec, propertiesNode)

    metadata=load_metadata(eiNode,"p:M")

    return EdgeInstance(graph,dst_device,dst_port_name,src_device,src_port_name,properties,metadata)

def load_graph_instance(graphTypes, graphNode):
    id=get_attrib(graphNode,"id")
    graphTypeId=get_attrib(graphNode,"graphTypeId")
    graphType=graphTypes[graphTypeId]

    properties=None
    propertiesNode=graphNode.find("p:Properties",ns)
    if propertiesNode is not None:
        assert graphType.properties
        properties=load_struct_instance(graphType.properties, propertiesNode)

    metadata=load_metadata(graphNode, "p:MetaData")

    graph=GraphInstance(id,graphType,properties,metadata)

    for diNode in graphNode.findall("p:DeviceInstances/p:DevI",ns):
        di=load_device_instance(graph,diNode)
        graph.add_device_instance(di)

    for eiNode in graphNode.findall("p:EdgeInstances/p:EdgeI",ns):
        ei=load_edge_instance(graph,eiNode)
        graph.add_edge_instance(ei)

    return graph


def load_graph_types_and_instances(src,basePath):

    tree = etree.parse(src)
    doc = tree.getroot()
    graphsNode = doc;

    graphTypes={}
    graphs={}

    try:
        for gtNode in graphsNode.findall("p:GraphType",ns):
            sys.stderr.write("Loading graph type\n")
            gt=load_graph_type(gtNode, basePath)
            graphTypes[gt.id]=gt

        for gtRefNode in graphsNode.findall("p:GraphTypeReference",ns):
            sys.stderr.write("Loading graph reference\n")
            gt=load_graph_type_reference(gtRefNode,basePath)
            graphTypes[gt.id]=gt

        for giNode in graphsNode.findall("p:GraphInstance",ns):
            sys.stderr.write("Loading graph\n")
            g=load_graph_instance(graphTypes, giNode)
            graphs[g.id]=g

        return (graphTypes,graphs)

    except XMLSyntaxError as e:
        sys.stderr.write(str(e)+"\n")
        if e.node is not None:
            sys.stderr.write(etree.tostring(e.node, pretty_print = True, encoding='utf-8').decode("utf-8")+"\n")
        raise e

def load_graph(src,basePath):
    (graphTypes,graphInstances)=load_graph_types_and_instances(src,basePath)
    if len(graphInstances)==0:
        raise RuntimeError("File contained no graph instances.")
    if len(graphInstances)>1:
        raise RuntimeError("File contained more than one graph instance.")
    for x in graphInstances.values():
        return x

