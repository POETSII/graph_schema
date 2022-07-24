from graph.core import *

#from xml.etree import ElementTree as etree
from lxml import etree

import os
import sys
import json
from typing import *

ns={"p":"https://poets-project.org/schemas/virtual-graph-schema-v4"}

def toNS(t):
    tt=t.replace("p:","{"+ns["p"]+"}")
    return tt

def typed_data_spec_to_c_decl(dt:TypedDataSpec, name:Optional[str]=None) -> str:
    if name is None:
        name=dt.name
    if isinstance(dt,TupleTypedDataSpec):
        res="struct {\n"
        for e in dt.elements_by_index:
            res+=typed_data_spec_to_c_decl(e)+";\n"
        res+=f"}} {name}"
        return res
    elif isinstance(dt,ScalarTypedDataSpec):
        if isinstance(dt.type,Typedef):
            raise RuntimeError("Typedefs are not supported when writing as v4")
        return f"{dt.type} {name}"
    elif isinstance(dt,ArrayTypedDataSpec):
        return typed_data_spec_to_c_decl(dt.type, name)+f"[{dt.length}]"
    elif isinstance(dt,Typedef):
        raise RuntimeError("Typedefs are not supported when writing as v4")
    else:
        raise RuntimeError("Unknown data type: {}.".format(dt))

def save_typed_struct_spec_contents(node,tuple) -> str:
    if tuple is None:
        return ""

    assert isinstance(tuple,TupleTypedDataSpec), "Expected tuple, got {}".format(tuple)
    if len(tuple.elements_by_index)==0:
        return ""

    res=""
    for elt in tuple.elements_by_index:
        res += typed_data_spec_to_c_decl(elt)+";\n"
    node.text=etree.CDATA(res)

def save_typed_struct_spec(node,childTagNameWithNS,tuple):
    r=etree.Element(childTagNameWithNS)
    
    if tuple is not None:
        assert isinstance(tuple,TupleTypedDataSpec), "Expected tuple, got {}".format(tuple)
        
        save_typed_struct_spec_contents(r, tuple)
    node.append(r)
    return r

def convert_json_init_to_c_init(t,v):
    if isinstance(v,(int,float)):
        assert isinstance(t,ScalarTypedDataSpec)
        if t.type=="uint64_t":
            return str(v)+"ull"
        elif t.type=="int64_t":
            return str(v)+"ll"
        else:
            return str(v)
    elif isinstance(v,bool):
        assert isinstance(t,ScalarTypedDataSpec)
        return str("1" if v else "0" )
    elif isinstance(v,dict):
        assert isinstance(t,TupleTypedDataSpec)

        return "{"+ ",".join([convert_json_init_to_c_init(e, v.get(e.name, e.default)) for e in t.elements_by_index ]) +"}"
    elif isinstance(v,list):
        assert isinstance(t,ArrayTypedDataSpec)
        return "{"+ ",".join([convert_json_init_to_c_init(t.type, e) for e in v ]) +"}"
    else:
        raise RuntimeError("Unknown json element type")


def save_typed_struct_instance_attrib(node,attrName,type,inst):
    if type is not None:
        assert type.is_refinement_compatible(inst)
        inst=type.expand(inst)
        if inst != {}:
            text=convert_json_init_to_c_init(type, inst)
            node.attrib[attrName]=text
    else:
        assert inst is None

def save_metadata(node,childTagNameWithNS,value):
    pass # TODO: we don't do anything with metadata yet

def save_type_def(parent,td):
    raise RuntimeError("Typedefs are not yet supported for conversion to v4")

def save_message_type(parent,mt):
    n=etree.SubElement(parent,toNS("p:MessageType"))

    n.attrib["id"]=mt.id
    save_typed_struct_spec(n, toNS("p:Message"), mt.message)

    return n

def add_code_element(n:etree.Element, tag:str, code:Optional[str]):
    sn=etree.SubElement(n,toNS(tag))
    if code is not None:
        sn.text=etree.CDATA(code)

def add_documentation_comment(n:etree.Element, doc:Optional[str]):
    if doc is not None:
        n.append(etree.Comment(doc))

def save_device_type(parent:etree.Element, dt):
    n=etree.SubElement(parent,toNS("p:DeviceType"))

    n.attrib["id"]=dt.id
    add_documentation_comment(n, dt.documentation)

    save_typed_struct_spec(n, toNS("p:Properties"), dt.properties)
    save_typed_struct_spec(n, toNS("p:State"), dt.state)

    shared_code=""
    if dt.shared_code:
        for s in dt.shared_code:
            shared_code+=s
    add_code_element(n, "p:SharedCode", shared_code)

    for p in dt.inputs_by_index:
        pn=etree.SubElement(n,toNS("p:InputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        assert not p.is_application
        add_documentation_comment(pn, p.documentation)
        save_typed_struct_spec(pn, toNS("p:Properties"), p.properties)
        save_typed_struct_spec(pn, toNS("p:State"), p.state)

        add_code_element(pn, "OnReceive", p.receive_handler)

    for p in dt.outputs_by_index:
        pn=etree.SubElement(n,toNS("OutputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        if p.is_indexed is not None:
            pn.attrib["indexed"]="true" if p.is_indexed else "false"
        assert not p.is_application
        add_documentation_comment(pn, p.documentation)
        add_code_element(pn, "OnSend", p.send_handler)

    add_code_element(n, "ReadyToSend", dt.ready_to_send_handler)
    add_code_element(n, "OnInit", dt.init_handler)
    add_code_element(n, "OnHardwareIdle", dt.on_hardware_idle_handler)
    add_code_element(n, "OnDeviceIdle", None)   

    return n

def save_external_type(parent,dt):
    n=etree.SubElement(parent,toNS("p:ExternalType"))

    n.attrib["id"]=dt.id
    add_documentation_comment(n, dt.documentation)

    save_typed_struct_spec(n, toNS("p:Properties"), dt.properties)

    for p in dt.inputs_by_index:
        pn=etree.SubElement(n,toNS("p:InputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        add_documentation_comment(p, p.documentation)

    for p in dt.outputs_by_index:
        pn=etree.SubElement(n,toNS("OutputPin"))
        pn.attrib["name"]=p.name
        pn.attrib["messageTypeId"]=p.message_type.id
        add_documentation_comment(pn, p.documentation)

    return n

def save_supervisor_type(parent:etree.Element, st:SupervisorType):
    n=etree.SubElement(parent,toNS("p:SupervisorType"))

    n.attrib["id"]=st.id
    
    add_code_element(n, "p:Properties", st.properties_code)
    add_code_element(n, "p:State", st.state_code)
    add_code_element(n, "p:Code", st.shared_code)
    add_code_element(n, "p:OnInit", st.init_handler)
    add_code_element(n, "p:OnSupervisorIdle", st.on_supervisor_idle_handler)
    add_code_element(n, "p:OnStop", st.on_stop_handler)
    for input in st.inputs_by_index:
        ni=etree.SubElement(n, toNS("p:SupervisorInPin"))
        ni.attrib["id"]=input[1]
        ni.attrib["messageTypeId"]=input[2].id
        add_code_element(ni, "p:OnReceive", input[3])

    return n

_device_instance_tag_type=toNS("p:DevI")

_external_instance_tag_type=toNS("p:ExtI")


def save_device_instance(parent, di):
    if di.device_type.isExternal:
        n = etree.SubElement(parent, _external_instance_tag_type, {"id":di.id,"type":di.device_type.id} )

        properties = di.device_type.properties.expand(di.properties) 
        save_typed_struct_instance_attrib(n, "P", di.device_type.properties, properties)

    else:
        n = etree.SubElement(parent, _device_instance_tag_type, {"id":di.id,"type":di.device_type.id} )

        properties = di.device_type.properties.expand(di.properties) 
        save_typed_struct_instance_attrib(n, "P", di.device_type.properties, properties)

        state = di.device_type.state.expand(di.state) 
        save_typed_struct_instance_attrib(n, "S", di.device_type.state, state)

    return n

_edge_instance_tag_type=toNS("p:EdgeI")
_edge_instance_properties_type=toNS("p:P")
_edge_instance_state_type=toNS("p:S")
_edge_instance_metadata_type=toNS("p:M")

def save_edge_instance(parent, ei):
    n=etree.SubElement(parent, _edge_instance_tag_type, {"path":ei.id } )

    if ei.send_index is not None:
        n.attrib["sendIndex"]=ei.send_index

    save_typed_struct_instance_attrib(n, "P", ei.dst_pin.properties, ei.properties)

    state=None
    if ei.dst_pin.state != None:
        state = ei.dst_pin.state.create_default()
    if hasattr(ei, "state") and ei.state is not None:
        state=ei.dst_pin.state.expand(state)
    save_typed_struct_instance_attrib(n, "S", ei.dst_pin.state, state)
    

    return n


def save_graph_type(parent, graph:GraphType):
    gn = etree.SubElement(parent,toNS("p:GraphType"))
    gn.attrib["id"]=graph.id

    add_documentation_comment(gn, graph.documentation)

    assert len(graph.typedefs_by_index)==0

    save_typed_struct_spec(gn, toNS("p:Properties"), graph.properties)

    shared_code=""
    if graph.shared_code is not None:
        for code in graph.shared_code:
            shared_code += code+"\n";
    add_code_element(gn, "p:SharedCode", shared_code)

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
    for st in graph.supervisor_types.values():
        save_supervisor_type(dtn,st)

    return gn

def save_graph_instance(parent, graph):
    
    gn = etree.SubElement(parent, toNS("p:GraphInstance"));
    gn.attrib["id"]=graph.id
    gn.attrib["graphTypeId"]=graph.graph_type.id

    add_documentation_comment(gn, graph.documentation)

    properties=None
    if graph.graph_type.properties:
        properties=graph.graph_type.properties.expand(graph.properties)
    save_typed_struct_instance_attrib(gn, "P", graph.graph_type.properties, properties)

    din = etree.Element(toNS("p:DeviceInstances"))
    gn.append(din)
    for i in graph.device_instances.keys():
        save_device_instance(din, graph.device_instances[i] )

    ein = etree.Element(toNS("p:EdgeInstances"))
    gn.append(ein)
    for i in graph.edge_instances.keys():
        assert( graph.edge_instances[i].dst_device.id in graph.device_instances )
        assert( graph.edge_instances[i].src_device.id in graph.device_instances )
        save_edge_instance(ein, graph.edge_instances[i] )

    return gn


def save_graph(graph:Union[GraphType,GraphInstance], dst):
    if isinstance(dst,str):
        if dst.endswith(".gz"):
            import gzip
            with gzip.open(dst, 'wt', compresslevel=6) as dstFile:
                return save_graph(graph,dstFile)
        else:
            with open(dst,"wt") as dstFile:
                assert not isinstance(dstFile,str)
                return save_graph(graph,dstFile)

    ####################################################
    ## dst is some kind of stream

    nsmap = { None : "https://poets-project.org/schemas/virtual-graph-schema-v4" }
    root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)
    root.attrib["formatMinorVersion"]="0"


    if isinstance(graph,GraphInstance):
        gt=graph.graph_type
        gi=graph
    else:
        assert isinstance(graph,GraphType), f"type = { type(graph)}"
        gt=graph
        gi=None

    sys.stderr.write("save_graph: Constructing graph type tree\n")
    save_graph_type(root,gt)
    if gi is not None:
        sys.stderr.write("save_graph: Constructing graph inst tree\n")
        save_graph_instance(root,gi)

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
