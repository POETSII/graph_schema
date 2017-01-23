import sys

class GraphDescriptionError(Exception):
    def __init__(self,msg):
        Exception.__init__(self,msg)

class TypedDataSpec(object):
    def __init__(self,name):
        self.name=name


class ScalarTypedDataSpec(TypedDataSpec):
    def _check_value(self,value):
        if self.type=="int32_t":
            res=int(value)
            assert(-2**31 <= res < 2**31)
        elif self.type=="int16_t":
            res=int(value)
            assert(-2**15 <= res < 2**15)
        elif self.type=="int8_t":
            res=int(value)
            assert(-2**7 <= res < 2**7)
        elif self.type=="uint32_t":
            res=int(value)
            assert(0 <= res < 2**32)
        elif self.type=="uint16_t":
            res=int(value)
            assert(0 <= res < 2**16)
        elif self.type=="uint8_t":
            res=int(value)
            assert(0 <= res < 2**8)
        elif self.type=="float":
            res=float(value)
        else:
            assert False, "Unknown data type {}.".format(self.type)
        return res
    
    def __init__(self,name,type,value=None):
        TypedDataSpec.__init__(self,name)
        self.type=type
        if value is not None:
            self.value=self._check_value(value)
        else:
            self.value=0

    def is_refinement_compatible(self,inst):
        if inst is None:
            return True
        try:
            self._check_value(inst)
        except:
            return False
        return True

    def create_default(self):
        return self.value

    def expand(self,inst):
        if inst is None:
            return self.create_default()
        else:
            return self._check_value(inst)
        
    def __str__(self):
        return "{}:{}".format(self.type,self.name,self.value)

class TupleTypedDataSpec(TypedDataSpec):
    def __init__(self,name,elements):
        TypedDataSpec.__init__(self,name)
        self._elts_by_name={}
        self._elts_by_index=[]
        for e in elements:
            if e.name in self._elts_by_name:
                raise GraphDescriptionError("Tuple element name appears twice.")
            self._elts_by_name[e.name]=e
            self._elts_by_index.append(e)

    @property
    def elements_by_name(self):
        return self._elts_by_name

    @property
    def elements_by_index(self):
        return self._elts_by_index

    def __str__(self):
        
        acc="Tuple:{}[\n".format(self.name)
        for i in range(len(self._elts_by_index)):
            if i!=0:
                acc=acc+";"
            acc=acc+str(self._elts_by_index[i])
            acc=acc+"\n"
        return acc+"]"

    def create_default(self):
        return { e.name:e.create_default() for e in self._elts_by_index  }

    def expand(self,inst):
        if inst is None:
            return self.create_default()
        assert isinstance(inst,dict), "Want to expand dict, got '{}'".format(inst)
        for e in self._elts_by_index:
            inst[e.name]=e.expand(inst.get(e.name,None))
        return inst

    def is_refinement_compatible(self,inst):
        if inst is None:
            return True;
        
        if not isinstance(inst,dict):
            return False

        count=0
        for ee in self._elts_by_index:
            if ee.name in inst:
                count=count+1
                if not ee.is_refinement_compatible(inst[ee.name]):
                    return False

        # There is something in the dict that we don't know about
        if count!=len(inst):
            return False

        return True


class ArrayTypedDataSpec(TypedDataSpec):
    def __init__(self,name,length,type):
        TypedDataSpec.__init__(self,name)
        self.type=type
        self.length=length

    def __str__(self):
        
        return "Array:{}[{}*{}]\n".format(self.name,self.types,self.length)

    def create_default(self):
        return [self.type.create_default() for i in range(self.length)]

    def expand(self,inst):
        if inst is None:
            return self.create_default()
        assert isinstance(inst,list)
        assert len(inst)==self.length
        for i in range(self.length):
            inst[i]=self.type.expand(inst[i])
        return inst

    def is_refinement_compatible(self,inst):
        if inst is None:
            return True;
        
        if not isinstance(inst,list):
            return False

        if len(inst)!=self.length:
            return False

        for v in inst:
            if not self.type.is_refinement_compatible(v):
                return False

        return True


def create_default_typed_data(proto):
    if proto is None:
        return None
    else:
        return proto.create_default()

def expand_typed_data(proto,inst):
    if proto is None:
        assert inst is None
        return None
    else:
        return proto.expand(inst)

def is_refinement_compatible(proto,inst):
    if proto is None:
        return inst is None
    if inst is None:
        return True;
    return proto.is_refinement_compatible(inst)
    

class MessageType(object):
    def __init__(self,parent,id,message,metadata):
        self.id=id
        self.parent=parent
        self.message=message
        self.metadata=metadata
    

class Port(object):
    def __init__(self,parent,name,message_type,metadata,source_file,source_line):
        self.parent=parent
        self.name=name
        self.message_type=message_type
        self.metadata=metadata
        self.source_file=source_file
        self.source_line=source_line

    
class InputPort(Port):
    def __init__(self,parent,name,message_type,properties,state,metadata,receive_handler,source_file=None,source_line=None):
        Port.__init__(self,parent,name,message_type,metadata,source_file,source_line)
        self.properties=properties
        self.state=state
        self.receive_handler=receive_handler

    
class OutputPort(Port):
    def __init__(self,parent,name,message_type,metadata,send_handler,source_file,source_line):
        Port.__init__(self,parent,name,message_type,metadata,source_file,source_line)
        self.send_handler=send_handler
    
            
class DeviceType(object):
    def __init__(self,parent,id,properties,state,metadata=None):
        self.id=id
        self.parent=parent
        self.state=state
        self.properties=properties
        self.inputs={}
        self.inputs_by_index=[]
        self.outputs={}
        self.outputs_by_index=[]
        self.ports={}
        self.metadata=metadata

    def add_input(self,name,message_type,properties,state,metadata,receive_handler,source_file=None,source_line={}):
        if name in self.ports:
            raise GraphDescriptionError("Duplicate port {} on device type {}".format(name,self.id))
        if message_type.id not in self.parent.message_types:
            raise GraphDescriptionError("Unregistered message type {} on port {} of device type {}".format(message_type.id,name,self.id))
        if message_type != self.parent.message_types[message_type.id]:
            raise GraphDescriptionError("Incorrect message type object {} on port {} of device type {}".format(message_type.id,name,self.id))
        p=InputPort(self, name, self.parent.message_types[message_type.id], properties, state, metadata, receive_handler, source_file, source_line)
        self.inputs[name]=p
        self.inputs_by_index.append(p)
        self.ports[name]=p

    def add_output(self,name,message_type,metadata,send_handler,source_file=None,source_line=None):
        if name in self.ports:
            raise GraphDescriptionError("Duplicate port {} on device type {}".format(name,self.id))
        if message_type.id not in self.parent.message_types:
            raise GraphDescriptionError("Unregistered message type {} on port {} of device type {}".format(message_type.id,name,self.id))
        if message_type != self.parent.message_types[message_type.id]:
            raise GraphDescriptionError("Incorrect message type object {} on port {} of device type {}".format(message_type.id,name,self.id))
        p=OutputPort(self, name, self.parent.message_types[message_type.id], metadata, send_handler, source_file, source_line)
        self.outputs[name]=p
        self.outputs_by_index.append(p)
        self.ports[name]=p


class GraphType(object):
    def __init__(self,id,properties,metadata,shared_code):
        self.id=id
        if properties:
            assert isinstance(properties,TupleTypedDataSpec), "Expected TupleTypedDataSpec, got={}".format(properties)
        self.properties=properties
        self.metadata=metadata
        self.shared_code=shared_code
        self.device_types={}
        self.message_types={}

    def add_message_type(self,message_type):
        if message_type.id in self.message_types:
            raise GraphDescriptionError("message type already exists.")
        self.message_types[message_type.id]=message_type

    def add_device_type(self,device_type):
        if device_type.id in self.device_types:
            raise GraphDescriptionError("Device type already exists.")
        self.device_types[device_type.id]=device_type
        
class GraphTypeReference(object):
    def __init__(self,id,src):
        self.id=id
        self.src=src
        
        self.properties=properties
        self.metadata=metadata
        self.device_types={}
        self.message_types={}

    @property
    def native_dimension(self):
        raise RuntimeError("Cannot get dimension of unresolved GraphTypeReference.")

    @property
    def properties(self):
        raise RuntimeError("Cannot get properties of unresolved GraphTypeReference.")

    @property
    def device_types(self):
        raise RuntimeError("Cannot get device_types of unresolved GraphTypeReference.")

    @property
    def message_types(self):
        raise RuntimeError("Cannot get message_types of unresolved GraphTypeReference.")

    def add_message_type(self,message_type):
        raise RuntimeError("Cannot add to unresolved GraphTypeReference.")

    def add_device_type(self,device_type):
        raise RuntimeError("Cannot add to unresolved GraphTypeReference.")

        
class DeviceInstance(object):
    def __init__(self,parent,id,device_type,properties,metadata=None):
        if not is_refinement_compatible(device_type.properties,properties):
            raise GraphDescriptionError("Properties not compatible with device type properties: proto={}, value={}".format(device_type.properties, properties))

        self.parent=parent
        self.id=id
        self.device_type=device_type
        self.properties=properties
        self.metadata=metadata

        
class EdgeInstance(object):
    def __init__(self,parent,dst_device,dst_port_name,src_device,src_port_name,properties=None,metadata=None):
        self.parent=parent

        if dst_port_name not in dst_device.device_type.inputs:
            raise GraphDescriptionError("Port '{}' does not exist on dest device type '{}'".format(dst_port_name,dst_device.device_type.id))
        if src_port_name not in src_device.device_type.outputs:
            raise GraphDescriptionError("Port '{}' does not exist on src device type '{}'".format(src_port_name,src_device.device_type.id))

        dst_port=dst_device.device_type.inputs[dst_port_name]
        src_port=src_device.device_type.outputs[src_port_name]
        
        if dst_port.message_type != src_port.message_type:
            raise GraphDescriptionError("Dest port has type {}, source port type {}".format(dst_port.id,src_port.id))

        if not is_refinement_compatible(dst_port.properties,properties):
            raise GraphDescriptionError("Properties are not compatible: proto={}, value={}.".format(dst_port.properties, properties))

        self.id = dst_device.id+":"+dst_port_name+"-"+src_device.id+":"+src_port_name
        
        self.dst_device=dst_device
        self.src_device=src_device
        self.message_type=dst_port.message_type
        self.dst_port=dst_port
        self.src_port=src_port
        self.properties=properties
        self.metadata=metadata
        

class GraphInstance:
    def __init__(self,id,graph_type,properties=None,metadata=None):
        self.id=id
        self.graph_type=graph_type
        assert is_refinement_compatible(graph_type.properties,properties), "value {} is not a refinement of {}".format(properties,graph_type.properties)
        self.properties=properties
        self.metadata=metadata
        self.device_instances={}
        self.edge_instances={}
        self._validated=True

    def _validate_message_type(self,et):
        pass

    def _validate_device_type(self,dt):
        for p in dt.ports.values():
            if p.message_type.id not in self.graph_type.message_types:
                raise GraphDescriptionError("DeviceType uses an message type that is uknown.")
            if p.message_type != self.graph_type.message_types[p.message_type.id]:
                raise GraphDescriptionError("DeviceType uses an message type object that is not part of this graph.")

    def _validate_device_instance(self,di):
        if di.device_type.id not in self.graph_type.device_types:
            raise GraphDescriptionError("DeviceInstance refers to unknown device type.")
        if di.device_type != self.graph_type.device_types[di.device_type.id]:
            raise GraphDescriptionError("DeviceInstance refers to a device tye object that is not part of this graph.")

        if not is_refinement_compatible(di.device_type.properties,di.properties):
            raise GraphDescriptionError("DeviceInstance properties don't match device type.")
            

    def _validate_edge_instance(self,ei):
        pass

            
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
        
        for et in self.message_types:
            self._validate_message_type(et)
        for dt in self.device_types:
            self._validate_device_type(et)
        for di in self.device_instances:
            self._validate_device_instance(di)
        for ei in self.edge_instances:
            self._validate_edge_instances(ei)

        self._validated=True

