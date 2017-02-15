from graph.core import *

#from xml.etree import ElementTree as etree
from lxml import etree

import os
import sys
import json

ns={"p":"http://TODO.org/POETS/virtual-graph-schema-v1"}

def toNS(t):
    tt=t.replace("p:","{"+ns["p"]+"}")
    return tt

_type_to_tag={
    GraphType : toNS("p:GraphType"),
    MessageType : toNS("p:MessageType"),
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
            n.attrib["default"]=str(dt.value)
    elif isinstance(dt,ArrayTypedDataSpec):
        n=etree.Element(toNS("p:Array"))
        n.attrib["name"]=dt.name
        n.attrib["length"]=str(dt.length)
        assert isinstance(dt.type,ScalarTypedDataSpec)
        n.attrib["type"]=dt.type.type
    else:
        raise RuntimeError("Unknown data type.")

    return n
    
def save_typed_struct_spec_contents(node,tuple):
    if tuple is None:
        return
    
    assert isinstance(tuple,TupleTypedDataSpec), "Expected tuple, got {}".format(tuple)
    if len(tuple.elements_by_index)==0:
        return

    for elt in tuple.elements_by_index:
        node.append(save_typed_data_spec(elt))
    return node

def save_typed_struct_spec(node,childTagName,tuple):
    if tuple is None:
        return
    
    assert isinstance(tuple,TupleTypedDataSpec), "Expected tuple, got {}".format(tuple)
    if len(tuple.elements_by_index)==0:
        return

    r=etree.Element(toNS(childTagName))
    save_typed_struct_spec_contents(r, tuple)   
    node.append(r)
    return r

def save_typed_struct_instance(node,childTagName,type,inst):
    if inst is None:
        return
    assert type.is_refinement_compatible(inst)
    if len(inst) is 0:
        return
    try:
        text=json.dumps(inst)
    except BaseException:
        raise RuntimeError("Exception while serialising '{}'".format(inst))
    assert text.startswith('{') and text.endswith('}')
    r=etree.Element(toNS(childTagName))
    r.text=text[1:-1] # Get rid of brackets
    node.append(r)
    return r

def save_metadata(node,childTagName,value):
    if value is None:
        return
    text=json.dumps(value)
    sys.stderr.write(str(value)+"\n")
    sys.stderr.write(text+"\n")
    assert text.startswith('{') and text.endswith('}')
    r=etree.Element(toNS(childTagName))
    r.text=text[1:-1] # Get rid of brackets
    node.append(r)
    return r
    
    
def save_message_type(mt):
    n=_type_to_element(mt)

    n.attrib["id"]=mt.id
    save_typed_struct_spec(n, "p:Message", mt.message)
    save_metadata(n, "p:MetaData", mt.metadata)

    return n


def save_device_type(dt):
    n=_type_to_element(dt)

    n.attrib["id"]=dt.id
    save_typed_struct_spec(n, "p:Properties", dt.properties)
    save_typed_struct_spec(n, "p:State", dt.state)
    save_metadata(n, "p:MetaData", dt.metadata)
    
    if dt.shared_code:
        for s in dt.shared_code:
            sn=etree.Element(toNS("p:SharedCode"))
            sn.text=etree.CDATA(s)
            n.append(sn)
        
    for p in dt.inputs_by_index:
        pn=_type_to_element(p)
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        if(p.properties):
            save_typed_struct_spec(pn, "p:Properties", p.properties)
        if(p.state):
            save_typed_struct_spec(pn, "p:State", p.state)

        save_metadata(pn, "p:MetaData", p.metadata)

        h=etree.Element(toNS("p:OnReceive"))
        h.text = etree.CDATA(p.receive_handler)
        #h.txt = p.received_handler
        pn.append(h)
        n.append(pn)

    for p in dt.outputs_by_index:
        pn=_type_to_element(p)
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id

        save_metadata(pn, "p:MetaData", p.metadata)
        
        h=etree.Element(toNS("p:OnSend"))
        h.text = etree.CDATA(p.send_handler)
        #h.text = p.send_handler
        pn.append(h)
        n.append(pn)

    pn=etree.Element(toNS("ReadyToSend"))
    pn.text=etree.CDATA(dt.ready_to_send_handler)
    n.append(pn)

    return n     


def save_device_instance(di):
    n=_type_to_element(di)

    n.attrib["id"]=di.id
    n.attrib["type"]=di.device_type.id
    save_typed_struct_instance(n, "p:P", di.device_type.properties, di.properties)

    save_metadata(n, "p:M", di.metadata)

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
    
    save_typed_struct_instance(n, "p:P", ei.dst_port.properties, ei.properties)

    save_metadata(n, "p:M", ei.metadata)


    return n


def save_graph_type(graph):
    gn = _type_to_element(graph);
    gn.attrib["id"]=graph.id


    save_typed_struct_spec(gn, "p:Properties", graph.properties)

    save_metadata(gn, "p:MetaData", graph.metadata)

    if graph.shared_code:
        for code in graph.shared_code:
            h=etree.Element(toNS("p:SharedCode"))
            h.text=etree.CDATA(code)
            gn.append(h)

    etn = etree.Element(toNS("p:MessageTypes"))
    gn.append(etn)
    for et in graph.message_types.values():
        etn.append(save_message_type(et))

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

    save_metadata(gn, "p:MetaData", graph.metadata)
    
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
    nsmap = { None : "http://TODO.org/POETS/virtual-graph-schema-v1" }
    root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)
    
    root.append(save_graph_type(graph.graph_type))
    root.append(save_graph_instance(graph))

    # The wierdness is because stdout is in text mode, so we send
    # it via a string. Ideally it would write it straight to the file...
    tree = etree.ElementTree(root)
    #s=etree.tostring(root,pretty_print=True,xml_declaration=True).decode("utf8")
    #dst.write(s)
    tree.write(dst.buffer, pretty_print=True, xml_declaration=True)
