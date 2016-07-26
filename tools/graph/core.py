class GraphDescriptionError(Exception):
    def __init__(self,msg):
        Exception.__init__(self,msg)

FNV64_PRIME=1099511628211;
FNV64_OFFSET=14695981039346656037;

def fnv64_hash_byte(hash,data):
    assert(isinstance(data,int) and 0<=data<256)
    assert(isinstance(hash,int) and 0<=hash<2**64)
    return ((hash^data)*FNV64_PRIME) % 2**64

def fnv64_hash_uint64(hash, data):
    assert(isinstance(data,int) and 0<=data<2**64)
    assert(isinstance(hash,int) and 0<=hash<2**64)
    for i in range(0,8):
        v=data&0xFF
        hash((hash^v)*FNV64_PRIME) % 2**64
        data=data>>8
    return data


def fnv64_hash_int(hash, data):
    assert(isinstance(data,int))
    assert(isinstance(hash,int))
    print(hash);
    assert(0<=hash<2**64)

    if data<0:
        return fnv64_hash_int(fnv64_hash_byte(hash, 0xCC), -(data+1))

    while data:
        hash=fnv64_hash_byte(hash, data & 0xFF)
        data=data >> 8

    return data

def fnv64_hash_combine(*hashes):
    res=hashes[0]
    for i in range(1,len(hashes)):
        res=fnv64_hash_uint64(res, hashes[i])
    return res
    

def fnv64_hash_string(hash, data):
    assert(isinstance(data,str))
    assert(isinstance(hash,int) and 0<=hash<2**64)

    for c in data:
        co=ord(c)
        assert(0<co<256)
        hash=((hash^data)*FNV64_PRIME) % 2**64

    return hash

class TypedData(object):
    def __init__(self,name):
        self.name=name

    def calc_hash(self):
        raise NotImplementedError()


class ScalarData(TypedData):
    def __init__(self,name,value):
        TypedData.__init__(self,name)
        if value is not None:
            self.value=self._check_value(value)
        else:
            self.value=None
        self.is_complete=value is not None
        self._print_name=None

    def _check_value(self,value):
        raise NotImplementedError()

    def is_refinement_compatible(self,inst):
        if inst is None:
            return True
        if not isinstance(inst,type(self)):
            return False # We are strict, it must be exactly the same type
        if inst.name!=self.name:
            return False
        return True

    def __str__(self):
        if self.value:
            return "{}:{}={},complete={}".format(self._print_name,self.name,self.value,self.is_complete)
        else:
            return "{}:{}".format(self._print_name,self.name)

    
class Int32Data(ScalarData):
    def __init__(self,name,value):
        ScalarData.__init__(self,name,value)
        self._print_name="Int32"
        
    def _check_value(self,value):
        return int(value)

    def calc_hash(self):
        return fnv64_hash_combine(fnv64_hash_string("Int32"),fnv64_hash_string(name))


class Float32Data(ScalarData):
    def __init__(self,name,value):
        ScalarData.__init__(self,name,value)
        self._print_name="Float32"
        
    def _check_value(self,value):
        return float(value)

    def calc_hash(self):
        return fnv64_hash_combine(fnv64_hash_string("Float32"),fnv64_hash_string(name))


class BoolData(ScalarData):
    def __init__(self,name,value):
        ScalarData.__init__(self,name,value)
        self._print_name="Bool"
        
    def _check_value(self,value):
        if isinstance(value,bool):
            return value
        if isinstance(value,str):
            if value in ['True','true','Yes','yes','1']:
                return True
            if value in ['False','false','No','no','0']:
                return False
            raise GraphDescriptionError("Couldn't convert '{}' to a boolean".format(value))
        if isinstance(value,int):
            return value!=0
        raise GraphDescriptionError("Don't know how to convert value to boolean.")

    def calc_hash(self):
        return fnv64_hash_combine(fnv64_hash_string("Bool"),fnv64_hash_string(name))


class TupleData(TypedData):
    def __init__(self,name,elements):
        TypedData.__init__(self,name)
        self._elts_by_name={}
        self._elts_by_index=[]
        self.is_complete=True
        for e in elements:
            if e.name in self._elts_by_name:
                raise GraphDescriptionError("Tuple element name appears twice.")
            self._elts_by_name[e.name]=e
            self._elts_by_index.append(e)
            self.is_complete=self.is_complete and e.is_complete

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

    def calc_hash(self):
        hash=fnv64_hash_combine(fnv64_hash_string("Tuple"),fnv64_hash_string(name))
        for e in self.elements_by_index:
            hash=fnv64_hash_combine(hash, e.calc_hash())
        return hash

    def is_refinement_compatible(self,inst):
        if not isinstance(inst,TupleData):
            return False

        for ee in self._elts_by_index:
            if ee.name in inst._elts_by_name:
                if not ee.is_refinement_compatible(inst._elts_by_name[ee.name]):
                    return False

        # Check that everything in the instance is known
        for ee in inst._elts_by_index:
            if ee.name not in self._elts_by_name:
                return False
        return True

def is_refinement_compatible(proto,inst):
    if proto is None:
        return inst is None
    if inst is None:
        return True
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
    def __init__(self,id,properties):
        self.id=id
        self.properties=properties
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
        
class DeviceInstance(object):
    def __init__(self,parent,id,device_type,properties):
        if not is_refinement_compatible(device_type.properties,properties):
            raise GraphDescriptionError("Properties not compatible with device type properties: proto={}, value={}".format(device_type.properties, properties))

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
        assert(is_refinement_compatible(graph_type.properties,properties))
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

