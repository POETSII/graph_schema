from graph.core import *

#from xml.etree import ElementTree as etree
from lxml import etree

import os
import sys

ns={"p":"TODO/POETS/virtual-graph-schema-v0"}

def toNS(t):
    tt=t.replace("p:","{"+ns["p"]+"}")
    return tt

_type_to_tag={

    TupleData : toNS("p:Tuple"),
    Int32Data : toNS("p:Int32"),
    Float32Data : toNS("p:Float32"),
    BoolData : toNS("p:Bool"),

    GraphType : toNS("p:GraphType"),
    EdgeType : toNS("p:EdgeType"),
    DeviceType : toNS("p:DeviceType"),
    InputPort : toNS("p:InputPort"),
    OutputPort : toNS("p:OutputPort"),
    GraphInstance : toNS("p:GraphInstance"),
    EdgeInstance : toNS("p:EdgeInstance"),
    DeviceInstance : toNS("p:DeviceInstance")
}

def _type_to_element(t):
    tag=_type_to_tag[type(t)]
    return etree.Element(tag)

def save_typed_data(dt):
    n=_type_to_element(dt)
    n.attrib["name"]=dt.name
    
    if isinstance(dt,TupleData):
        for e in dt.elements_by_index:
            n.append(save_typed_data(e))
    elif isinstance(dt,ScalarData):
        if dt.value is not None:
            n.attrib["value"]=str(dt.value)
    else:
        raise RuntimeError("Unknown data type.")

    return n

def attach_typed_data(node,childType,data):
    if data is None:
        return None
    
    r=etree.Element(toNS(childType))
    r.append(save_typed_data(data))
    node.append(r)
    return r
    
def save_edge_type(et):
    n=_type_to_element(et)

    n.attrib["id"]=et.id
    attach_typed_data(n, "p:Message", et.message)
    attach_typed_data(n, "p:Properties", et.properties)
    attach_typed_data(n, "p:State", et.state)

    return n


def save_device_type(dt):
    n=_type_to_element(dt)

    n.attrib["id"]=dt.id
    attach_typed_data(n, "p:Properties", dt.properties)
    attach_typed_data(n, "p:State", dt.state)
        
    for p in dt.inputs_by_index:
        pn=_type_to_element(p)
        pn.attrib["name"]=p.name
        pn.attrib["edgeTypeId"]=p.edge_type.id
        h=etree.Element(toNS("p:OnReceive"))
        h.text = etree.CDATA(p.receive_handler)
        #h.txt = p.received_handler
        pn.append(h)
        n.append(pn)

    for p in dt.outputs_by_index:
        pn=_type_to_element(p)
        pn.attrib["name"]=p.name
        pn.attrib["edgeTypeId"]=p.edge_type.id
        h=etree.Element(toNS("p:OnSend"))
        h.text = etree.CDATA(p.send_handler)
        #h.text = p.send_handler
        pn.append(h)
        n.append(pn)

    return n     


def save_device_instance(di):
    n=_type_to_element(di)

    n.attrib["id"]=di.id
    n.attrib["deviceTypeId"]=di.device_type.id
    attach_typed_data(n, "p:Properties", di.properties)

    return n


def save_edge_instance(ei):
    n=_type_to_element(ei)

    n.attrib["dstDeviceId"]=ei.dst_device.id
    n.attrib["dstPortName"]=ei.dst_port.name
    n.attrib["srcDeviceId"]=ei.src_device.id
    n.attrib["srcPortName"]=ei.src_port.name
    
    attach_typed_data(n, "p:Properties", ei.properties)

    return n


def save_graph_type(graph):
    gn = _type_to_element(graph);
    gn.attrib["id"]=graph.id

    etn = etree.Element(toNS("p:EdgeTypes"))
    gn.append(etn)
    for et in graph.edge_types.values():
        etn.append(save_edge_type(et))

    dtn = etree.Element(toNS("p:DeviceTypes"))
    gn.append(dtn)
    for dt in graph.device_types.values():
        dtn.append(save_device_type(dt))

    return gn

def save_graph_instance(graph):
    gn = _type_to_element(graph);
    gn.attrib["id"]=graph.id
    gn.attrib["graphTypeId"]=graph.graph_type.id
    
    din = etree.Element(toNS("p:DeviceInstances"))
    gn.append(din)
    for di in graph.device_instances.values():
        din.append(save_device_instance(di))

    ein = etree.Element(toNS("p:EdgeInstances"))
    gn.append(ein)
    for ei in graph.edge_instances.values():
        ein.append(save_edge_instance(ei))

    return gn

def save_graph(graph,dst):
    root=etree.Element(toNS("p:Graph"))
    
    root.append(save_graph_type(graph.graph_type))
    root.append(save_graph_instance(graph))

    # The wierdness is because stdout is in text mode, so we send
    # it via a string. Ideally it would write it straight to the file...
    tree = etree.ElementTree(root)
    s=etree.tostring(root,pretty_print=True).decode("utf8")
    dst.write(s)
