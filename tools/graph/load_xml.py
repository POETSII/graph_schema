from graph.core import *

import xml.etree.ElementTree as ET
from lxml import etree

import os
import sys
import json

ns={"p":"http://TODO.org/POETS/virtual-graph-schema-v0"}

def deNS(t):
    tt=t.replace("{"+ns["p"]+"}","p:")
    #print("{} -> {}".format(t,tt))
    return tt


class XMLSyntaxError(Exception):
    def __init__(self,msg,node,outer=None):
        if node==None:
            Exception.__init__(self, "Parse error at line <unknown> : {}".format(msg))
        else:
            Exception.__init__(self, "Parse error at line {} : {}".format(node.sourceline,msg))
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
    return n.text


def load_typed_data_spec(dt):
    name=get_attrib(dt,"name")

    tag=deNS(dt.tag)
    if tag=="p:Tuple":
        elts=[]
        for eltNode in dt.findall("p:*",ns): # Anything from this namespace must be a member
            elt=load_typed_data_spec(eltNode)
            elts.append(elt)
        return TupleTypedDataSpec(name,elts)
    elif tag=="p:Scalar":
        type=get_attrib(dt, "type")
        value=get_attrib_optional(dt, "value")
        return ScalarTypedDataSpec(name,type,value)
    else:
        raise XMLSyntaxError("Unknown data type '{}'.".format(tag), dt)

def load_typed_data_instance(dt,spec):
    if dt is None:
        return None
    value=json.loads(dt.text)
    assert(spec.is_refinement_compatible(value))
    return value
    
    
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
    return value
    
def load_edge_type(parent,dt):
    id=get_attrib(dt,"id")
    try:    
    
        message=None
        messageNode=dt.find("p:Message",ns)
        if messageNode is not None:
            message=load_struct_spec(id+"_message", messageNode)

        state=None
        stateNode=dt.find("p:State",ns)
        if stateNode is not None:
            state=load_struct_spec(id+"_state", stateNode)

        try:
            properties=None
            propertiesNode=dt.find("p:Properties",ns)
            if propertiesNode is not None:
                properties=load_struct_spec(id+"_properties", propertiesNode)
        except Exception as e:
            raise XMLSyntaxError("Error while parsing properties of edge {} : {}".format(id,e),dt,e)
                
        return EdgeType(parent,id,message,state,properties)
    except XMLSyntaxError:
            raise
    except Exception as e:
        raise XMLSyntaxError("Error while parsing edge {}".format(id),dt,e)


def load_device_type(graph,dtNode):
    id=get_attrib(dtNode,"id")

    state=None
    stateNode=dtNode.find("p:State",ns)
    if stateNode is not None:
        state=load_struct_spec(id+"_state", stateNode)

    properties=None
    propertiesNode=dtNode.find("p:Properties",ns)
    if propertiesNode is not None:
        properties=load_struct_spec(id+"_properties", propertiesNode)

    dt=DeviceType(graph,id,state,properties)
        
    for p in dtNode.findall("p:InputPort",ns):
        name=get_attrib(p,"name")
        edge_type_id=get_attrib(p,"edgeTypeId")
        if edge_type_id not in graph.edge_types:
            raise XMLSyntaxError("Unknown edgeTypeId {}".format(edge_type_id),p)
        edge_type=graph.edge_types[edge_type_id]
        handler=get_child_text(p,"p:OnReceive")
        dt.add_input(name,edge_type,handler)

    for p in dtNode.findall("p:OutputPort",ns):
        name=get_attrib(p,"name")
        edge_type_id=get_attrib(p,"edgeTypeId")
        if edge_type_id not in graph.edge_types:
            raise XMLSyntaxError("Unknown edgeTypeId {}".format(edge_type_id),p)
        edge_type=graph.edge_types[edge_type_id]
        handler=get_child_text(p,"p:OnSend")
        dt.add_output(name,edge_type,handler)

    return dt            

def load_graph_type(graphNode):
    id=get_attrib(graphNode,"id")

    dimension=get_attrib_optional(graphNode, "nativeDimension")
    if dimension:
        dimension=int(dimension)
    else:
        dimension=0
        
    properties=None
    propertiesNode=graphNode.find("p:Properties",ns)
    if propertiesNode is not None:
        properties=load_struct_spec(id+"_properties", propertiesNode)

    graphType=GraphType(id,dimension,properties)
        
    for etNode in graphNode.findall("p:EdgeTypes/p:*",ns):
        et=load_edge_type(graphType,etNode)
        graphType.add_edge_type(et)

    for dtNode in graphNode.findall("p:DeviceTypes/p:*",ns):
        dt=load_device_type(graphType,dtNode)
        graphType.add_device_type(dt)

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
            gt=load_graph_type(gtNode)
            if gt.id==id:
                return gt

        raise XMLSyntaxError("Couldn't load graph type '{}' from src '{}'".format(id,src))
    else:
        return GraphTypeReference(id)        
        

def load_device_instance(graph,diNode):
    id=get_attrib(diNode,"id")

    nativeLocationStr=get_attrib_optional(diNode,"nativeLocation")
    if(nativeLocationStr):
        nativeLocation=[float(p) for p in nativeLocationStr.split(',')]
        if len(nativeLocation) != graph.graph_type.native_dimension:
            raise XMLSyntaxError("native location '{}' does not match graph dimension {}.".format(nativeLocationStr,graph.graph_type.native_dimension), diNode)
    else:
        nativeLocation=None
    
    device_type_id=get_attrib(diNode,"type")
    if device_type_id not in graph.graph_type.device_types:
        raise XMLSyntaxError("Unknown device type id {}".format(device_type_id))
    device_type=graph.graph_type.device_types[device_type_id]

    properties=None
    propertiesNode=diNode.find("p:Properties",ns)
    if propertiesNode is not None:
        properties=load_struct_instance(device_type.properties, propertiesNode)

    return DeviceInstance(graph,id,device_type,nativeLocation,properties)

def split_endpoint(endpoint,node):
    parts=endpoint.split(':')
    if len(parts)!=2:
        raise XMLSyntaxError("Path does not contain exactly two elements",node)
    return (parts[0],parts[1])
    

def split_path(path,node):
    """Splits a path up into (dstDevice,dstPort,srcDevice,srcPort)"""
    parts=endpoint.split('<-')
    if len(parts)!=2:
        raise XMLSyntaxError("Path does not contain exactly two endpoints",node)
    return split_endpoint(parts[0])+split_endpoint(parts[1])

def load_edge_instance(graph,eiNode):
    path=get_attrib_optional(eiNode,"path")
    if path:
        (dst_device_id,dst_port_name,src_device_id,src_port_name)=split_path(path)
    else:
        dst_device_id=get_attrib(eiNode,"dstDeviceId")
        dst_port_name=get_attrib(eiNode,"dstPortName")
        src_device_id=get_attrib(eiNode,"srcDeviceId")
        src_port_name=get_attrib(eiNode,"srcPortName")

    dst_device=graph.device_instances[dst_device_id]
    src_device=graph.device_instances[src_device_id]

    properties=None
    propertiesNode=eiNode.find("p:Properties",ns)
    if propertiesNode is not None:
        spec=dst_device.device_type.inputs[dst_port_name].properties
        properties=load_struct_instance(spec, propertiesNode[0])

    return EdgeInstance(graph,dst_device,dst_port_name,src_device,src_port_name,properties)

def load_graph_instance(graphTypes, graphNode):
    id=get_attrib(graphNode,"id")
    graphTypeId=get_attrib(graphNode,"graphTypeId")

    graph=GraphInstance(id,graphTypes[graphTypeId])

    properties=None
    propertiesNode=graphNode.find("p:Properties",ns)
    if propertiesNode is not None:
        assert(len(propertiesNode)==1)
        properties=load_struct_instance(graph.properties, propertiesNode[0])

    for diNode in graphNode.findall("p:DevI/p:*",ns):
        di=load_device_instance(graph,diNode)
        graph.add_device_instance(di)

    for eiNode in graphNode.findall("p:EdgeI/p:*",ns):
        ei=load_edge_instance(graph,eiNode)
        graph.add_edge_instance(ei)

    return graph


def load_graph_types_and_instances(src,basePath=None):
    if basePath==None:
        if isinstance(src,str):
            basePath=os.path.dirname(src)
        else:
            basePath=os.getcwd()
    
    tree = etree.parse(src)
    doc = tree.getroot()
    graphsNode = doc;

    graphTypes={}
    graphs={}
    
    try:
        for gtNode in graphsNode.findall("p:GraphType",ns):
            sys.stderr.write("Loading graph type")
            gt=load_graph_type(gtNode)
            graphTypes[gt.id]=gt

        for gtRefNode in graphsNode.findall("p:GraphTypeReference",ns):
            sys.stderr.write("Loading graph reference")
            gt=load_graph_type_reference(gtRefNode,basePath)
            graphTypes[gt.id]=gt
            
        for giNode in graphsNode.findall("p:GraphInstance",ns): 
            sys.stderr.write("Loading graph")
            g=load_graph_instance(graphTypes, giNode)
            graphs[g.id]=g

        return (graphTypes,graphs)

    except XMLSyntaxError as e:
        print(e)
        if e.node is not None:
            print(etree.tostring(e.node, pretty_print = True, encoding='utf-8').decode("utf-8"))
        raise e

def load_graph(src,basePath=None):
    (graphTypes,graphInstances)=load_graph_types_and_instances(src,basePath)
    if len(graphInstances)==0:
        raise RuntimeError("File contained no graph instances.")
    if len(graphInstances)>1:
        raise RuntimeError("File contained more than one graph instance.")
    for x in graphInstances.values():
        return x
