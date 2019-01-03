import sys
import collections
class GraphDescriptionError(Exception):
    def __init__(self,msg):
        Exception.__init__(self,msg)

class TypedDataSpec(object):
    def __init__(self,name):
        self.name=name
        self.cTypeName=None

    def visit_subtypes(self,visitor):
        pass


# https://stackoverflow.com/a/48725499
class frozendict(dict):
    def __init__(self, *args, **kwargs):
        self._hash = None
        super(frozendict, self).__init__(*args, **kwargs)

    def __hash__(self):
        if self._hash is None:
            self._hash = hash(tuple(sorted(self.items())))  # iteritems() on py2
        return self._hash

    def _immutable(self, *args, **kws):
        raise TypeError('cannot change object - object is immutable')

    __setitem__ = _immutable
    __delitem__ = _immutable
    pop = _immutable
    popitem = _immutable
    clear = _immutable
    update = _immutable
    setdefault = _immutable

# Freezes a dictionary using the above frozen dict
# This is for the case of dictionaries containing dictionaries
def freezedict(d):
    newD = {}
    for i in d:
        if isinstance(i, dict):
            newD[i]=freezedict(i)
    frozendict(newD)

class ScalarTypedDataSpec(TypedDataSpec):

    _primitives=set(["int32_t","uint32_t","int16_t","uint16_t","int8_t","uint8_t","float","double"])

    def _check_value(self,value):
        assert not isinstance(self.type,Typedef)
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
        elif self.type=="double":
            res=float(value)
        else:
            assert False, "Unknown data type {}.".format(self.type)
        return res

    def __init__(self,name,type,default=None):
        TypedDataSpec.__init__(self,name)
        assert isinstance(type,Typedef) or (type in ScalarTypedDataSpec._primitives), "Didn't understand type parameter {}".format(type)
        self.type=type
        if isinstance(self.type,Typedef):
            self.default=self.type.expand(default)
        elif default is not None:
            self.default=self._check_value(default)
        else:
            self.default=0
        if isinstance(self.default,dict):
            #self.default=frozenset(self.default) # make it hashable
            self.default=freezedict(self.default) # frozendict(self.default)
        if isinstance(self.default,list):
            self.default=tuple(self.default) # make it hashable
        self._hash = hash(self.name) ^ hash(self.type) ^ hash(self.default)

    def visit_subtypes(self,visitor):
        if isinstance(self.type,Typedef):
            visitor(self.type)

    def is_refinement_compatible(self,inst):
        if inst is None:
            return True
        if isinstance(self.type,Typedef):
            return self.type.is_refinement_compatible(inst)

        try:
            self._check_value(inst)
        except:
            return False
        return True

    def create_default(self):
        if isinstance(self.type, Typedef):
            return self.type.create_default()
        else:
            return self.default

    def expand(self,inst):
        if isinstance(self.type,Typedef):
            return self.type.expand(inst)
        elif inst is None:
            return self.create_default()
        return self._check_value(inst)

    def contract(self,inst):
        if inst is not None:
            default=self.default or 0
            if inst!=default:
                return inst
        return None

    def __eq__(self, o):
        return isinstance(o, ScalarTypedDataSpec) and self.name==o.name and self.type==o.type and self.default==o.default

    def __hash__(self):
        return self._hash

    def __str__(self):
        if isinstance(self.type,Typedef):
            return "{}:{}={}".format(self.type,self.name,self.default)
        else:
            return "{}:{}={}".format(self.type,self.name,self.default)

class TupleTypedDataSpec(TypedDataSpec):
    def __init__(self,name,elements,default=None):
        TypedDataSpec.__init__(self,name)
        self._elts_by_name={}
        self._elts_by_index=[]
        self._hash = hash(self.name)
        self.default = default
        if self.default is not None:
            self.default = self.expand(self.default)
        for e in elements:
            if e.name in self._elts_by_name:
                raise GraphDescriptionError("Tuple element name appears twice.")
            self._elts_by_name[e.name]=e
            self._elts_by_index.append(e)
            self._hash = self._hash ^ hash(e)

    @property
    def elements_by_name(self):
        return self._elts_by_name

    @property
    def elements_by_index(self):
        return self._elts_by_index

    def __eq__(self, o):
        return isinstance(o, TupleTypedDataSpec) and self.name==o.name and self._elts_by_index==o._elts_by_index

    def __hash__(self):
        return self._hash

    def __str__(self):
        acc="Tuple:{}[\n".format(self.name)
        for i in range(len(self._elts_by_index)):
            if i!=0:
                acc=acc+";"
            acc=acc+str(self._elts_by_index[i])
            acc=acc+"\n"
        return acc+"]"

    def visit_subtypes(self,visitor):
        map(visitor, self._elts_by_index)

    def create_default(self):
        return { e.name:e.create_default() for e in self._elts_by_index  }

    def expand(self,inst):
        if inst is None:
            return self.create_default()
        assert isinstance(inst,dict), "Want to expand dict, got '{}'".format(inst)
        for e in self._elts_by_index:
            inst[e.name]=e.expand(inst.get(e.name,None))
        return inst

    def contract(self,inst):
        if inst is None:
            return inst
        res={}
        for (k,v) in inst:
            ks=self._elts_by_name[k]
            nv=ks.contract(v)
            if nv is not None:
                res[k]=nv
        return res

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
    def __init__(self,name,length,type,default=None):
        TypedDataSpec.__init__(self,name)
        self.type=type
        assert length>0, "Lengths must be greater than 0, as per PIP0007"
        self.length=length
        self.default = default
        if self.default is not None:
            self.default=self.expand(self.default)
        self._hash=hash(name) ^ hash(type) + length

    def __eq__(self, o):
        return isinstance(o, ArrayTypedDataSpec) and self.name==o.name and self.length==o.length and self.type==o.type

    def __hash__(self):
        return self._hash

    def __repr__(self):
        return "Array:{}[{}*{}]\n".format(self.name,self.type,self.length)

    def create_default(self):
        return [self.type.create_default() for i in range(self.length)]

    def visit_subtypes(self,visitor):
        visitor(self.type)

    def expand(self,inst):
        if inst is None:
            return self.create_default()
        elif isinstance(inst,list):
            # assert len(inst)==self.length
            assert len(inst) <= self.length
            # Now allowing only some of the array elements, but still ensuring that there isn't too many
            l = len(inst)
            # Filling in the blanks
            for i in range(self.length):
                if i < l:
                    # Fill the array from the front
                    inst[i]=self.type.expand(inst[i])
                else:
                    # Don't need to provide all array elements, so fill in the blanks
                    inst.append(self.type.create_default())
            return inst
        elif isinstance(inst,dict):
            # Can select particular index to default
            assert len(inst) <= self.length
            # Must ensure there is not too many
            newInst = []
            for i in range(0, self.length):
                if str(i) in inst:
                    # Fill this index in if specificed
                    newInst.append(self.type.expand(inst[str(i)]))
                else:
                    # Fill it with a blank if not specified
                    newInst.append(self.type.create_default())
            return newInst
        else:
            # Something has gone wrong with the default string, just fill it with blanks for now
            # TODO: Throw an error message here
            return self.create_default()

    def contract(self,inst):
        if inst is None:
            return inst
        assert isinstance(inst,list)
        assert len(inst)==self.length
        for x in inst:
            nv=self.type.contract(inst[i])
            if nv is not None:
                return inst
        return None


    def is_refinement_compatible(self,inst):
        if inst is None:
            return True;

        if isinstance(inst,list):
            if len(inst)>self.length:
                return False
            for v in inst:
                if not self.type.is_refinement_compatible(v):
                    return False
        elif isinstance(inst,dict):
            if len(inst)>self.length:
                return False
            for v in inst:
                if not self.type.is_refinement_compatible(inst[v]):
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


class Typedef(TypedDataSpec):
    def __init__(self,id,type,default=None):
        TypedDataSpec.__init__(self,id)
        self.id=id
        assert(isinstance(type,TypedDataSpec))
        self.type=type
        self.default=default
        if self.default is not None:
            self.default=self.expand(self.default)
        self._hash=hash(id) + 19937*hash(type)

    def is_refinement_compatible(self,inst):
        sys.stderr.write(" is_refinement_compatible( {} , {} )\n".format(self,inst))
        return self.type.is_refinement_compatible(inst)

    def create_default(self):
        return self.type.create_default()

    def expand(self,inst):
        return self.type.expand(inst)

    def contract(self,inst):
        return self.contract(inst)

    def __eq__(self, o):
        return isinstance(o, Typedef) and self.id==o.id

    def __hash__(self):
        return self._hash

    def __str__(self):
        return self.id
        # return "{}[{}]".format(self.id,self.type) If we are to allow typedefs of typedefs, this cannot be used

class MessageType(object):
    def __init__(self,parent,id,message,metadata=None,cTypeName=None,numid=0):
        assert (message is None) or isinstance(message,TypedDataSpec)
        self.id=id
        self.parent=parent
        self.message=message
        self.metadata=metadata
        self.numid=numid
        size = self._checkMessageSize(self.message)
        if size > 44:
            raise RuntimeError("ERROR: Message \"" + self.id + "\" has a payload that's too large.\n"
                             + "A message header is 20 bytes, and this payload is " + str(size) + " bytes.\n"
                             + "The total is therefore " + str(20+size) + " bytes, and must be a maximum of 64")

    def _checkMessageSize(self, payload):
        # https://github.com/POETSII/toy_softswitch/blob/873a9f4583179d6d1842c6fc3ef1068c7294f32e/include/softswitch.hpp#L29
        # The header of a message is 20 bytes, the total message size is 64.
        # This function serves to check that the total message size, header+payload, is no more than 64 bytes.
        # Therefore the payload must be less than or equal to 44 bytes
        if isinstance(payload, ScalarTypedDataSpec):
            if isinstance(payload.type, Typedef):
                return self._checkMessageSize(payload.type)
            elif payload.type == "uint8_t" or payload.type == "int8_t":
                return 1
            elif payload.type == "uint16_t" or payload.type == "int16_t":
                return 2
            elif payload.type == "uint32_t" or payload.type == "int32_t":
                return 4
            elif payload.type == "float" or payload.type == "double":
                return 4
            elif payload.type== "uint64_t" or payload.type == "int64_t":
                return 8
            else:
                raise RuntimeError("Unrecognised scalar type in message")
        elif isinstance(payload, Typedef):
            return self._checkMessageSize(payload.type)
        elif isinstance(payload, TupleTypedDataSpec):
            value = 0
            for elt in payload.elements_by_index:
                value = value + self._checkMessageSize(elt)
            return value
        elif isinstance(payload, ArrayTypedDataSpec):
            return (self._checkMessageSize(payload.type) * payload.length)
        elif payload is None:
            return 0
        else:
            raise RuntimeError("Unrecognised type in message {}".format(payload))
        return 0

class Pin(object):
    def __init__(self,parent,name,message_type,is_application,metadata,source_file,source_line):
        self.parent=parent
        self.name=name
        self.message_type=message_type
        self.is_application=is_application
        self.metadata=metadata
        self.source_file=source_file
        self.source_line=source_line


class InputPin(Pin):
    def __init__(self,parent,name,message_type,is_application,properties,state,metadata,receive_handler,source_file=None,source_line=None):
        Pin.__init__(self,parent,name,message_type,is_application,metadata,source_file,source_line)
        self.properties=properties
        self.state=state
        self.receive_handler=receive_handler

class OutputPin(Pin):
    def __init__(self,parent,name,message_type,is_application,metadata,send_handler,source_file,source_line):
        Pin.__init__(self,parent,name,message_type,is_application,metadata,source_file,source_line)
        self.send_handler=send_handler


class DeviceType(object):
    def __init__(self,parent,id,properties,state,metadata=None,shared_code=[], isExternal=False):
        assert (state is None) or isinstance(state,TypedDataSpec)
        assert (properties is None) or isinstance(properties,TypedDataSpec)
        self.id=id
        self.parent=parent
        self.state=state
        self.properties=properties
        self.inputs={}
        self.inputs_by_index=[]
        self.outputs={}
        self.outputs_by_index=[]
        self.pins={}
        self.metadata=metadata
        self.shared_code=shared_code
        self.ready_to_send_handler="" # Hrmm, this was missing as an explicit member, but is used by load/save
        self.init_handler=""
        self.isExternal=isExternal

    def add_input(self,name,message_type,is_application,properties,state,metadata,receive_handler,source_file=None,source_line={}):
        assert (state is None) or isinstance(state,TypedDataSpec)
        assert (properties is None) or isinstance(properties,TypedDataSpec)
        if name in self.pins:
            raise GraphDescriptionError("Duplicate pin {} on device type {}".format(name,self.id))
        if message_type.id not in self.parent.message_types:
            raise GraphDescriptionError("Unregistered message type {} on pin {} of device type {}".format(message_type.id,name,self.id))
        if message_type != self.parent.message_types[message_type.id]:
            raise GraphDescriptionError("Incorrect message type object {} on pin {} of device type {}".format(message_type.id,name,self.id))
        p=InputPin(self, name, self.parent.message_types[message_type.id], is_application, properties, state, metadata, receive_handler, source_file, source_line)
        self.inputs[name]=p
        self.inputs_by_index.append(p)
        self.pins[name]=p

    def add_output(self,name,message_type,is_application, metadata,send_handler,source_file=None,source_line=None):
        if name in self.pins:
            raise GraphDescriptionError("Duplicate pin {} on device type {}".format(name,self.id))
        if message_type.id not in self.parent.message_types:
            raise GraphDescriptionError("Unregistered message type {} on pin {} of device type {}".format(message_type.id,name,self.id))
        if message_type != self.parent.message_types[message_type.id]:
            raise GraphDescriptionError("Incorrect message type object {} on pin {} of device type {}".format(message_type.id,name,self.id))
        p=OutputPin(self, name, self.parent.message_types[message_type.id], is_application, metadata, send_handler, source_file, source_line)
        self.outputs[name]=p
        self.outputs_by_index.append(p)
        self.pins[name]=p


class GraphType(object):
    def __init__(self,id,properties,metadata=None,shared_code=[]):
        self.id=id
        if properties:
            assert isinstance(properties,TupleTypedDataSpec), "Expected TupleTypedDataSpec, got={}".format(properties)
        self.properties=properties
        self.metadata=metadata
        self.shared_code=shared_code
        self.device_types=collections.OrderedDict()
        self.typedefs_by_index=[]
        self.typedefs={}
        self.message_types=collections.OrderedDict()

    def _validate_type(self,type):
        """Walk through the types and check that any typedefs have already been added to the graphtype."""
        if isinstance(type,Typedef):
            if (type.id not in self.typedefs):
                raise GraphDescriptionError("Typedef {} not added to graph type. Knowns are {}".format(type.id,self.typedefs.values()))
            if (self.typedefs[type.id]!=type):
                raise GraphDescriptionError("Typedef has wrong identity.")
            # If it's in the graph, we don't need to validate it's contained type again
        else:
            type.visit_subtypes( lambda t: self._validate_type(t) )


    def add_typedef(self,typedef):
        assert(isinstance(typedef,Typedef))
        if typedef.id in self.typedefs:
            raise GraphDescriptionError("typedef already exists.")
        self._validate_type(typedef.type)
        self.typedefs_by_index.append(typedef)
        self.typedefs[typedef.id]=typedef


    def add_message_type(self,message_type):
        if message_type.id in self.message_types:
            raise GraphDescriptionError("message type already exists.")
        self.message_types[message_type.id]=message_type

    def add_device_type(self,device_type):
        if device_type.id in self.device_types:
            raise GraphDescriptionError("Device type already exists.")
        self.device_types[device_type.id]=device_type

    def add_external_type(self, external_type):
        if external_type.id in self.external_types:
            raise GraphDescriptionError("External device type already exists.")
        self.external_types[external_type.id]=external_type

class GraphTypeReference(object):
    def __init__(self,id,src=None):
        self.id=id
        self.src=src

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
    def __init__(self,parent,id,device_type,properties=None,state=None,metadata=None):
        if __debug__ and (properties is not None and not is_refinement_compatible(device_type.properties,properties)):
            raise GraphDescriptionError("Properties not compatible with device type properties: proto={}, value={}".format(device_type.properties, properties))

        self.parent=parent
        self.id=id
        self.device_type=device_type
        self.properties=properties
        self.state=state
        self.metadata=metadata

    def set_property(self, name, value):
        if self.properties==None:
            self.properties={}
        self.properties[name]=value
        if __debug__ and not is_refinement_compatible(self.device_type.properties,self.properties):
            raise GraphDescriptionError("Setting property {} on {} results in properties incompatible with device type properties: proto={}, value={}".format(name, self.id, self.device_type.properties, self.properties))



class EdgeInstance(object):

    def __init__(self,parent,dst_device,dst_pin,src_device,src_pin,properties=None,metadata=None):
        # type : (GraphInstance, DeviceInstance, Union[str,InputPin], DeviceInstance, Union[str,OutputPin], Optional[Dict], Optional[Dict] ) -> None
        self.parent=parent

        if isinstance(dst_pin,str):
            if __debug__ and (dst_pin not in dst_device.device_type.inputs):
                raise GraphDescriptionError("Pin '{}' does not exist on dest device type '{}'. Candidates are {}".format(dst_pin,dst_device.device_type.id,",".join(dst_device.device_type.inputs)))
            dst_pin=dst_device.device_type.inputs[dst_pin]
        else:
            assert dst_device.device_type.inputs[dst_pin.name]==dst_pin

        if isinstance(src_pin,str):
            if __debug__ and (src_pin not in src_device.device_type.outputs):
                raise GraphDescriptionError("Pin '{}' does not exist on src device type '{}'. Candidates are {}".format(src_pin,src_device.device_type.id,",".join(dst_device.device_type.outputs)))
            src_pin=src_device.device_type.outputs[src_pin]
        else:
            assert src_device.device_type.outputs[src_pin.name]==src_pin

        if __debug__ and (dst_pin.message_type != src_pin.message_type):
            raise GraphDescriptionError("Dest pin has type {}, source pin type {}".format(dst_pin.message_type.id,src_pin.message_type.id))

        if __debug__ and (not is_refinement_compatible(dst_pin.properties,properties)):
            raise GraphDescriptionError("Properties are not compatible: proto={}, value={}.".format(dst_pin.properties, properties))

        if __debug__ and dst_pin.is_application:
            raise GraphDescriptionError("Attempt to connect edge instance to application input pin")
        if __debug__ and src_pin.is_application:
            raise GraphDescriptionError("Attempt to connect edge instance to application output pin")

        self.id = "{}:{}-{}:{}".format(dst_device.id,dst_pin.name,src_device.id,src_pin.name)

        self.dst_device=dst_device
        self.src_device=src_device
        self.message_type=dst_pin.message_type
        self.dst_pin=dst_pin
        self.src_pin=src_pin
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
        for p in dt.pins.values():
            if p.message_type.id not in self.graph_type.message_types:
                raise GraphDescriptionError("DeviceType uses a message type that is uknown.")
            if p.message_type != self.graph_type.message_types[p.message_type.id]:
                raise GraphDescriptionError("DeviceType uses a message type object that is not part of this graph.")

    def _validate_device_instance(self,di):
        if not di.device_type.isExternal:
            if di.device_type.id not in self.graph_type.device_types:
                raise GraphDescriptionError("DeviceInstance refers to unknown device type.")
            if di.device_type != self.graph_type.device_types[di.device_type.id]:
                raise GraphDescriptionError("DeviceInstance refers to a device type object that is not part of this graph.")
        else:
            if di.device_type.id not in self.graph_type.device_types:
                raise GraphDescriptionError("DeviceInstance refers to unknown external device type.")
            if di.device_type != self.graph_type.device_types[di.device_type.id]:
                raise GraphDescriptionError("DeviceInstance refers to an external device type object that is not part of this graph.")

        if not is_refinement_compatible(di.device_type.properties,di.properties):
            raise GraphDescriptionError("DeviceInstance properties don't match device type.")

    def _validate_edge_instance(self,ei):
        pass


    def set_property(self, name, value):
        if self.properties==None:
            self.properties={}
        self.properties[name]=value
        if __debug__ and not is_refinement_compatible(self.graph_type.properties,self.properties):
            raise GraphDescriptionError("Setting property {} on {} results in properties incompatible with device type properties: proto={}, value={}".format(name, self.id, self.graph_type.properties, self.properties))

    def add_device_instance(self,di,validate=False):
        if __debug__ and (di.id in self.device_instances):
            raise GraphDescriptionError("Duplicate deviceInstance id {}".format(id))

        if __debug__ or validate:
            self._validate_device_instance(di)
        else:
            self._validated=False

        self.device_instances[di.id]=di

        return di

    def create_device_instance(self, id,device_type,properties=None,metadata=None):
        if isinstance(device_type,str):
            assert isinstance(self.graph_type,GraphType)
            if device_type not in self.graph_type.device_types:
                raise RuntimeError("No such device type called '{}'".format(device_type))
            device_type=self.graph_type.device_types[device_type]
        assert isinstance(device_type,DeviceType)
        di=DeviceInstance(self,id,device_type,properties,metadata)
        return self.add_device_instance(di)


    def add_edge_instance(self,ei,validate=False):
        if __debug__ and (ei.id in self.edge_instances):
            raise GraphDescriptionError("Duplicate edgeInstance id {}".format(ei.id))

        if __debug__ or validate:
            self._validate_edge_instance(ei)
        else:
            self._validated=False

        self.edge_instances[ei.id]=ei

        return ei

    def create_edge_instance(self,dst_device,dst_pin_name,src_device,src_pin_name,properties=None,metadata=None):
        ei=EdgeInstance(self,dst_device,dst_pin_name,src_device,src_pin_name,properties,metadata)
        return self.add_edge_instance(ei)


    #~ def add_reduction(self,dstDevInst,dstPinName,reducerFactory,maxFanIn,srcInstances,srcPinName):
        #~ """Adds a reduction via a tree of reduction nodes.

            #~ dstDevInst : the (already constructed) destination node
            #~ dstPinName : output pin name for the destination _and_ the input pin for reducers
            #~ reducerFactor : a functor from (name,faninCount) -> deviceInstance
            #~ maxFanIn : largest acceptable fanin at any level of tree
            #~ srcInstances: Array of source instances
            #~ srcPinName : output pin name for the sources _and_ the output pin for reducers

        #~ """

        #~ assert(len(srcInstances)>0)

        #~ while(len(srcInstances)>maxFanIn):
            #~ currLevel=srcInstances
            #~ nextLevel=[]



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

