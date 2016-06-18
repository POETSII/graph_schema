#/bin/python

import xml.etree.ElementTree as ET
from lxml import etree


import os
import sys

ns={"p":"TODO/POETS/virtual-graph-schema-v0"}

def deNS(t):
    tt=t.replace("{"+ns["p"]+"}","p:")
    #print("{} -> {}".format(t,tt))
    return tt


scalarTypes={"p:Float32","p:Int32","p:Bool"}

class GraphDescriptionError(Exception):
    def __init__(self,msg):
        Exception.__init__(self,msg)

class XMLSyntaxError(Exception):
    def __init__(self,msg,node):
        if node==None:
            Exception.__init__(self, "Parse error at line <unknown> : {}".format(msg))
        else:
            Exception.__init__(self, "Parse error at line {} : {}".format(node.sourceline,msg))
        self.node=node
        

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

class TypedData(object):
    def __init__(self,name):
        self.name=name

class ScalarData(TypedData):
    def __init__(self,elt,value):
        TypedData.__init__(self,elt)
        if value is not None:
            self.value=self._check_value(value)
        else:
            self.value=None
        self.is_complete=value is not None

    def _check_value(self,value):
        raise NotImplementedError()

    def is_refinement_compatible(self,inst):
        if inst is None:
            return self.is_complete # If we have a default, then the instance can be none
        if not isinstance(inst,type(self)):
            return False # We are strict, it must be exactly the same type
        if inst.name!=self.name:
            return False
        return self.is_complete or inst.is_complete # As long as one of us has a value we are ok

    
    
class Int32Data(ScalarData):
    def __init__(self,name,value):
        ScalarData.__init__(self,name,value)
        
    def _check_value(self,value):
        return int(value)


class Float32Data(ScalarData):
    def __init__(self,name,value):
        ScalarData.__init__(self,name,value)
        
    def _check_value(self,value):
        return float(value)


class BoolData(ScalarData):
    def __init__(self,name,value):
        ScalarData.__init__(self,name,value)
        
    def _check_value(self,value):
        return bool(value)


class TupleData(TypedData):
    def __init__(self,name,elements):
        TypedData.__init__(self,name)
        self._elts_by_name={}
        self._elts_by_index=[]
        self.is_complete=True
        for e in elements:
            if e.name in self_elts_by_name:
                raise GraphDescriptionError("Tuple element name appears twice.")
            self._elts_by_name[e.name]=e
            self._elts_by_index.push(e)
            self.is_complete=self.is_complete and e.is_complete

    def is_refinement_compatible(self,inst):
        if not instanceof(inst,TupleDataType):
            return False

        # Everything that isn't complete must be specified
        for ee in self._eltsByName:
            if ee.name not in inst._eltsByName:
                if not ee.is_complete:
                    return False
            else:
                if not ee.is_refinement_compatible(self._eltsByName):
                    return False

        # Check that everything in the instance is known
        for ee in inst._eltsByName:
            if ee.name not in self._eltsByName:
                return False
        return True

def is_refinement_compatible(proto,inst):
    if proto is None:
        return inst is None
    if inst is None:
        return proto.is_complete
    return proto.is_refinement_compatible(inst)
    
    
def load_typed_data(dt):
    name=get_attrib(dt,"name")
    
    tag=deNS(dt.tag)
    if tag=="p:Tuple":
        elts=[]
        for eltNode in dt.findall("p:*"): # Anything from this namespace must be a member
            elt=load_typed_data(eltNode)
            elts.push(elt)
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


class EdgeType(object):
    def __init__(self,parent,id,message,state,properties):
        self.id=id
        self.parent=parent
        self.message=message
        self.state=state
        self.properties=properties
    

class Port(object):
    def __init__(self,parent,name,edge_type):
        self.parent=parent
        self.name=name
        self.edge_type=edge_type

    
class InputPort(Port):
    def __init__(self,parent,name,edge_type,receive_handler):
        Port.__init__(self,parent,name,edge_type)
        self.receive_handler=receive_handler

    
class OutputPort(Port):
    def __init__(self,parent,name,edge_type,send_handler):
        Port.__init__(self,parent,name,edge_type)
        self.send_handler=send_handler
    
            
class DeviceType(object):
    def __init__(self,parent,id,state,properties):
        self.id=id
        self.parent=parent
        self.state=state
        self.properties=properties
        self.inputs={}
        self.outputs={}
        self.ports={}

    def add_input(self,name,edge_type,receive_handler):
        if name in self.ports:
            raise GraphDescriptionError("Duplicate port {} on device type {}".format(name,self.id))
        if edge_type.id not in self.parent.edge_types:
            raise GraphDescriptionError("Unregistered edge type {} on port {} of device type {}".format(edge_type.id,name,self.id))
        if edge_type != self.parent.edge_types[edge_type.id]:
            raise GraphDescriptionError("Incorrect edge type object {} on port {} of device type {}".format(edge_type.id,name,self.id))
        p=InputPort(self, name, self.parent.edge_types[edge_type.id], receive_handler)
        self.inputs[name]=p
        self.ports[name]=p
        print("  added {}".format(name))

    def add_output(self,name,edge_type,send_handler):
        if name in self.ports:
            raise GraphDescriptionError("Duplicate port {} on device type {}".format(name,self.id))
        if edge_type.id not in self.parent.edge_types:
            raise GraphDescriptionError("Unregistered edge type {} on port {} of device type {}".format(edge_type.id,name,self.id))
        if edge_type != self.parent.edge_types[edge_type.id]:
            raise GraphDescriptionError("Incorrect edge type object {} on port {} of device type {}".format(edge_type.id,name,self.id))
        p=OutputPort(self, name, self.parent.edge_types[edge_type.id], send_handler)
        self.outputs[name]=p
        self.ports[name]=p
        print("  added {}".format(name))

class DeviceInstance(object):
    def __init__(self,parent,id,device_type,properties):
        if not is_refinement_compatible(device_type.properties,properties):
            raise GraphDescriptionError("Properties not compatible with device type properties.")
        
        self.parent=parent
        self.id=id
        self.device_type=device_type
        self.properties=properties

class EdgeInstance(object):
    def __init__(self,parent,dst_device,dst_port_name,src_device,src_port_name,properties):
        self.parent=parent

        if dst_port_name not in dst_device.device_type.inputs:
            raise GraphDescriptionError("Port '{}' does not exist on dest device type '{}'".format(dst_port_name,dst_device.device_type.id))
        if src_port_name not in src_device.device_type.outputs:
            raise GraphDescriptionError("Port '{}' does not exist on src device type '{}'".format(src_port_name,src_device.device_type.id))

        dst_port=dst_device.device_type.inputs[dst_port_name]
        src_port=src_device.device_type.outputs[src_port_name]
        
        if dst_port.edge_type != src_port.edge_type:
            raise GraphDescriptionError("Dest port has type {}, source port type {}".format(dst_port.id,src_port.id))

        if not is_refinement_compatible(dst_port.edge_type.properties,properties):
            raise GraphDescriptionError("Properties are not compatible.")

        # We create a local id to ensure uniqueness of edges, but this is not persisted
        self.id = (dst_device.id,dst_port_name,src_device.id,src_port_name)
        
        self.dst_device=dst_device
        self.src_device=src_device
        self.edge_type=dst_port.edge_type
        self.dst_port=dst_port
        self.src_port=src_port
        self.properties=properties
        

class Graph:
    def __init__(self,id):
        self.edge_types={}
        self.device_types={}
        self.device_instances={}
        self.edge_instances={}
        self._validated=True

    def _validate_edge_type(self,et):
        pass

    def _validate_device_type(self,dt):
        for p in dt.ports.values():
            if p.edge_type.id not in self.edge_types:
                raise GraphDescriptionError("DeviceType uses an edge type that is uknown.")
            if p.edge_type != self.edge_types[p.edge_type.id]:
                raise GraphDescriptionError("DeviceType uses an edge type object that is not part of this graph.")

    def _validate_device_instance(self,di):
        if di.device_type.id not in self.device_types:
            raise GraphDescriptionError("DeviceInstance refers to unknown device type.")
        if di.device_type != self.device_types[di.device_type.id]:
            raise GraphDescriptionError("DeviceInstance refers to a device tye object that is not part of this graph.")

        if not is_refinement_compatible(di.device_type.properties,di.properties):
            raise GraphDescriptionError("DeviceInstance properties don't match device type.")
            

    def _validate_edge_instance(self,ei):
        pass

            
    def add_edge_type(self,et,validate=True):
        if et.id in self.edge_types:
            raise GraphDescriptionError("Duplicate edgeType id {}".format(id))

        if validate:
            self._validate_edge_type(et)
        else:
            self._validated=False
        
        self.edge_types[et.id]=et

    def add_device_type(self,dt,validate=True):
        if dt.id in self.edge_types:
            raise GraphDescriptionError("Duplicate deviceType id {}".format(id))

        if validate:
            self._validate_device_type(dt)
        else:
            self._validated=False
        
        self.device_types[dt.id]=dt

    def add_device_instance(self,di,validate=True):
        if di.id in self.device_instances:
            raise GraphDescriptionError("Duplicate deviceInstance id {}".format(id))

        if validate:
            self._validate_device_instance(di)
        else:
            self._validated=False

        self.device_instances[di.id]=di

    def add_edge_instance(self,ei,validate=True):
        if ei.id in self.edge_instances:
            raise GraphDescriptionError("Duplicate edgeInstance id {}".format(id))

        if validate:
            self._validate_edge_instance(ei)
        else:
            self._validated=False

        self.edge_instances[ei.id]=ei

    def validate(self):
        if self._validated:
            return True
        
        for et in self.edge_types:
            self._validate_edge_type(et)
        for dt in self.device_types:
            self._validate_device_type(et)
        for di in self.device_instances:
            self._validate_device_instance(di)
        for ei in self.edge_instances:
            self._validate_edge_instances(ei)

        self._validated=True

    
def load_edge_type(parent,dt):
    id=get_attrib(dt,"id")
    
    message=None
    messageNode=dt.find("p:Message",ns)
    if messageNode is not None:
        assert(len(messageNode)==1)
        message=load_typed_data(messageNode[0])

    state=None
    stateNode=dt.find("p:State",ns)
    if stateNode is not None:
        assert(len(state)==1)
        tate=load_typed_data(stateNode[0])

    properties=None
    propertiesNode=dt.find("p:Properties",ns)
    if propertiesNode is not None:
        assert(len(propertiesNode)==1)
        properties=load_typed_data(propertiesNode[0])

    return EdgeType(parent,id,message,state,properties)

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
    if propertiesNode:
        assert(len(propertiesNode[0])==1)
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
    if propertiesNode:
        assert(len(propertiesNode[0])==1)
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

        for etNode in graphNode.find("p:EdgeTypes",ns):
            et=load_edge_type(graph,etNode)
            graph.add_edge_type(et)

        for dtNode in graphNode.find("p:DeviceTypes",ns):
            dt=load_device_type(graph,dtNode)
            graph.add_device_type(dt)

        for diNode in graphNode.find("p:DeviceInstances",ns):
            di=load_device_instance(graph,diNode)
            graph.add_device_instance(di)

        for eiNode in graphNode.find("p:EdgeInstances",ns):
            ei=load_edge_instance(graph,eiNode)
            graph.add_edge_instance(ei)

        return graph
    except XMLSyntaxError as e:
        print(e)
        if e.node is not None:
            print(etree.tostring(e.node, pretty_print = True, encoding='utf-8').decode("utf-8"))

load_graph(sys.stdin)
