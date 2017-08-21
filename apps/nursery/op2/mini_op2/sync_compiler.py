import typing
from typing import Any, Dict, List

from contextlib import contextmanager

from mini_op2.core import *
from mini_op2.control_flow import Statement, ParFor
from mini_op2.system import SystemSpecification
from mini_op2.airfoil import build_system

from graph.core import *

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
    numpy.double:"double",
    numpy.float:"float",
    numpy.uint32:"uint32_t",
}

def import_data_type(name:str,ot:DataType) -> TypedDataSpec:
    if ot.shape==():
        return ScalarTypedDataSpec(name,_scalar_type_map[ot.dtype])
    elif len(ot.shape)==1:
        return ArrayTypedDataSpec(name,ot.shape[0],ScalarTypeDataSpec("_", _scalar_type_map[ot.dtype]))
    elif len(ot.shape)==2 and ot.shape[1]==1
        return ArrayTypedDataSpec(name,ot.shape[0],ScalarTypeDataSpec("_", _scalar_type_map[ot.dtype]))
    else:
        raise RuntimeError("data type not supported yet.")


class DeviceTypeBuilder:
    _subst=Dict[str,str]

    @contextmanager
    def subst(self, **kwargs):
        old_vals={}
        for (k,v) in kwargs:
            if k in self._subst:
                old_val[k]=self._subst[k]
            self._subst[k]=vv
        
        yield
        
        for (k,v) in old_vals:
            self._subst[k]=v
            
    def s(self, x:str) -> str:
        return x.format(**self._subst)
    
    class DeviceTypeBuilder:
        def __init__(self, id:str):
            self.id=id
            self.properties={} # type:Dict[str,TypedDataSpec]
            self.state={} # type:Dict[str,TypedDataSpec]
            self.rts=[] # type:List[str]
            self.inputs={} # type:Dict[str,InputPin]
            self.outputs={} # type:Dict[str,OutputPin]
    
        def add_property(self, name:str, type:DataType):
            assert name not in self.properties
            self.properties[name]=import_data_type(name,type)
            
        def add_state(self, name:str, type:DataType):
            assert name not in self.state
            self.state[name]=import_data_type(name,type)
    
    def __init__(self, id:str) -> None:
        self.id=id
        self.properties={} # type:Dict[str,TypedDataSpec]
        self.device_types={} # type:Dict[str,DeviceTypeBuilder]
        self.message_types={} # type:Dict[str,MessageType]
    
    
    def create_device_type(devType:str) -> None :
        devType=self.s(devType)
        assert str not in self.device_types
        self.device_types[devType]=DeviceTypeBuilder(devType)
        
    def create_message_type(devType:str, members:Dict[str,DataType]) -> None :
        devType=self.s(devType)
        assert devType not in self.message_types
        members=[ import_data_type(self.s(n),t) for (n,t) in members.items() ]
        self.message_types[devType]=MessageType(devType, members)
        
    def add_device_property(devType:str, name:str, type:DataType) -> None :
        devType=self.device_types[ self.s(str) ]
        devType.add_property(self.s(name), type)
        
    def add_device_state(devType:str, name:str, type:DataType) -> None :
        devType=self.device_types[ self.s(str) ]
        devType.add_state(self.s(name), type)
        
    def add_input_port(devType:str, name:str, body:str) -> None:
        raise NotImplementedError

    def add_output_port(devType:str, name:str, body:str) -> None:
        raise NotImplementedError


    
    
    
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

def compile_invocation(spec:SystemSpecification, stat:ParFor):
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
        elif isinstance(arg,MutableGlobalArgument):
            if arg.access_mode==AccessMode.READ:
                global_reads.add( (i,arg) )
            elif arg.access_mode==AccessMode.INC:
                global_writes.add( (i,arg) )
            else:
                raise RuntimeError("Unexpected access mode for global.")
        elif isinstance(arg,ConstGlobalArgument):
            pass # No action needed
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
    
    with builder.subst(base=base):
        directDevType=arg.iter_set.id
        
        ###########################################
        ## Deal with indirect reads.
        ## - Message broadcast from global to all indirect sets
        ## - Indirect sets then broadcast each dat involved
        
        add_buffer(directDevType, "{}_args_received", scalar_uint32)
        total_read_args_pending=0
        for (index,arg) in indirect_reads:
            arg_subst={"index":index, "dat_name":arg.dat.id, **subst}
            
            dataMsgType=create_message_type("{base}_indirect_arg{index}", { "value":arg.data_type }, arg_subst)
            
            # On the indirect device we need to flag whether the dat send is pending
            add_state(directDevType, "{base}_send_pending_arg{index}", scalar_uint32, arg_subst)
            add_input_pin(indirectDevType, "{base}_trigger_arg{index}", triggerMsgType, None,
                # Receive handler
                """
                assert(deviceState->{base}_trigger_arg{index}==0);
                deviceState->{base}_trigger_arg{index}=1;
                """,
                arg_subst
            )
            add_output_pin(indirectDevType, "{base}_trigger_arg{index}", dataMsgType,
                # Ready to send
                """
                if(deviceState->{base}_trigger_arg{index}){{
                    *readyToSend |= RTS_FLAG_{base}_trigger_arg{index};
                }}
                """
                # Send handler
                """
                assert(deviceState->{base}_trigger_arg{index});
                copy_value(message->value, deviceState->{dat_name});
                deviceState->{base}_trigger_arg{index}=0;
                """,
                arg_subst
            )

            
            # We need a landing pad for the dat in the state of the home set
            add_state(directDevType, "{base}_in_arg{index}", arg.data_type, arg_subst)
            # And when the message arrives, just copy it in
            add_input_pin(directDevType,"{base}_recv_arg{index}", dataMsgType,
                """
                copy_value(deviceState->{base}_arg{}, message->value);
                deviceState->{base}_arg{index}_received++;
                """,
                arg_subst
            )
            
            total_read_args_pending+=1
        
        ############################################
        ## Deal with parallel iteration over set
        ## - Receive message from globals with any constants, and to indicate kernel should start
        ## - Also wait for any indirect reads
        
        mutableGlobals=dict()
        for mg in global_reads:
            mutableGlobals["global_"+mg.id]=mg.data_type
        startMsgType=create_message_type("{base}_start", mutableGlobals, subst)
        
        add_input_pin(directDevType,"{base}_
    
    
   
    
        
        

def sync_compiler(spec:SystemSpecification, code:Statement):
    builder=GraphBuilder()
    
    builder.create_device_type("global")
    for global_ in spec.globals.values():
        if isinstance(global_,MutableGlobal):
            builder.add_device_state("global", "global_{}".format(global_.id), global_.data_type)
        elif isinstance(global_,ConstGlobal):
            builder.add_graph_property("global_{}".format(global_,id), global_.data_type)
        else:
            raise RuntimeError("Unexpected global type.")
    
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
        compile_invocation(spec,stat)
        
            

if __name__=="__main__":
    logging.basicConfig(level=4)
    
    (spec,inst,code)=build_system()
    sync_compiler(inst,code)

