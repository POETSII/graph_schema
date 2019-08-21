from graph.core import *

#from xml.etree import ElementTree as etree
from lxml import etree

from typing import *
import os
import sys
import json

ns={"p":"https://poets-project.org/schemas/virtual-graph-schema-v3"}

def toNS(t):
    tt=t.replace("p:","{"+ns["p"]+"}")
    return tt

#_type_to_tag={
#    GraphType : toNS("p:GraphType"),
#    MessageType : toNS("p:MessageType"),
#    DeviceType : toNS("p:DeviceType"),
#    InputPin : toNS("p:InputPin"),
#    OutputPin : toNS("p:OutputPin"),
#    GraphInstance : toNS("p:GraphInstance"),
#    EdgeInstance : toNS("p:EdgeI"),
#    DeviceInstance : toNS("p:DevI")
#}

def _type_to_element(parent,t):
    tag=_type_to_tag[type(t)]
    return etree.SubElement(parent,tag)

# TODO: Don't print empty parts of default, i.e. in a tuple { x, y, z }, if only x is non-0, then print { "x": 1 } only
def save_typed_data_spec(dt):
    if isinstance(dt,TupleTypedDataSpec):
        n=etree.Element(toNS("p:Tuple"))
        n.attrib["name"]=dt.name
        if dt.documentation is not None:
            doc = etree.SubElement(n, toNS("p:Documentation"))
            doc.text = dt.documentation
        for e in dt.elements_by_index:
            n.append(save_typed_data_spec(e))
        if dt.default is not None:
            save_typed_struct_instance(n, toNS("p:Default"), dt, dt.default)
    elif isinstance(dt,ScalarTypedDataSpec):
        n=etree.Element(toNS("p:Scalar"))
        n.attrib["name"]=dt.name
        if dt.documentation is not None:
            doc = etree.SubElement(n, toNS("p:Documentation"))
            doc.text = dt.documentation
        if isinstance(dt.type,Typedef):
            n.attrib["type"]=dt.type.id
        else:
            n.attrib["type"]=dt.type
        if dt.default is not None:
            if (isinstance(dt.default, int) or isinstance(dt.default, float)):
                if dt.default is not 0:
                    n.attrib["default"]=json.dumps(dt.default) # Need json for typedef'd structs and arrays
            else:
                save_typed_struct_instance(n, toNS("p:Default"), dt, dt.default)
    elif isinstance(dt,ArrayTypedDataSpec):
        n=etree.Element(toNS("p:Array"))
        n.attrib["name"]=dt.name
        n.attrib["length"]=str(dt.length)
        if dt.documentation is not None:
            doc = etree.SubElement(n, toNS("p:Documentation"))
            doc.text = dt.documentation
        if isinstance(dt.type,ScalarTypedDataSpec):
            n.attrib["type"]=dt.type.type
        elif isinstance(dt.type,Typedef):
            n.attrib["type"]=dt.type.id
        else:
            subn=save_typed_data_spec(dt.type)
            n.append(subn)
        if dt.default is not None:
            save_typed_struct_instance(n, toNS("p:Default"), dt, dt.default)
    elif isinstance(dt,Typedef):
        return save_typed_data_spec(dt.type)
    else:
        raise RuntimeError("Unknown data type: {}.".format(dt))

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
    assert (text.startswith('{') and text.endswith('}')) or (text.startswith('[') and text.endswith(']'))
    r=etree.SubElement(node, childTagNameWithNS)
    if (text.startswith('[') and text.endswith(']')):
        r.text=text # Get rid of brackets
    else:
        r.text=text[1:-1]
    return r

def save_metadata(node,childTagNameWithNS,value):
    if value is None:
        return
    text=json.dumps(value)
    assert text.startswith('{') and text.endswith('}')
    text=text[1:-1] # Get rid of brackets
    text=text.strip()
    if text=="":
        return
    r=etree.SubElement(node, childTagNameWithNS)
    r.text=text
    return r

def save_type_def(parent,td):
    n=etree.SubElement(parent,toNS("p:TypeDef"))

    n.attrib["id"]=td.id
    s=save_typed_data_spec(td.type)
    if td.documentation is not None:
        doc = etree.SubElement(n,toNS("p:Documentation"))
        doc.text = td.documentation
    n.append(s)

    return n

def save_message_type(parent,mt):
    n=etree.SubElement(parent,toNS("p:MessageType"))

    n.attrib["id"]=mt.id
    if mt.documentation is not None:
        doc = etree.SubElement(n,toNS("p:Documentation"))
        doc.text = mt.documentation
    save_typed_struct_spec(n, toNS("p:Message"), mt.message)
    save_metadata(n, toNS("p:MetaData"), mt.metadata)

    return n


def save_device_type(parent,dt):
    n=etree.SubElement(parent,toNS("p:DeviceType"))

    n.attrib["id"]=dt.id
    if dt.documentation is not None:
        doc = etree.SubElement(n, toNS("p:Documentation"))
        doc.text = dt.documentation
    save_typed_struct_spec(n, toNS("p:Properties"), dt.properties)
    save_typed_struct_spec(n, toNS("p:State"), dt.state)
    save_metadata(n, toNS("p:MetaData"), dt.metadata)

    if dt.shared_code:
        for s in dt.shared_code:
            sn=etree.SubElement(n,toNS("p:SharedCode"))
            sn.text=etree.CDATA(s)

    if dt.init_handler:
        pn=etree.Element(toNS("p:OnInit"))
        pn.text=etree.CDATA(dt.init_handler)
        n.append(pn)

    for p in dt.inputs_by_index:
        pn=etree.SubElement(n,toNS("p:InputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        if p.is_application: # TODO: Remove applications pins in all forms
            pn.attrib["application"]="true"
        if p.documentation is not None:
            doc = etree.SubElement(pn, toNS("p:Documentation"))
            doc.text = p.documentation
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
        pn=etree.SubElement(n,toNS("OutputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        if p.is_indexed is not None:
            pn.attrib["indexed"]= "true" if p.is_indexed  else "false"
        if p.is_application: # TODO: Remove applications pins in all forms
            pn.attrib["application"]="true"
        if p.documentation is not None:
            doc = etree.SubElement(pn, toNS("p:Documentation"))
            doc.text = p.documentation
        save_metadata(pn, toNS("p:MetaData"), p.metadata)

        h=etree.Element(toNS("p:OnSend"))
        h.text = etree.CDATA(p.send_handler)
        #h.text = p.send_handler
        pn.append(h)

    pn=etree.Element(toNS("p:ReadyToSend"))
    pn.text=etree.CDATA(dt.ready_to_send_handler)
    n.append(pn)

    if dt.on_hardware_idle_handler:
        pn=etree.Element(toNS("p:OnHardwareIdle"))
        pn.text=etree.CDATA(dt.on_hardware_idle_handler)
        n.append(pn)

    if dt.on_device_idle_handler:
        pn=etree.Element(toNS("p:OnDeviceIdle"))
        pn.text=etree.CDATA(dt.on_device_idle_handler)
        n.append(pn)

    return n

def save_external_type(parent,dt):
    n=etree.SubElement(parent,toNS("p:ExternalType"))

    n.attrib["id"]=dt.id
    if dt.documentation is not None:
        doc = etree.SubElement(n, toNS("p:Documentation"))
        doc.text = dt.documentation

    for p in dt.inputs_by_index:
        pn=etree.SubElement(n,toNS("p:InputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        if p.documentation is not None:
            doc = etree.SubElement(pn, toNS("p:Documentation"))
            doc.text = p.documentation

    for p in dt.outputs_by_index:
        pn=etree.SubElement(n,toNS("OutputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id

        if p.documentation is not None:
            doc = etree.SubElement(pn, toNS("p:Documentation"))
            doc.text = p.documentation
        save_metadata(pn, "p:MetaData", p.metadata)

    return n

_device_instance_tag_type=toNS("p:DevI")
_device_instance_properties_type=toNS("p:P")
_device_instance_state_type=toNS("p:S")
_device_instance_metadata_type=toNS("p:M")

_external_instance_tag_type=toNS("p:ExtI")


def reduce_typed_data(default, typed, result=None):
    if default == typed:
        if isinstance(default, dict):
            return {}
        elif isinstance(default, list):
            return []
        else:
            return 0
    elif isinstance(default, dict):
        res = {}
        for i in default:
            if i in typed:
                r = reduce_typed_data(default[i], typed[i], res)
                if r != [] and r != {} and r != 0:
                    res[i] = r
        return res
    elif isinstance(default, list):
        res = []
        diff = []
        for i in range(0, len(default)):
            if i < len(typed):
                r = reduce_typed_data(default[i], typed[i], res)
                if r == [] or r == {} or r == 0:
                    diff.append(r)
                else:
                    diff.append(r)
                    res = []
                    for l in range(0, len(diff)):
                        res.append(diff[l])
        return res
    else:
        return typed


def save_device_instance(parent, di):
    if di.device_type.isExternal:
        n = etree.SubElement(parent, _external_instance_tag_type, {"id":di.id,"type":di.device_type.id} )
    else:
        n = etree.SubElement(parent, _device_instance_tag_type, {"id":di.id,"type":di.device_type.id} )


        if di.properties is not None:
            defaultProperties = di.device_type.properties.create_default()
            differingProperties = reduce_typed_data(defaultProperties, di.properties)
            save_typed_struct_instance(n, _device_instance_properties_type, di.device_type.properties, differingProperties)

        defaultState = di.device_type.state.create_default()
        if di.state is not None:
            differingState = reduce_typed_data(defaultState, di.state)
            save_typed_struct_instance(n, _device_instance_state_type, di.device_type.state, differingState)

        save_metadata(n, _device_instance_metadata_type, di.metadata)

    return n

_edge_instance_tag_type=toNS("p:EdgeI")
_edge_instance_properties_type=toNS("p:P")
_edge_instance_state_type=toNS("p:S")
_edge_instance_metadata_type=toNS("p:M")

def save_edge_instance(parent, ei):
    n=etree.SubElement(parent, _edge_instance_tag_type, {"path":ei.id } )

    if ei.send_index is not None:
        n.attrib["sendIndex"]=ei.send_index

    save_typed_struct_instance(n, _edge_instance_properties_type, ei.dst_pin.properties, ei.properties)
    save_typed_struct_instance(n, _edge_instance_state_type, ei.dst_pin.state, ei.state)
    save_metadata(n, _edge_instance_metadata_type, ei.metadata)

    return n


def save_graph_type(parent, graph):
    gn = etree.SubElement(parent,toNS("p:GraphType"))
    gn.attrib["id"]=graph.id

    if graph.documentation is not None:
        doc=etree.Element(toNS("p:Documentation"))
        doc.text = graph.documentation
        gn.append(doc)

    if len(graph.typedefs_by_index)>0:
        tdn=etree.Element(toNS("p:Types"))
        gn.append(tdn)
        for td in graph.typedefs_by_index:
            save_type_def(tdn,td)

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
        if dt.isExternal:
            save_external_type(dtn, dt)
        else:
            save_device_type(dtn,dt)

    return gn

def save_graph_instance(parent, graph):
    gn = etree.SubElement(parent, toNS("p:GraphInstance"));
    gn.attrib["id"]=graph.id
    gn.attrib["graphTypeId"]=graph.graph_type.id

    if graph.documentation is not None:
        doc = etree.SubElement(gn, toNS("p:Documentation"))
        doc.text = graph.documentation

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


def save_graph(graph:Union[GraphType,GraphInstance],dst):
    nsmap = { None : "https://poets-project.org/schemas/virtual-graph-schema-v3" }
    root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)

    if dst is str:
        if dst.endswith(".gz"):
            import gzip
            with gzip.open(dst, 'wt', compresslevel=6) as dstFile:
                save_graph(graph,dstFile)
        else:
            with open(dst,"wt") as dstFile:
                assert not isinstance(dstFile,str)
                save_graph(graph,dstFile)
    else:
        if isinstance(graph, GraphInstance):
            graph_type=graph.graph_type
            graph_instance=graph
        else:
            graph_type=graph
            graph_instance=None

        sys.stderr.write("save_graph: Constructing graph type tree\n")
        save_graph_type(root,graph_type)
        if graph_instance is not None:
            sys.stderr.write("save_graph: Constructing graph inst tree\n")
            save_graph_instance(root,graph_instance)

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
    nsmap = { None : "https://poets-project.org/schemas/virtual-graph-schema-v2" }
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


