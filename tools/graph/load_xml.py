from graph.core import *

import xml.etree.ElementTree as ET
from lxml import etree

import os
import sys

ns={"p":"TODO/POETS/virtual-graph-schema-v0"}

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

def get_child_text(node,name):
    n=node.find(name,ns)
    if n is None:
        raise XMLSyntaxError("No child text node called {}".format(name),node)
    return n.text


def load_typed_data(dt):
    name=get_attrib(dt,"name")
    
    tag=deNS(dt.tag)
    if tag=="p:Tuple":
        elts=[]
        for eltNode in dt.findall("p:*",ns): # Anything from this namespace must be a member
            elt=load_typed_data(eltNode)
            elts.append(elt)
        return TupleData(name,elts)

    # Must be a scalar
    value=get_attrib_optional(dt,"value")
    
    if tag=="p:Int32":
        return Int32Data(name,value)
    elif tag=="p:Float32":
        return Float32Data(name,value)
    elif tag=="p:Bool":
        return BoolData(name,value)
    else:
        raise XMLSyntaxError("Unknown data type.",dt)

    
def load_edge_type(parent,dt):
    id=get_attrib(dt,"id")
    try:    
    
        message=None
        messageNode=dt.find("p:Message",ns)
        if messageNode is not None:
            assert(len(messageNode)==1)
            message=load_typed_data(messageNode[0])

        state=None
        stateNode=dt.find("p:State",ns)
        if stateNode is not None:
            assert(len(stateNode)==1)
            tate=load_typed_data(stateNode[0])

        properties=None
        propertiesNode=dt.find("p:Properties",ns)
        if propertiesNode is not None:
            assert(len(propertiesNode)==1)
            properties=load_typed_data(propertiesNode[0])
            
        return EdgeType(parent,id,message,state,properties)
    except Exception as e:
        raise XMLSyntaxError("Error while parsing edge {}".format(id),dt,e)


def load_device_type(graph,dtNode):
    id=get_attrib(dtNode,"id")

    state=None
    stateNode=dtNode.find("p:State",ns)
    if stateNode is not None:
        assert(len(stateNode)==1)
        state=load_typed_data(stateNode[0])

    properties=None
    propertiesNode=dtNode.find("p:Properties",ns)
    if propertiesNode is not None:
        assert(len(propertiesNode)==1)
        properties=load_typed_data(propertiesNode[0])

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


def load_device_instance(graph,diNode):
    id=get_attrib(diNode,"id")
    
    device_type_id=get_attrib(diNode,"deviceTypeId")
    if device_type_id not in graph.device_types:
        raise XMLSyntaxError("Unknown device type id {}".format(device_type_id))
    device_type=graph.device_types[device_type_id]

    properties=None
    propertiesNode=diNode.find("p:Properties",ns)
    if propertiesNode is not None:
        assert(len(propertiesNode)==1)
        properties=load_typed_data(propertiesNode[0])

    return DeviceInstance(graph,id,device_type,properties)
    

def load_edge_instance(graph,eiNode):
    dst_device_id=get_attrib(eiNode,"dstDeviceId")
    dst_port_name=get_attrib(eiNode,"dstPortName")
    src_device_id=get_attrib(eiNode,"srcDeviceId")
    src_port_name=get_attrib(eiNode,"srcPortName")

    dst_device=graph.device_instances[dst_device_id]
    src_device=graph.device_instances[src_device_id]

    properties=None
    propertiesNode=eiNode.find("p:Properties",ns)
    if propertiesNode is not None:
        assert(len(propertiesNode)==1)
        properties=load_typed_data(propertiesNode[0])

    return EdgeInstance(graph,dst_device,dst_port_name,src_device,src_port_name,properties)


def load_graph(src):
    tree = etree.parse(os.sys.stdin)
    doc = tree.getroot()
    graphNode = doc;

    try:
        id=get_attrib(graphNode,"id")

        graph=Graph(id)

        properties=None
        propertiesNode=graphNode.find("p:Properties",ns)
        if propertiesNode is not None:
            assert(len(propertiesNode)==1)
            properties=load_typed_data(propertiesNode[0])

        for etNode in graphNode.findall("p:EdgeTypes/p:*",ns):
            et=load_edge_type(graph,etNode)
            graph.add_edge_type(et)

        for dtNode in graphNode.findall("p:DeviceTypes/p:*",ns):
            dt=load_device_type(graph,dtNode)
            graph.add_device_type(dt)

        for diNode in graphNode.findall("p:DeviceInstances/p:*",ns):
            di=load_device_instance(graph,diNode)
            graph.add_device_instance(di)

        for eiNode in graphNode.findall("p:EdgeInstances/p:*",ns):
            ei=load_edge_instance(graph,eiNode)
            graph.add_edge_instance(ei)

        return graph
    except XMLSyntaxError as e:
        print(e)
        if e.node is not None:
            print(etree.tostring(e.node, pretty_print = True, encoding='utf-8').decode("utf-8"))
        raise e
