from graph.core import *

#from xml.etree import ElementTree as etree
from lxml import etree

import os
import sys
import json

ns={"p":"http://TODO.org/POETS/virtual-graph-schema-v0"}

def toNS(t):
    tt=t.replace("p:","{"+ns["p"]+"}")
    return tt

_type_to_tag={
    GraphType : toNS("p:GraphType"),
    EdgeType : toNS("p:EdgeType"),
    DeviceType : toNS("p:DeviceType"),
    InputPort : toNS("p:InputPort"),
    OutputPort : toNS("p:OutputPort"),
    GraphInstance : toNS("p:GraphInstance"),
    EdgeInstance : toNS("p:EdgeI"),
    DeviceInstance : toNS("p:DevI")
}

def _type_to_element(t):
    tag=_type_to_tag[type(t)]
    return etree.Element(tag)

def save_typed_data_spec(dt):    
    if isinstance(dt,TupleTypedDataSpec):
        n=etree.Element(toNS("p:Tuple"))
        n.attrib["name"]=dt.name
        for e in dt.elements_by_index:
            n.append(save_typed_data_spec(e))
    elif isinstance(dt,ScalarTypedDataSpec):
        n=etree.Element(toNS("p:Scalar"))
        n.attrib["name"]=dt.name
        n.attrib["type"]=dt.type

        if dt.value is not None and dt.value is not 0:
            n.attrib["value"]=str(dt.value)
    elif isinstance(dt,ArrayTypedDataSpec):
        n=etree.Element(toNS("p:Array"))
        n.attrib["name"]=dt.name
        n.attrib["length"]=str(dt.length)
        assert isinstance(dt.type,ScalarTypedDataSpec)
        n.attrib["type"]=dt.type.type
    else:
        raise RuntimeError("Unknown data type.")

    return n

def save_typed_struct_spec(node,childTagName,tuple):
    if tuple is None:
        return
    
    assert isinstance(tuple,TupleTypedDataSpec), "Expected tuple, got {}".format(tuple)
    if len(tuple.elements_by_index)==0:
        return

    r=etree.Element(toNS(childTagName))
    for elt in tuple.elements_by_index:
        r.append(save_typed_data_spec(elt))
    node.append(r)
    return r

def save_typed_struct_instance(node,childTagName,type,inst):
    if inst is None:
        return
    assert type.is_refinement_compatible(inst)
    if len(inst) is 0:
        return
    text=json.dumps(inst)
    assert text.startswith('{') and text.endswith('}')
    r=etree.Element(toNS(childTagName))
    r.text=text[1:-1] # Get rid of brackets
    node.append(r)
    return r
    
    
    
def save_edge_type(et):
    n=_type_to_element(et)

    n.attrib["id"]=et.id
    save_typed_struct_spec(n, "p:Message", et.message)
    save_typed_struct_spec(n, "p:Properties", et.properties)
    save_typed_struct_spec(n, "p:State", et.state)

    return n


def save_device_type(dt):
    n=_type_to_element(dt)

    n.attrib["id"]=dt.id
    save_typed_struct_spec(n, "p:Properties", dt.properties)
    save_typed_struct_spec(n, "p:State", dt.state)
        
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
    n.attrib["type"]=di.device_type.id
    save_typed_struct_instance(n, "p:P", di.device_type.properties, di.properties)

    if di.native_location is not None:
        n.attrib["nativeLocation"]= ",".join([str(l) for l in di.native_location])

    return n


def save_edge_instance(ei):
    n=_type_to_element(ei)

    if True:
        n.attrib["path"]="{}:{}-{}:{}".format(ei.dst_device.id, ei.dst_port.name,ei.src_device.id,ei.src_port.name)
    else:
        n.attrib["dstDeviceId"]=ei.dst_device.id
        n.attrib["dstPortName"]=ei.dst_port.name
        n.attrib["srcDeviceId"]=ei.src_device.id
        n.attrib["srcPortName"]=ei.src_port.name
    
    save_typed_struct_instance(n, "p:P", ei.edge_type.properties, ei.properties)

    return n


def save_graph_type(graph):
    gn = _type_to_element(graph);
    gn.attrib["id"]=graph.id

    if graph.native_dimension!=None and graph.native_dimension!=0:
        gn.attrib["nativeDimension"]=str(graph.native_dimension)

    save_typed_struct_spec(gn, "p:Properties", graph.properties)

    if graph.shared_code:
        for code in graph.shared_code:
            h=etree.Element(toNS("p:SharedCode"))
            h.text=etree.CDATA(code)
            gn.append(h)

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

    save_typed_struct_instance(gn, "p:Properties", graph.graph_type.properties ,graph.properties)
    
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
    nsmap = { None : "http://TODO.org/POETS/virtual-graph-schema-v0" }
    root=etree.Element(toNS("p:Graph"), nsmap=nsmap)
    
    root.append(save_graph_type(graph.graph_type))
    root.append(save_graph_instance(graph))

    # The wierdness is because stdout is in text mode, so we send
    # it via a string. Ideally it would write it straight to the file...
    tree = etree.ElementTree(root)
    #s=etree.tostring(root,pretty_print=True,xml_declaration=True).decode("utf8")
    #dst.write(s)
    tree.write(dst.buffer, pretty_print=True, xml_declaration=True)
