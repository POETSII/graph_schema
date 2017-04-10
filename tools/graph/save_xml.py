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

#_type_to_tag={
#    GraphType : toNS("p:GraphType"),
#    MessageType : toNS("p:MessageType"),
#    DeviceType : toNS("p:DeviceType"),
#    InputPort : toNS("p:InputPort"),
#    OutputPort : toNS("p:OutputPort"),
#    GraphInstance : toNS("p:GraphInstance"),
#    EdgeInstance : toNS("p:EdgeI"),
#    DeviceInstance : toNS("p:DevI")
#}

def _type_to_element(parent,t):
    tag=_type_to_tag[type(t)]
    return etree.SubElement(parent,tag)

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

        if dt.default is not None and dt.default is not 0:
            n.attrib["default"]=str(dt.default)
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

def save_typed_struct_spec(node,childTagNameWithNS,tuple):
    if tuple is None:
        return

    assert isinstance(tuple,TupleTypedDataSpec), "Expected tuple, got {}".format(tuple)
    if len(tuple.elements_by_index)==0:
        return

    r=etree.Element(childTagNameWithNS)
    save_typed_struct_spec_contents(r, tuple)
    node.append(r)
    return r

def save_typed_struct_instance(node,childTagNameWithNS,type,inst):
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
    r=etree.SubElement(node, childTagNameWithNS)
    r.text=text[1:-1] # Get rid of brackets
    return r

def save_metadata(node,childTagNameWithNS,value):
    if value is None:
        return
    text=json.dumps(value)
    assert text.startswith('{') and text.endswith('}')
    r=etree.SubElement(node, childTagNameWithNS)
    r.text=text[1:-1] # Get rid of brackets
    return r


def save_message_type(parent,mt):
    n=etree.SubElement(parent,toNS("p:MessageType"))

    n.attrib["id"]=mt.id
    save_typed_struct_spec(n, toNS("p:Message"), mt.message)
    save_metadata(n, toNS("p:MetaData"), mt.metadata)

    return n


def save_device_type(parent,dt):
    n=etree.SubElement(parent,toNS("p:DeviceType"))

    n.attrib["id"]=dt.id
    save_typed_struct_spec(n, toNS("p:Properties"), dt.properties)
    save_typed_struct_spec(n, toNS("p:State"), dt.state)
    save_metadata(n, toNS("p:MetaData"), dt.metadata)

    if dt.shared_code:
        for s in dt.shared_code:
            sn=etree.SubElement(n,toNS("p:SharedCode"))
            sn.text=etree.CDATA(s)

    for p in dt.inputs_by_index:
        pn=etree.SubElement(n,toNS("p:InputPort"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        if(p.properties):
            save_typed_struct_spec(pn, toNS("p:Properties"), p.properties)
        if(p.state):
            save_typed_struct_spec(pn, toNS("p:State"), p.state)

        save_metadata(pn, toNS("p:MetaData"), p.metadata)

        h=etree.Element(toNS("p:OnReceive"))
        h.text = etree.CDATA(p.receive_handler)
        #h.txt = p.received_handler
        pn.append(h)

    for p in dt.outputs_by_index:
        pn=etree.SubElement(n,toNS("OutputPort"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id

        save_metadata(pn, "p:MetaData", p.metadata)

        h=etree.Element(toNS("p:OnSend"))
        h.text = etree.CDATA(p.send_handler)
        #h.text = p.send_handler
        pn.append(h)

    pn=etree.Element(toNS("ReadyToSend"))
    pn.text=etree.CDATA(dt.ready_to_send_handler)
    n.append(pn)

    return n


_device_instance_tag_type=toNS("p:DevI")
_device_instance_properties_type=toNS("p:P")
_device_instance_metadata_type=toNS("p:M")


def save_device_instance(parent, di):
    n=etree.SubElement(parent, _device_instance_tag_type, {"id":di.id,"type":di.device_type.id} )

    save_typed_struct_instance(n, _device_instance_properties_type, di.device_type.properties, di.properties)
    save_metadata(n, _device_instance_metadata_type, di.metadata)

    return n

_edge_instance_tag_type=toNS("p:EdgeI")
_edge_instance_properties_type=toNS("p:P")
_edge_instance_metadata_type=toNS("p:M")

def save_edge_instance(parent, ei):
    n=etree.SubElement(parent, _edge_instance_tag_type, {"path":ei.id } )

    save_typed_struct_instance(n, _edge_instance_properties_type, ei.dst_port.properties, ei.properties)
    save_metadata(n, _edge_instance_metadata_type, ei.metadata)

    return n


def save_graph_type(parent, graph):
    gn = etree.SubElement(parent,toNS("p:GraphType"))
    gn.attrib["id"]=graph.id

    save_typed_struct_spec(gn, toNS("p:Properties"), graph.properties)

    save_metadata(gn, toNS("p:MetaData"), graph.metadata)

    if graph.shared_code:
        for code in graph.shared_code:
            h=etree.Element(toNS("p:SharedCode"))
            h.text=etree.CDATA(code)
            gn.append(h)

    etn = etree.Element(toNS("p:MessageTypes"))
    gn.append(etn)
    for et in graph.message_types.values():
        save_message_type(etn, et)

    dtn = etree.Element(toNS("p:DeviceTypes"))
    gn.append(dtn)
    for dt in graph.device_types.values():
        save_device_type(dtn,dt)

    return gn

def save_graph_instance(parent, graph):
    gn = etree.SubElement(parent, toNS("p:GraphInstance"));
    gn.attrib["id"]=graph.id
    gn.attrib["graphTypeId"]=graph.graph_type.id

    save_typed_struct_instance(gn, toNS("p:Properties"), graph.graph_type.properties ,graph.properties)

    save_metadata(gn, toNS("p:MetaData"), graph.metadata)

    din = etree.Element(toNS("p:DeviceInstances"))
    din.attrib["sorted"]="1"
    gn.append(din)
    for i in sorted(graph.device_instances.keys()):
        save_device_instance(din, graph.device_instances[i] )

    ein = etree.Element(toNS("p:EdgeInstances"))
    ein.attrib["sorted"]="1"
    gn.append(ein)
    for i in sorted(graph.edge_instances.keys()):
        assert( graph.edge_instances[i].dst_device.id in graph.device_instances )
        assert( graph.edge_instances[i].src_device.id in graph.device_instances )
        save_edge_instance(ein, graph.edge_instances[i] )

    return gn
    

def save_graph_instance_metadata_patch(parent, id,graphMeta,deviceMeta,edgeMeta):
    gn = etree.Element(toNS("p:GraphInstanceMetadataPatch"))
    gn.attrib["id"]=id
    parent.append(gn)

    save_metadata(gn, toNS("p:MetaData"), graphMeta)

    din = etree.Element(toNS("p:DeviceInstances"))
    din.attrib["sorted"]="1"
    gn.append(din)
    diTag=toNS("p:DevI")
    for di in sorted(deviceMeta.keys()):
        n=etree.SubElement(din, diTag, {"id":di})
        save_metadata(n, toNS("p:M"), deviceMeta[id])
        
        
    ein = etree.Element(toNS("p:EdgeInstances"))
    ein.attrib["sorted"]="1"
    gn.append(ein)
    eiTag=toNS("p:EdgeI")
    for ei in sorted(edgeMeta.keys()):
        n=etree.SubElement(ein, eiTag, {"id":ei})
        save_metadata(n, toNS("p:M"), edgeMeta[ei])
        
    return gn    
 

def save_graph(graph,dst):
    nsmap = { None : "http://TODO.org/POETS/virtual-graph-schema-v1" }
    root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)

    sys.stderr.write("save_graph: Constructing graph type tree\n")
    save_graph_type(root,graph.graph_type)
    sys.stderr.write("save_graph: Constructing graph inst tree\n")
    save_graph_instance(root,graph)

    sys.stderr.write("save_graph: writing\n")
    # The wierdness is because stdout is in text mode, so we send
    # it via a string. Ideally it would write it straight to the file...
    tree = etree.ElementTree(root)
    #s=etree.tostring(root,pretty_print=True,xml_declaration=True).decode("utf8")
    #dst.write(s)
    
    # TODO : Fix this!
    if (sys.version_info > (3, 0)):
        # Python3
        tree.write(dst.buffer, pretty_print=True, xml_declaration=True)
    else:
        #Python2
        tree.write(dst, pretty_print=True, xml_declaration=True)

def save_metadata_patch(id,graphMeta,deviceMeta,edgeMeta,dst):
    nsmap = { None : "http://TODO.org/POETS/virtual-graph-schema-v1" }
    root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)

    save_graph_instance_metadata_patch(root, id,graphMeta,deviceMeta,edgeMeta)

    # The wierdness is because stdout is in text mode, so we send
    # it via a string. Ideally it would write it straight to the file...
    tree = etree.ElementTree(root)
    #s=etree.tostring(root,pretty_print=True,xml_declaration=True).decode("utf8")
    #dst.write(s)
    
    # TODO : Fix this!
    if (sys.version_info > (3, 0)):
        # Python3
        tree.write(dst.buffer, pretty_print=True, xml_declaration=True)
    else:
        #Python2
        tree.write(dst, pretty_print=True, xml_declaration=True)

    
