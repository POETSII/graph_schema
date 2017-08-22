import typing
from typing import Any, Dict, List

from contextlib import contextmanager

from mini_op2.core import *
from mini_op2.control_flow import Statement, ParFor
from mini_op2.system import SystemSpecification
from mini_op2.airfoil import build_system

from graph.core import *
from graph.save_xml import *

from lxml import etree

_unq_id_to_obj={} # type:Dict[Any,str]
_unq_obj_to_id={} # type:Dict[str,Any]


def make_unique_id(obj:Any,base:str) -> str:
    name=base
    while name in _unq_id_to_obj:
        name="{}_{}".format(name,len(_unq_ids))
    _unq_id_to_obj[name]=obj
    _unq_obj_to_id[obj]=name
    return name
    
def get_unique_id(obj:Any) -> str:
    return _unq_obj_to_id[obj]


_scalar_type_map={
    numpy.dtype("float64"):"double",
    numpy.dtype("uint32"):"uint32_t",
    numpy.dtype("uint16"):"uint16_t",
    numpy.dtype("uint8"):"uint8_t",
    numpy.double:"double",
    numpy.float:"float",
    numpy.uint32:"uint32_t",
    numpy.uint16:"uint16_t",
    numpy.uint8:"uint8_t"
}

def import_data_type(name:str,ot:DataType) -> TypedDataSpec:
    if ot.shape==():
        return ScalarTypedDataSpec(name,_scalar_type_map[ot.dtype])
    elif len(ot.shape)==1:
        return ArrayTypedDataSpec(name,ot.shape[0],ScalarTypedDataSpec("_", _scalar_type_map[ot.dtype]))
    elif len(ot.shape)==2 and ot.shape[1]==1:
        return ArrayTypedDataSpec(name,ot.shape[0],ScalarTypedDataSpec("_", _scalar_type_map[ot.dtype]))
    else:
        raise RuntimeError("data type not supported yet.")
        
def import_data_type_tuple(name:str, members:Sequence[DataType]):
    return TupleTypedDataSpec(name, members)


class DeviceTypeBuilder(object):
    def __init__(self, id:str):
        self.id=id
        self.properties={} # type:Dict[str,TypedDataSpec]
        self.state={} # type:Dict[str,TypedDataSpec]
        self.inputs={} # type:Dict[str,Tuple[str,str,str]]
        self.outputs={} # type:Dict[str,Tuple[str,str,str,str]]

    def add_property(self, name:str, type:DataType):
        assert name not in self.properties
        self.properties[name]=import_data_type(name,type)
        
    def add_state(self, name:str, type:DataType):
        assert name not in self.state
        self.state[name]=import_data_type(name,type)
        
    def add_input_pin(self, name:str, msgType:str, properties:None, state:None, body:str):
        assert name not in self.inputs
        assert properties is None
        assert state is None
        self.inputs[name]=(name, msgType, body)
    
    def extend_input_pin_handler(self, name:str, code:str):
        assert name in self.inputs
        (name,msgType,body)=self.inputs[name]
        self.inputs[name]=(name,msgType,body+code)
        
    def add_output_pin(self, name:str, msgType:str, rts:str, body:str):
        assert name not in self.outputs
        self.outputs[name]=(name, msgType, body, rts)
        
    def build(self, graph:GraphType) -> DeviceType:
        device_properties=import_data_type_tuple("_", self.properties.values())
        assert isinstance(device_properties,TypedDataSpec)
        device_state=import_data_type_tuple("_", self.state.values())
        assert isinstance(device_state,TypedDataSpec)
        d=DeviceType(graph, self.id, device_properties, device_state)
        for (name,msgType,handler) in self.inputs.values():
            message_type=graph.message_types[msgType]
            input_properties=None
            input_state=None
            d.add_input(name,message_type,input_properties,input_state,None,handler)
        for (name,msgType,handler,rts) in self.outputs.values():
            message_type=graph.message_types[msgType]
            output_properties=None
            output_state=None
            d.add_output(name,message_type,None,handler)
            d.ready_to_send_handler+=rts
        return d

class GraphTypeBuilder:
    _subst={} # type:Dict[str,str]

    @contextmanager
    def subst(self, **kwargs):
        old_vals={}
        for (k,v) in kwargs.items():
            if k in self._subst:
                old_vals[k]=self._subst[k]
            self._subst[k]=v
        
        yield
        
        for (k,v) in old_vals.items():
            self._subst[k]=v
            
    def s(self, x:str) -> str:
        if len(self._subst)==0:
            return x
        else:
            print(self._subst)
            return x.format(**self._subst)
    
    def __init__(self, id:str) -> None:
        self.id=id
        self.properties={} # type:Dict[str,TypedDataSpec]
        self.device_types={} # type:Dict[str,DeviceTypeBuilder]
        self.message_types={} # type:Dict[str,Tuple[str,TypedDataSpec]]
    
    
    def create_device_type(self, devType:str) -> str :
        devType=self.s(devType)
        assert str not in self.device_types
        self.device_types[devType]=DeviceTypeBuilder(devType)
        return devType
        
    def create_message_type(self, msgType:str, members:Dict[str,DataType]) -> str :
        msgType=self.s(msgType)
        assert msgType not in self.message_types
        members=[ import_data_type(self.s(n),t) for (n,t) in members.items() ]
        self.message_types[msgType]=(msgType, members)
        return msgType
        
    def add_device_property(self, devType:str, name:str, type:DataType) -> None :
        devType=self.device_types[ self.s(devType) ]
        devType.add_property(self.s(name), type)
        
    def add_graph_property(self, name:str, type:DataType) -> None :
        assert name not in self.properties
        self.properties[name]=import_data_type(name,type)

        
    def add_device_state(self, devType:str, name:str, type:DataType) -> None :
        devType=self.device_types[ self.s(devType) ]
        devType.add_state(self.s(name), type)
        
    def add_input_pin(self, devType:str, name:str, msgType:str, properties:None, state:None, body:str) -> None:
        devType=self.s(devType)
        msgType=self.s(msgType)
        name=self.s(name)
        body=self.s(body)
        assert devType in self.device_types
        assert msgType in self.message_types
        self.device_types[devType].add_input_pin(name, msgType, properties, state, body)
        
    def extend_input_pin_handler(self, devType:str, pinName:str, code:str) -> None:
        devType=self.s(devType)
        pinName=self.s(pinName)
        code=self.s(code)
        self.device_types[devType].extend_input_pin_handler(pinName, code)

    def add_output_pin(self, devType:str, name:str, msgType:str, rts:str, body:str) -> None:
        devType=self.s(devType)
        msgType=self.s(msgType)
        name=self.s(name)
        body=self.s(body)
        rts=self.s(rts)
        assert devType in self.device_types
        assert msgType in self.message_types
        self.device_types[devType].add_output_pin(name, msgType, rts, body)



    def build(self) -> GraphType:
        graph_properties=import_data_type_tuple("_", self.properties.values())
        
        graph=GraphType(self.id, graph_properties)
        
        for (name,type) in self.message_types.values():
            graph.add_message_type(MessageType(graph, name, import_data_type_tuple("_", type)))
            
        for db in self.device_types.values():
            graph.add_device_type(db.build(graph))
        
        return graph
    
    
""" Compilation strategy

- Every set is turned into a device type called "set_{id}"
  - Each dat is mapped to a state member of the relevant device called "dat_{id}"
  
- There is a single "global" device type that contains mutable globals
  - Each mutable global is mapped to a state member called "global_{id}"
  
- Constant globals are mapped to graph properties called "global_{id}"

A parallel invocation consists of:
 - Single message to global object
   - Broadcast message to indirect dats (both send and receive) identifying invocation
     - Each indirect input set (READ,RW) sends dat value to direct element on set
   - Broadcast message to direct set containing global values
     - direct set devices apply kernel
     - for each indirect (RW,INC) dat
       - send a message with new dat value
     - send a message back to globals including any global INC values
   - Each indirect (RW,INC) device waits for update
     - After update, sends back ack.
   - Global object waits for:
     - ack + global updates from direct set
     - ack from indirect write set
 - Single message from global object back, with new mutable globals
 
TODO, lots of corner cases:
- One dat appearing as both direct and indirect parameters (legal? With what access mode?)
- One device appearing as part of both read and write indirect set (on different and/or same dat)
 
"""

def compile_invocation(spec:SystemSpecification, builder:GraphTypeBuilder, stat:ParFor):
    invocation=get_unique_id(stat)
    global_reads=set() # type:List[(int,GlobalArgument)]
    global_writes=set() # type:List[(int,GlobalArgument)]
    indirect_reads=set() # type:List[(int,IndirectArgument)]
    indirect_writes=set() # type:List[(int,IndirectArgument)]
    direct_reads=set() # type:List[(int,DirectDatArgument)]
    direct_writes=set() # type:List[(int,DirectDatArgument)]
    for (i,arg) in enumerate(stat.arguments):
        if isinstance(arg,IndirectDatArgument):
            if arg.access_mode==AccessMode.READ:
                indirect_reads.add( (i,arg) )
            elif arg.access_mode==AccessMode.WRITE:
                indirect_writes.add( (i,arg) )
            elif arg.access_mode==AccessMode.RW:
                indirect_reads.add( (i,arg) )
                indirect_writes.add( (i,arg) )
            elif arg.access_mode==AccessMode.INC:
                indirect_writes.add( (i,arg) )
            else:
                raise RuntimeError("Unexpected access mode for indirect dat.")
        elif isinstance(arg,GlobalArgument):
            if arg.access_mode==AccessMode.READ:
                global_reads.add( (i,arg) )
            elif arg.access_mode==AccessMode.INC:
                global_writes.add( (i,arg) )
            else:
                raise RuntimeError("Unexpected access mode for global.")
        elif isinstance(arg,DirectDatArgument):
            if arg.access_mode==AccessMode.READ:
                direct_reads.add( (i,arg) )
            elif arg.access_mode==AccessMode.WRITE:
                direct_writes.add( (i,arg) )
            elif arg.access_mode==AccessMode.RW:
                direct_reads.add( (i,arg) )
                direct_writes.add( (i,arg) )
            elif arg.access_mode==AccessMode.INC:
                direct_writes.add( (i,arg) )
            else:
                raise RuntimeError("Unexpected access mode for direct dat.")
        else:
            raise RuntimeError("Unexpected argument type.")
    logging.info("Invocation %s : global reads = %s", invocation, global_reads)
    logging.info("Invocation %s : global writes = %s", invocation, global_writes)
    logging.info("Invocation %s : indirect reads = %s", invocation, indirect_reads)
    logging.info("Invocation %s : indirect writes = %s", invocation, indirect_writes)
    logging.info("Invocation %s : direct reads = %s", invocation, direct_reads)
    logging.info("Invocation %s : direct writes = %s", invocation, direct_writes)
    
    with builder.subst(base=invocation):
        directDevType="set_"+stat.iter_set.id
        
        mutableGlobals=dict()
        for (i,mg) in global_reads:
            mutableGlobals["global_"+mg.global_.id]=mg.data_type
        triggerMsgType=builder.create_message_type("{base}_trigger", mutableGlobals)

        
        ###########################################
        ## Deal with indirect reads.
        ## - Message broadcast from global to all indirect sets
        ## - Indirect sets then broadcast each dat involved
        
        builder.add_device_state(directDevType, "{base}_received", scalar_uint32)
        total_read_args_pending=0
        for (index,arg) in indirect_reads:
            indirectDevType="set_"+arg.to_set.id
            with builder.subst(index=index, dat_name=arg.dat.id):
                
                dataMsgType=builder.create_message_type("{base}_indirect_arg{index}", { "value":arg.data_type })
                
                # On the indirect device we need to flag whether the dat send is pending
                builder.add_device_state(directDevType, "{base}_send_pending_arg{index}", scalar_uint32)
                builder.add_input_pin(indirectDevType, "{base}_trigger_arg{index}", triggerMsgType, None, None,
                    # Receive handler
                    """
                    assert(deviceState->{base}_trigger_arg{index}==0);
                    deviceState->{base}_trigger_arg{index}=1;
                    """
                )
                builder.add_output_pin(indirectDevType, "{base}_send_arg{index}", dataMsgType,
                    # Ready to send
                    """
                    if(deviceState->{base}_trigger_arg{index}){{
                        *readyToSend |= RTS_FLAG_{base}_trigger_arg{index};
                    }}
                    """,
                    # Send handler
                    """
                    assert(deviceState->{base}_trigger_arg{index});
                    copy_value(message->value, deviceState->{dat_name});
                    deviceState->{base}_trigger_arg{index}=0;
                    """
                )

                
                # We need a landing pad for the dat in the state of the home set
                builder.add_device_state(directDevType, "{base}_in_arg{index}", arg.data_type)
                # And when the message arrives, just copy it in
                builder.add_input_pin(directDevType,"{base}_recv_arg{index}", dataMsgType, None, None,
                    """
                    copy_value(deviceState->{base}_arg{index}, message->value);
                    deviceState->{base}_received++;
                    """
                )
                
                total_read_args_pending+=1
        
        ############################################
        ## Deal with parallel iteration over set
        ## - Receive message from globals with any constants, and to indicate kernel should start
        ## - Also wait for any indirect reads
        
        builder.add_device_state(directDevType, "{base}_ready", scalar_uint32)
        builder.add_input_pin(directDevType,"{base}_trigger", triggerMsgType, None, None,
            """"
            assert(!*deviceState->{base}_ready);
            deviceState->{base}_received++;
            """
        )
        for k in mutableGlobals.keys():
            builder.extend_input_pin_handler(directDevType, "{base}_trigger",
                """
                copy_value(deviceState->{}, message->{});
                """.format(k,k)
            )
        
        with builder.subst(exec_thresh=total_read_args_pending):
            builder.add_output_pin(directDevType, "{base}_execute", "executeMsgType",
                """
                if(deviceState->{base}_received=={exec_thresh}){{
                  *readyToSend |= RTS_FLAG_{base}_execute;
                }}
                """,
                """
                    TODO
                """
            )
    
    
   
    
        
        

def sync_compiler(spec:SystemSpecification, code:Statement):
    builder=GraphTypeBuilder("op2_inst")
    
    builder.create_message_type("executeMsgType", {})
    
    builder.create_device_type("global")
    for global_ in spec.globals.values():
        if isinstance(global_,MutableGlobal):
            builder.add_device_state("global", "global_{}".format(global_.id), global_.data_type)
        elif isinstance(global_,ConstGlobal):
            builder.add_graph_property("global_{}".format(global_.id), global_.data_type)
        else:
            raise RuntimeError("Unexpected global type : {}", type(global_))
    
    for set in spec.sets.values():
        with builder.subst(set="set_"+set.id):
            builder.create_device_type("{set}")
            for dat in set.dats.values():
                builder.add_device_state("{set}", "dat_{}".format(dat.id), dat.data_type)
    
    kernels=[] # type:List[ParFor]
    for stat in code.all_statements():
        if isinstance(stat,ParFor):
            id=make_unique_id(stat, stat.name)
            print(id)
            kernels.append(stat)
    for stat in kernels:
        compile_invocation(spec,builder,stat)
    
    return builder
            

if __name__=="__main__":
    logging.basicConfig(level=4)
    
    (spec,inst,code)=build_system()
    builder=sync_compiler(spec,code)
    
    type=builder.build()
    
    nsmap = { None : "https://poets-project.org/schemas/virtual-graph-schema-v2", "p":"https://poets-project.org/schemas/virtual-graph-schema-v2" }
    def toNS(t):
        tt=t.replace("p:","{"+nsmap["p"]+"}")
        return tt
    
    root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)
    xml=save_graph_type(root, type)
    
    etree.ElementTree(root).write(sys.stdout.buffer, pretty_print=True, xml_declaration=True)
