from graph.core import *

import xml.etree.ElementTree as ET
from lxml import etree

from typing import *

import re
import os
import sys
import json

v3_namespace_uri = "https://poets-project.org/schemas/virtual-graph-schema-v3"

ns={"p":v3_namespace_uri}

# Precalculate, as these are on inner loop for (DevI|ExtI) and EdgeI
_ns_P="{{{}}}P".format(ns["p"])
_ns_S="{{{}}}S".format(ns["p"])
_ns_M="{{{}}}M".format(ns["p"])
_ns_DevI="{{{}}}DevI".format(ns["p"])
_ns_ExtI="{{{}}}ExtI".format(ns["p"])
_ns_EdgeI="{{{}}}EdgeI".format(ns["p"])

# Precalculate for parsing of the device types (DeviceType | ExternalType )
_ns_DeviceType="{{{}}}DeviceType".format(ns["p"])
_ns_ExternalType="{{{}}}ExternalType".format(ns["p"])

def deNS(t, namespace=None):
    if namespace==None:
        namespace=ns
    tt=t.replace("{"+namespace["p"]+"}","p:")
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

def get_attrib_optional_bool(node,name):
    if name not in node.attrib:
        return False
    v=node.attrib[name]
    if v=="false" or v=="0":
        return False;
    if v=="true" or v=="1":
        return True;
    raise XMLSyntaxError("Couldn't convert value {} to bool".format(v, node))


def get_attrib_defaulted(node,name,default):
    if name not in node.attrib:
        return default
    return node.attrib[name]

def get_child_text(node,name,namespace=None):
    if namespace==None:
        namespace=ns
    n=node.find(name,namespace)
    if n is None:
        raise XMLSyntaxError("No child text node called {}".format(name),node)
    text=src=etree.tostring(n, method="text",encoding=str)
    line=n.sourceline
    return (text,line)

def load_documentation_of_typed_data(node, member, namespace=None):
    if namespace == None:
        namespace = ns

    documentation = None
    docNode = node.find("p:Documentation", namespace)
    if docNode is not None:
        member.add_documentation(docNode.text)

def load_typed_data_spec(dt, namedTypes={}, namespace=None, loadDocumentation=False):
    if namespace==None:
        namspace=ns

    name=get_attrib(dt,"name")

    tag=deNS(dt.tag,namespace)
    if tag=="p:Tuple":
        elts=[]
        default=None
        documentation=None
        for eltNode in dt.findall("p:*",namespace): # Anything from this namespace must be a member
            if deNS(eltNode.tag) == "p:Documentation":
                if loadDocumentation:
                    documentation = eltNode.text
            elif not deNS(eltNode.tag) == "p:Default":
                elt=load_typed_data_spec(eltNode, namedTypes,namespace,loadDocumentation)
                elts.append(elt)
            else:
                default = json.loads('{'+eltNode.text+'}')
        member = TupleTypedDataSpec(name,elts,default,documentation)
        # return TupleTypedDataSpec(name,elts,default)
    elif tag=="p:Array":
        length=int(get_attrib(dt,"length"))
        type=get_attrib_optional(dt, "type")
        default=None
        documentation = None
        d=dt.find("p:Default", namespace)
        if d is not None:
            if d.text.strip().startswith('['):
                default=json.loads('{"'+name+'":'+d.text+'}')
            else:
                default=json.loads('{"'+name+'": {'+d.text+'} }')
            default=default[name]
        if type:
            if type in namedTypes:
                type=namedTypes[type]
            else:
                type=ScalarTypedDataSpec("_",type)
        else:
            subs=dt.findall("p:*", namespace) # Anything from this namespace must be the sub-type other than <Default>
            d = dt.find("p:Documentation", namespace)
            if d is not None:
                if loadDocumentation:
                    documentation = d.text
                subs.remove(d)
            d = dt.find("p:Default", namespace)
            if default:
                if len(subs)!=2:
                    raise RuntimeError("If there is no type attribute, there should be exactly one sub-type element.")
                if deNS(subs[0].tag) == "p:Default":
                    type=load_typed_data_spec(subs[1], namedTypes, namespace, loadDocumentation)
                else:
                    type=load_typed_data_spec(subs[0], namedTypes, namespace, loadDocumentation)
            elif len(subs)!=1:
                raise RuntimeError("If there is no type attribute, there should be exactly one sub-type element.")
            else:
                type=load_typed_data_spec(subs[0], namedTypes, namespace, loadDocumentation)
        member = ArrayTypedDataSpec(name,length,type,default,documentation)
        # return ArrayTypedDataSpec(name,length,type,default)
    elif tag=="p:Scalar":
        type=get_attrib(dt, "type")
        default=get_attrib_optional(dt, "default")
        if default:
            default=json.loads(default) # needed for typedef'd structs
        else:
            d = dt.find("p:Default", namespace)
            if d is not None:
                if d.text.strip().startswith('['):
                    default=json.loads('{"'+name+'":'+d.text+'}')
                else:
                    default=json.loads('{"'+name+'": {'+d.text+'} }')
                default=default[name]

        documentation = None
        if loadDocumentation:
            d=dt.find("p:Documentation", namespace)
            if d is not None:
                documentation = d.text

        # sys.stderr.write("namedTypes={}\n".format(namedTypes))

        if type in namedTypes:
            type=namedTypes[type]
        member = ScalarTypedDataSpec(name,type,default,documentation)
        # return ScalarTypedDataSpec(name,type,default)
    elif tag=="p:Union":
        # TODO: Implement unions
        # pass
        raise RuntimeError("Unions have not yet been implemented")
    else:
        raise XMLSyntaxError("Unknown data type '{}'.".format(tag), dt)
    if loadDocumentation:
        load_documentation_of_typed_data(dt, member, namespace)
    return member

def load_typed_data_instance(dt,spec):
    if dt is None:
        return None
    value=json.loads(dt.text)
    assert(spec.is_refinement_compatible(value))
    return spec.expand(value)


def load_struct_spec(name, members,namedTypes,namespace=None,loadDocumentation=False):
    if namespace==None:
        namespace=ns
    elts=[]
    for eltNode in members.findall("p:*",namespace): # Anything from this namespace must be a member
        elt=load_typed_data_spec(eltNode,namedTypes,namespace,loadDocumentation)
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

def load_metadata(parent, name, namespace=None):
    if namespace==None:
        namespace=ns
    metadata={}
    metadataNode=parent.find(name,namespace)
    if metadataNode is not None and metadataNode.text is not None:
        try:
            metadata=json.loads("{"+metadataNode.text+"}")
        except:
            sys.stderr.write("Couldn't parse '{}'".format(metadataNode.text))
            raise

    return metadata

def load_type_def(parent,td, namedTypes, namespace=None, loadDocumentation=False):
    if namespace==None:
        namespace = ns

    id=get_attrib(td,"id")
    documentation=None
    try:
        typeNodes=td.findall("p:*",namespace)
        assert len(typeNodes) <= 2, "Typedef can hold one data type and documentation only." + str(len(typeNodes))
        for typeNode in typeNodes:
            if typeNode is None:
                raise XMLSyntaxError("Missing sub-child to give actual type.")
            elif deNS(typeNode.tag, namespace) == "p:Documentation":
                if loadDocumentation:
                    documentation = typeNode.text
            else:
                type=load_typed_data_spec(typeNode, namedTypes, namespace, loadDocumentation)

        return Typedef(id,type,documentation=documentation)
    except XMLSyntaxError:
            raise
    except Exception as e:
        raise XMLSyntaxError("Error while parsing type def {}".format(id),td,e)


def load_message_type(parent,dt, namedTypes, namespace=None, loadDocumentation=False):
    if namespace==None:
        namespace=ns
    id=get_attrib(dt,"id")
    try:
        message=None
        messageNode=dt.find("p:Message",namespace)
        if messageNode is not None:
            message=load_struct_spec(id+"_message", messageNode, namedTypes, namespace, loadDocumentation)

        metadata=load_metadata(dt, "p:MetaData")

        documentation=None
        if loadDocumentation:
            docNode=dt.find("p:Documentation",namespace)
            if docNode is not None:
                documentation=docNode.text

        return MessageType(parent,id,message,metadata,documentation=documentation)
    except XMLSyntaxError:
            raise
    except Exception as e:
        raise XMLSyntaxError("Error while parsing message {}".format(id),dt,e)

def load_external_type(graph,dtNode,sourceFile, namespace=None, loadDocumentation=False):
    if namespace==None:
        namespace=ns
    id=get_attrib(dtNode,"id")
    state=None
    properties=None
    metadata=load_metadata(dtNode, "p:MetaData")
    shared_code=[]

    documentation=None
    if loadDocumentation:
        docNode=dtNode.find("p:Documentation",namespace)
        if docNode is not None:
            documentation=docNode.text

    dt=DeviceType(graph,id,properties,state,metadata,shared_code, True, documentation)

    for p in dtNode.findall("p:InputPin", namespace):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        message_type=graph.message_types[message_type_id]
        state=None # pins of ExternalType cannot have any state, properties, or handlers
        properties=None
        handler=''
        is_application=False #na to external devices (they are all application pins)
        sourceLine=0

        pinMetadata=load_metadata(p,"p:MetaData")
        dt.add_input(name,message_type,is_application,properties,state,pinMetadata, handler,sourceFile,sourceLine)
        sys.stderr.write("      Added external input {}\n".format(name))

    for p in dtNode.findall("p:OutputPin", namespace):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        message_type=graph.message_types[message_type_id]
        is_application=False #na to external devices (they are all application pins)
        handler=''
        sourceLine=0

        pinMetadata=load_metadata(p,"p:MetaData")
        dt.add_output(name,message_type,is_application,pinMetadata,handler,sourceFile,sourceLine)
        sys.stderr.write("      Added external output {}\n".format(name))

    dt.ready_to_send_handler=''
    dt.ready_to_send_source_line=0
    dt.ready_to_send_source_file=None

    return dt

def load_device_type(graph,dtNode,sourceFile,namespace=None,loadDocumentation=False):
    if namespace==None:
        namespace=ns

    id=get_attrib(dtNode,"id")

    state=None
    stateNode=dtNode.find("p:State",namespace)
    if stateNode is not None:
        state=load_struct_spec(id+"_state", stateNode,graph.typedefs,namespace,loadDocumentation)
    
    properties=None
    propertiesNode=dtNode.find("p:Properties",namespace)
    if propertiesNode is not None:
        properties=load_struct_spec(id+"_properties", propertiesNode,graph.typedefs, namespace, loadDocumentation)

    metadata=load_metadata(dtNode,"p:MetaData")

    shared_code=[]
    for s in dtNode.findall("p:SharedCode",namespace):
        shared_code.append(s.text)

    documentation=None
    if loadDocumentation:
        docNode=dtNode.find("p:Documentation",namespace)
        if docNode is not None:
            documentation=docNode.text

    dt=DeviceType(graph,id,properties,state,metadata,shared_code,isExternal=False,documentation=documentation)

    if dtNode.find("p:OnInit",namespace) is not None:
        (handler,sourceLine)=get_child_text(dtNode,"p:OnInit", namespace)
        dt.init_handler=handler
        dt.init_source_line=sourceLine
        dt.init_source_file=sourceFile

    for p in dtNode.findall("p:InputPin",namespace):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        message_type=graph.message_types[message_type_id]
        # NOTE: application pin support needed for as long as 2to3 is relevant.
        is_application=get_attrib_optional_bool(p,"application") # TODO: REMOVE APPLICATION PIN

        try:
            state=None
            stateNode=p.find("p:State",namespace)
            if stateNode is not None:
                state=load_struct_spec(id+"_state", stateNode,graph.typedefs, namespace, loadDocumentation)
        except Exception as e:
            raise XMLSyntaxError("Error while parsing state of pin {} : {}".format(id,e),p,e)

        try:
            properties=None
            propertiesNode=p.find("p:Properties",namespace)
            if propertiesNode is not None:
                properties=load_struct_spec(id+"_properties", propertiesNode,graph.typedefs, namespace, loadDocumentation)
        except Exception as e:
            raise XMLSyntaxError("Error while parsing properties of pin {} : {}".format(id,e),p,e)

        pinMetadata=load_metadata(p,"p:MetaData")

        documentation = None
        if loadDocumentation:
            docNode = p.find("p:Documentation", namespace)
            if docNode is not None:
                documentation = docNode.text

        (handler,sourceLine)=get_child_text(p,"p:OnReceive",namespace)
        dt.add_input(name,message_type,is_application,properties,state,pinMetadata, handler,sourceFile,sourceLine,documentation)
        #sys.stderr.write("      Added input {}\n".format(name))

    for p in dtNode.findall("p:OutputPin",namespace):
        name=get_attrib(p,"name")
        message_type_id=get_attrib(p,"messageTypeId")
        if message_type_id not in graph.message_types:
            raise XMLSyntaxError("Unknown messageTypeId {}".format(message_type_id),p)
        is_application=get_attrib_optional_bool(p,"application")
        is_indexed=get_attrib_optional_bool(p,"indexed")
        message_type=graph.message_types[message_type_id]
        pinMetadata=load_metadata(p,"p:MetaData")
        (handler,sourceLine)=get_child_text(p,"p:OnSend",namespace)
        documentation = None
        if loadDocumentation:
            docNode = p.find("p:Documentation", namespace)
            if docNode is not None:
                documentation = docNode.text
        dt.add_output(name,message_type,is_application,pinMetadata,handler,sourceFile,sourceLine,documentation,is_indexed)

    (handler,sourceLine)=get_child_text(dtNode,"p:ReadyToSend",namespace)
    dt.ready_to_send_handler=handler
    dt.ready_to_send_source_line=sourceLine
    dt.ready_to_send_source_file=sourceFile

    # Tags with no implementation

    i = dtNode.find("p:OnHardwareIdle", namespace)
    if i is not None:
        (handler,sourceLine)=get_child_text(dtNode,"p:OnHardwareIdle",namespace)
        dt.on_hardware_idle_handler=handler
        dt.on_hardware_idle_source_line=sourceLine
        dt.on_hardware_idle_source_file=sourceFile

    i = dtNode.find("p:OnThreadIdle", namespace)
    if i is not None:
        raise RuntimeError("OnThreadIdle has not been implemented")

    i = dtNode.find("p:OnDeviceIdle", namespace)
    if i is not None:
        (handler,sourceLine)=get_child_text(dtNode,"p:OnDeviceIdle",namespace)
        dt.on_device_idle_handler=handler
        dt.on_device_idle_source_line=sourceLine
        dt.on_device_idle_source_file=sourceFile

    return dt

def load_graph_type(graphNode, sourcePath, namespace=None, loadDocumentation=False):
    if namespace==None:
        namespace=ns
    deviceTypeTag = "{{{}}}DeviceType".format(namespace["p"])
    externalTypeTag = "{{{}}}ExternalType".format(namespace["p"])

    id=get_attrib(graphNode,"id")
    #sys.stderr.write("  Loading graph type {}\n".format(id))

    namedTypes={}
    namedTypesByIndex=[]
    for etNode in graphNode.findall("p:Types/p:TypeDef",namespace):
        #sys.stderr.write("  Loading type defs, current={}\n".format(namedTypes))
        td=load_type_def(graphNode, etNode, namedTypes, namespace, loadDocumentation)
        namedTypes[td.id]=td
        namedTypesByIndex.append(td)

    properties=None
    propertiesNode=graphNode.find("p:Properties",namespace)
    if propertiesNode is not None:
        properties=load_struct_spec(id+"_properties", propertiesNode, namedTypes, namespace, loadDocumentation)

    metadata=load_metadata(graphNode,"p:MetaData",namespace)

    documentation=None
    if loadDocumentation:
        docNode=graphNode.find("p:Documentation",namespace)
        if docNode is not None:
            documentation = docNode.text

    shared_code=[]
    for n in graphNode.findall("p:SharedCode",namespace):
        shared_code.append(n.text)

    graphType=GraphType(id,properties,metadata,shared_code,documentation)

    for nt in namedTypesByIndex:
        graphType.add_typedef(nt)

    for etNode in graphNode.findall("p:MessageTypes/p:*",namespace):
        et=load_message_type(graphType,etNode, graphType.typedefs, namespace, loadDocumentation)
        graphType.add_message_type(et)

    for dtNode in graphNode.findall("p:DeviceTypes/p:*",namespace):

        if dtNode.tag == deviceTypeTag:
            dt=load_device_type(graphType, dtNode, sourcePath, namespace, loadDocumentation)
            graphType.add_device_type(dt)
            #sys.stderr.write("    Added device type {}\n".format(dt.id))
        elif dtNode.tag == externalTypeTag:
            et=load_external_type(graphType,dtNode,sourcePath, namespace)
            graphType.add_device_type(et)
            #sys.stderr.write("    Added external device type {}\n".format(et.id))
        elif dtNode.tag == ("{{{}}}SupervisorType".format(namespace["p"])):
            raise RuntimeError("Supervisor Types have not been implemented")

    return graphType

def load_graph_type_reference(graphNode,basePath,namespace=None):
    if namespace==None:
        namespace=ns
    id=get_attrib(graphNode,"id")

    src=get_attrib_optional(graphNode, "src")
    if src:
        baseDir=os.path.dirname(basePath)
        fullSrc=os.path.join(baseDir, src)
        print("  basePath = {}, src = {}, fullPath = {}".format(basePath,src,fullSrc))

        tree = etree.parse(fullSrc)
        doc = tree.getroot()
        graphsNode = doc;

        for gtNode in graphsNode.findall("p:GraphType",namespace):
            gt=load_graph_type(gtNode, fullSrc)
            if gt.id==id:
                return gt

        raise XMLSyntaxError("Couldn't load graph type '{}' from src '{}'".format(id,src))
    else:
        return GraphTypeReference(id)

def load_external_instance(graph, eiNode, namespace=None):
    if namespace==None:
        namespace = ns
    pTag = "{{{}}}M".format(namespace["p"])

    id=get_attrib(eiNode,"id")
    external_type_id=get_attrib(eiNode,"type")
    if external_type_id not in graph.graph_type.device_types:
        raise XMLSyntaxError("Unknown external type id {}, known devices = [{}]".format(external_type_id, [d.di for d in graph.graph_type.deivce_types.keys()]), eiNode)
    external_type=graph.graph_type.device_types[external_type_id]
    properties=None # external devices cannot have any properties
    metadata=None

    for n in eiNode: # walk over children rather than using find. Better performance
        if n.tag == mTag:
            assert not metadata
            metadata=json.loads("{"+n.text+"}")
        else:
            assert "Unknown tag type in EdgeI"

    return DeviceInstance(graph,id,external_type,properties,metadata)

def load_device_instance(graph,diNode,namespace=None):
    if namespace==None:
        namespace=ns
    pTag = "{{{}}}P".format(namespace["p"])
    sTag = "{{{}}}S".format(namespace["p"])
    mTag = "{{{}}}M".format(namespace["p"])

    id=get_attrib(diNode,"id")
    device_type_id=get_attrib(diNode,"type")
    if device_type_id not in graph.graph_type.device_types:
        raise XMLSyntaxError("Unknown device type id {}, known device types = {}".format(device_type_id,
                        [id for d in graph.graph_type.device_types.keys()]
                    ),
                    diNode
                )
    device_type=graph.graph_type.device_types[device_type_id]

    properties=None
    state=None
    metadata=None

    for n in diNode: # walk over children rather than using find. Better performance
        if n.tag==pTag:
            assert not properties

            spec=device_type.properties
            assert spec is not None, "Can't have properties value for device with no properties spec"

            value=json.loads("{"+n.text+"}")
            assert spec.is_refinement_compatible(value), "Spec = {}, value= {}".format(spec,value)
            properties=spec.expand(value)
        elif n.tag==sTag:
            assert not state

            spec=device_type.state
            assert spec is not None, "Can't have state value for device with no state spec"
            value=json.loads("{"+n.text+"}")
            assert spec.is_refinement_compatible(value), "Spec = {}, value= {}".format(spec,value)

            state=spec.expand(value)
        elif n.tag==mTag:
            assert not metadata
            metadata=json.loads("{"+n.text+"}")
        else:
            assert "Unknown tag type in EdgeI"

    return DeviceInstance(graph,id,device_type,properties,state,metadata)

def split_endpoint(endpoint,node):
    parts=endpoint.split(':')
    if len(parts)!=2:
        raise XMLSyntaxError("Path does not contain exactly two elements",node)
    return (parts[0],parts[1])


def split_path(path,node):
    """Splits a path up into (dstDevice,dstPin,srcDevice,srcPin)"""
    parts=path.split('-')
    if len(parts)!=2:
        raise XMLSyntaxError("Path does not contain exactly two endpoints",node)
    return split_endpoint(parts[0],node)+split_endpoint(parts[1],node)

_split_path_re=re.compile("^([^:]+):([^-]+)-([^:]+):([^-]+)$")

def load_edge_instance(graph,eiNode):
    path=eiNode.attrib["path"]
    (dst_device_id,dst_pin_name,src_device_id,src_pin_name)=_split_path_re.match(path).groups()

    send_index=eiNode.attrib.get("sendIndex")

    dst_device=graph.device_instances[dst_device_id]
    src_device=graph.device_instances[src_device_id]

    assert dst_pin_name in dst_device.device_type.inputs, "Couldn't find input pin called '{}' in device type '{}'. Inputs are [{}]".format(dst_pin_name,dst_device.device_type.id, [p for p in dst_device.device_type.inputs])
    assert src_pin_name in src_device.device_type.outputs

    properties=None
    state=None
    metadata=None
    for n in eiNode: # walk over children rather than using find. Better performance
        if n.tag==_ns_P:
            assert not properties

            spec=dst_device.device_type.inputs[dst_pin_name].properties
            assert spec is not None, "Can't have properties value for edge with no properties spec"

            value=json.loads("{"+n.text+"}")
            assert(spec.is_refinement_compatible(value))
            properties=spec.expand(value)
        elif n.tag==_ns_S:
            assert not state

            spec=dst_device.device_type.inputs[dst_pin_name].state
            assert spec is not None, "Can't have state value for edge with no state spec"

            value=json.loads("{"+n.text+"}")
            assert(spec.is_refinement_compatible(value))
            state=spec.expand(value)
        elif n.tag==_ns_M:
            assert not metadata
            metadata=json.loads("{"+n.text+"}")
        else:
            assert "Unknown tag type in EdgeI"

    return EdgeInstance(graph,dst_device,dst_pin_name,src_device,src_pin_name,properties,metadata, send_index, state=state)

def load_graph_instance(graphTypes, graphNode, namespace=None, loadDocumentation=False):
    if namespace==None:
        namespace=ns
    devITag = "{{{}}}DevI".format(namespace["p"])
    extITag = "{{{}}}ExtI".format(namespace["p"])
    edgeITag = "{{{}}}EdgeI".format(namespace["p"])

    id=get_attrib(graphNode,"id")
    graphTypeId=get_attrib(graphNode,"graphTypeId")
    graphType=graphTypes[graphTypeId]

    properties=None
    propertiesNode=graphNode.find("p:Properties",namespace)
    if propertiesNode is not None:
        assert graphType.properties
        properties=load_struct_instance(graphType.properties, propertiesNode)

    metadata=load_metadata(graphNode, "p:MetaData")

    documentation=None
    if loadDocumentation:
        docNode=graphNode.find("p:Documentation",namespace)
        if docNode is not None:
            documentation=docNode.text

    graph=GraphInstance(id,graphType,properties,metadata, documentation)
    disNode=graphNode.findall("p:DeviceInstances",namespace)
    assert(len(disNode)==1)
    for diNode in disNode[0]:
        assert (diNode.tag==devITag) or (diNode.tag==extITag)
        if diNode.tag==devITag:
            di=load_device_instance(graph,diNode,namespace)
            graph.add_device_instance(di)
        elif diNode.tag==extITag:
            ei=load_external_instance(graph,diNode)
            graph.add_device_instance(ei)

    eisNode=graphNode.findall("p:EdgeInstances",namespace)
    assert(len(eisNode)==1)
    for eiNode in eisNode[0]:
        assert eiNode.tag==edgeITag
        ei=load_edge_instance(graph,eiNode)
        graph.add_edge_instance(ei)

    return graph


def load_graph_types_and_instances(src,basePath,namespace=None,loadDocumentation=False,skip_instance:Optional[bool]=False):
    if namespace==None:
        namespace=ns

    if isinstance(src, etree._Element):
        graphsNode=src
    else:
        tree = etree.parse(src)
        doc = tree.getroot()
        graphsNode = doc
    
    graphTypes={}
    graphs={}

    try:
        for gtNode in graphsNode.findall("p:GraphType",namespace):
            #sys.stderr.write("Loading graph type\n")
            gt=load_graph_type(gtNode, basePath, namespace, loadDocumentation)
            graphTypes[gt.id]=gt

        for gtRefNode in graphsNode.findall("p:GraphTypeReference",namespace):
            #sys.stderr.write("Loading graph reference\n")
            gt=load_graph_type_reference(gtRefNode, basePath, namespace)
            graphTypes[gt.id]=gt

        if not skip_instance:
            for giNode in graphsNode.findall("p:GraphInstance",namespace):
                #sys.stderr.write("Loading graph\n")
                g=load_graph_instance(graphTypes, giNode, namespace, loadDocumentation)
                graphs[g.id]=g

        return (graphTypes,graphs)

    except XMLSyntaxError as e:
        sys.stderr.write(str(e)+"\n")
        if e.node is not None:
            sys.stderr.write(etree.tostring(e.node, pretty_print = True, encoding='utf-8').decode("utf-8")+"\n")
        raise e

def v3_load_graph_types_and_instances(src,basePath, skip_instance:Optional[bool]=False):
    (graphTypes,graphInstances)=load_graph_types_and_instances(src,basePath, skip_instance=skip_instance)
    if len(graphInstances)==0:
        if len(graphTypes)==1:
            for x in graphTypes.values():
                return (x,None)
        else:
            raise RuntimeError("File contained no graph instances.")
    
    if len(graphInstances)>1:
        raise RuntimeError("File contained more than one graph instance.")
    for x in graphInstances.values():
        return (x.graph_type,x)

