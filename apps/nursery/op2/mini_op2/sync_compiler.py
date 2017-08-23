import typing
from typing import Any, Dict, List

import sys

from contextlib import contextmanager

from mini_op2.core import *
from mini_op2.control_flow import Statement, ParFor
from mini_op2.system import SystemSpecification
from mini_op2.airfoil import build_system
from mini_op2.builder import GraphTypeBuilder


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

    
    



class InvocationContext(object):
    def get_indirect_reads(self) -> Dict[ Set, List[ Tuple[int,Args] ] ]:
        """Find all the sets that are involve as indirect reads, and all the arguments (i.e. dats) that
        then are read from that set."""
        res={}
        for (i,arg) in self.indirect_reads:
            res.setdefault(arg.iter_set,[]).append( (i,arg) )
        return res
    
    def get_indirect_writes(self) -> Dict[ Set, List[ Tuple[int,Args] ] ]:
        """Find all the sets that are involve as indirect writes, and all the arguments (i.e. dats) that
        then are read from that set."""
        res={}
        for (i,arg) in self.indirect_reads:
            res.setdefault(arg.iter_set,[]).append( (i,arg) )
        return res
    
    def __init__(self, spec:SystemSpecification, stat:ParFor) -> None:
        super().__init__()
        self.spec=spec
        self.stat=stat
        
        self.invocation=get_unique_id(stat)
        self.const_global_reads=set() # type:List[(int,GlobalArgument)]
        self.mutable_global_reads=set() # type:List[(int,GlobalArgument)]
        self.global_writes=set() # type:List[(int,GlobalArgument)]
        self.indirect_reads=set() # type:List[(int,IndirectArgument)]
        self.indirect_writes=set() # type:List[(int,IndirectArgument)]
        self.direct_reads=set() # type:List[(int,DirectDatArgument)]
        self.direct_writes=set() # type:List[(int,DirectDatArgument)]
        for (i,arg) in enumerate(stat.arguments):
            if isinstance(arg,IndirectDatArgument):
                if arg.access_mode==AccessMode.READ:
                    self.indirect_reads.add( (i,arg) )
                elif arg.access_mode==AccessMode.WRITE:
                    self.indirect_writes.add( (i,arg) )
                elif arg.access_mode==AccessMode.RW:
                    self.indirect_reads.add( (i,arg) )
                    self.indirect_writes.add( (i,arg) )
                elif arg.access_mode==AccessMode.INC:
                    self.indirect_writes.add( (i,arg) )
                else:
                    raise RuntimeError("Unexpected access mode for indirect dat.")
            elif isinstance(arg,GlobalArgument):
                if arg.access_mode==AccessMode.READ:
                    if isinstance(arg.global_,ConstGlobal):
                        self.const_global_reads.add( (i,arg) )
                    else:
                        self.mutable_global_reads.add( (i,arg) )
                elif arg.access_mode==AccessMode.INC:
                    self.global_writes.add( (i,arg) )
                else:
                    raise RuntimeError("Unexpected access mode for global.")
            elif isinstance(arg,DirectDatArgument):
                if arg.access_mode==AccessMode.READ:
                    self.direct_reads.add( (i,arg) )
                elif arg.access_mode==AccessMode.WRITE:
                    self.direct_writes.add( (i,arg) )
                elif arg.access_mode==AccessMode.RW:
                    self.direct_reads.add( (i,arg) )
                    self.direct_writes.add( (i,arg) )
                elif arg.access_mode==AccessMode.INC:
                    self.direct_writes.add( (i,arg) )
                else:
                    raise RuntimeError("Unexpected access mode for direct dat.")
            else:
                raise RuntimeError("Unexpected argument type.")
        logging.info("Invocation %s : const global reads = %s", self.invocation, self.const_global_reads)
        logging.info("Invocation %s : mutable global reads = %s", self.invocation, self.mutable_global_reads)
        logging.info("Invocation %s : global writes = %s", self.invocation, self.global_writes)
        logging.info("Invocation %s : indirect reads = %s", self.invocation, self.indirect_reads)
        logging.info("Invocation %s : indirect writes = %s", self.invocation, self.indirect_writes)
        logging.info("Invocation %s : direct reads = %s", self.invocation, self.direct_reads)
        logging.info("Invocation %s : direct writes = %s", self.invocation, self.direct_writes)    


def compile_invocation(spec:SystemSpecification, builder:GraphTypeBuilder, stat:ParFor):
    ctxt=InvocationContext(spec, stat)
    
    with builder.subst(base=ctxt.invocation):
        directDevType="set_"+stat.iter_set.id
        
        triggerMsgType=builder.create_message_type(
            "{base}_trigger",
            { "global_"+mg.global_.id : mg.data_type for (i,mg) in ctxt.mutable_global_reads }
        )
        
        completeMsgType=builder.create_message_type(
            "{base}_complete",
            { "global_"+mg.global_.id : mg.data_type for (i,mg) in ctxt.global_writes }
        )
        
        for (i,arg) in ctxt.mutable_global_reads | ctxt.global_writes:
            # If it doesn't already exist, we need a buffer
            builder.merge_device_state(directDevType, "global_"+arg.global_.id, arg.data_type)
        
        for (i,arg) in ctxt.indirect_reads | ctxt.indirect_writes:
            # We need a working area for the argument in the state of the home set
            builder.add_device_state(directDevType, "{base}_arg"+str(i), arg.data_type)
                    
        ###########################################
        ## Deal with indirect reads.
        ## - Message broadcast from global to all indirect sets
        ## - Indirect sets then broadcast each dat involved
        
        builder.add_device_state(directDevType, "{base}_received", scalar_uint32)
        total_read_args_pending=0
        for (index,arg) in ctxt.indirect_reads:
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

                # And when the message arrives, just copy it into the landing pad
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
        for (ai,arg) in ctxt.mutable_global_reads:
            builder.extend_input_pin_handler(directDevType, "{base}_trigger",
                """
                copy_value(deviceState->{}, message->{});
                """.format(arg.global_.id,arg.global_.id)
            )
        
        # This tracks how many outputs have been sent. Each index >1 represents an indirect
        # output argument, and index==1 represents the final completion message.
        builder.add_device_state(directDevType, "{base}_to_send", scalar_uint32)
                
        with builder.subst(exec_thresh=total_read_args_pending, output_count=len(ctxt.indirect_writes)):
            builder.add_output_pin(directDevType, "{base}_execute", "executeMsgType",
                """
                if(deviceState->{base}_received=={exec_thresh}){{
                  *readyToSend |= RTS_FLAG_{base}_execute;
                }}
                """,
                """
                
                    TODO
                    
                deviceState->{base}_to_send = 1+{output_count};    
                """
            )
        
        ## Send all the output indirect values
        
        ordered_indirect_writes=list(ctxt.indirect_writes) # Impose an ordering on the outputs
        for (order,(index,arg)) in enumerate(ordered_indirect_writes):
            with builder.subst(trigger_num=1+order, dat_name=arg.dat.id, index=index):
                indirectDevType="set_"+arg.to_set.id
                
                # Might already exist if it is a RW type, so merge
                dataMsgType=builder.merge_message_type("{base}_indirect_arg{index}", { "value":arg.data_type })
            
                
                builder.add_output_pin(directDevType, "{base}_send_arg{index}", dataMsgType,
                    """
                    if(deviceState->{base}_to_send=={trigger_num}){{
                        *readyToSend|=RTS_FLAG_{base}_send_arg{index};
                    }}
                    """,
                    """
                    copy_value(message->value, deviceState->{base}_arg{index});
                    deviceState->{base}_to_send--;
                    """
                )
                
                builder.add_input_pin(indirectDevType, "{base}_recv_arg{index}", dataMsgType, None, None,
                    """
                    copy_value(deviceState->{dat_name}, message->value);
                    deviceState->{base}_received++;
                    """
                )
        
        ## TODO : for all devices in indirect receive set, send completion when all indirects received
                
        
        ## Send back the complete message from the home set
        
            
    
   
    
        
        

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
    
    builder.build_and_save(sys.stdout)
    
