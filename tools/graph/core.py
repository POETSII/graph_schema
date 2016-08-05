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
        elif self.type=="uint32_t":
            res=int(value)
            assert(0 <= res < 2**32)
        elif self.type=="float":
            res=float(value)
        elif self.type=="bool":
            res=bool(value)
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



def is_refinement_compatible(proto,inst):
    if proto is None:
        return inst is None
    if inst is None:
        return True;
    return proto.is_refinement_compatible(inst)
    

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
        self.inputs_by_index=[]
        self.outputs={}
        self.outputs_by_index=[]
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
        self.inputs_by_index.append(p)
        self.ports[name]=p

    def add_output(self,name,edge_type,send_handler):
        if name in self.ports:
            raise GraphDescriptionError("Duplicate port {} on device type {}".format(name,self.id))
        if edge_type.id not in self.parent.edge_types:
            raise GraphDescriptionError("Unregistered edge type {} on port {} of device type {}".format(edge_type.id,name,self.id))
        if edge_type != self.parent.edge_types[edge_type.id]:
            raise GraphDescriptionError("Incorrect edge type object {} on port {} of device type {}".format(edge_type.id,name,self.id))
        p=OutputPort(self, name, self.parent.edge_types[edge_type.id], send_handler)
        self.outputs[name]=p
        self.outputs_by_index.append(p)
        self.ports[name]=p


class GraphType(object):
    def __init__(self,id,native_dimension,properties,shared_code):
        self.id=id
        self.native_dimension=native_dimension
        if properties:
            assert isinstance(properties,TupleTypedDataSpec), "Expected TupleTypedDataSpec, got={}".format(properties)
        self.properties=properties
        self.shared_code=shared_code
        self.device_types={}
        self.edge_types={}

    def add_edge_type(self,edge_type):
        if edge_type.id in self.edge_types:
            raise GraphDescriptionError("Edge type already exists.")
        self.edge_types[edge_type.id]=edge_type

    def add_device_type(self,device_type):
        if device_type.id in self.device_types:
            raise GraphDescriptionError("Device type already exists.")
        self.device_types[device_type.id]=device_type
        
class GraphTypeReference(object):
    def __init__(self,id,src):
        self.id=id
        self.src=src
        
        self.native_dimension=native_dimension
        self.properties=properties
        self.device_types={}
        self.edge_types={}

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
    def edge_types(self):
        raise RuntimeError("Cannot get edge_types of unresolved GraphTypeReference.")

    def add_edge_type(self,edge_type):
        raise RuntimeError("Cannot add to unresolved GraphTypeReference.")

    def add_device_type(self,device_type):
        raise RuntimeError("Cannot add to unresolved GraphTypeReference.")

        
class DeviceInstance(object):
    def __init__(self,parent,id,device_type,native_location,properties):
        if not is_refinement_compatible(device_type.properties,properties):
            raise GraphDescriptionError("Properties not compatible with device type properties: proto={}, value={}".format(device_type.properties, properties))

        self.parent=parent
        self.id=id
        self.device_type=device_type
        self.native_location=native_location
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
            raise GraphDescriptionError("Properties are not compatible: proto={}, value={}.".format(dst_port.edge_type.properties, properties))

        # We create a local id to ensure uniqueness of edges, but this is not persisted
        self.id = (dst_device.id,dst_port_name,src_device.id,src_port_name)
        
        self.dst_device=dst_device
        self.src_device=src_device
        self.edge_type=dst_port.edge_type
        self.dst_port=dst_port
        self.src_port=src_port
        self.properties=properties
        

class GraphInstance:
    def __init__(self,id,graph_type,properties=None):
        self.id=id
        self.graph_type=graph_type
        assert is_refinement_compatible(graph_type.properties,properties), "value {} is not a refinement of {}".format(properties,graph_type.properties)
        self.properties=properties
        self.device_instances={}
        self.edge_instances={}
        self._validated=True

    def _validate_edge_type(self,et):
        pass

    def _validate_device_type(self,dt):
        for p in dt.ports.values():
            if p.edge_type.id not in self.graph_type.edge_types:
                raise GraphDescriptionError("DeviceType uses an edge type that is uknown.")
            if p.edge_type != self.graph_type.edge_types[p.edge_type.id]:
                raise GraphDescriptionError("DeviceType uses an edge type object that is not part of this graph.")

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
        
        for et in self.edge_types:
            self._validate_edge_type(et)
        for dt in self.device_types:
            self._validate_device_type(et)
        for di in self.device_instances:
            self._validate_device_instance(di)
        for ei in self.edge_instances:
            self._validate_edge_instances(ei)

        self._validated=True

