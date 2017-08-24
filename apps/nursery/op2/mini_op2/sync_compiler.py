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
    def get_indirect_read_dats(self) -> typing.Set[ Dat ]:
        """Find all the dats that are read from indirectly."""
        res=set()
        for (i,arg) in self.indirect_reads:
            res.add( arg.dat )
        return res
    
    def get_indirect_writes(self) -> Dict[ Set, List[ Tuple[int,Argument] ] ]:
        """Find all the sets that are involve as indirect writes, and all the arguments (i.e. dats) that
        then are read from that set."""
        res={}
        for (i,arg) in self.indirect_reads:
            res.setdefault(arg.iter_set,[]).append( (i,arg) )
        return res
        
    def get_all_involved_sets(self) -> typing.Set[ Set ] :
        """Return a list of all sets involved as direct, indirect read, or indirect write"""
        return set([self.stat.iter_set] + [ arg.to_set for (ai,arg) in (self.indirect_reads | self.indirect_writes) ])
    
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


def create_all_messages(ctxt:InvocationContext, builder:GraphTypeBuilder):
    read_globals={}
    for (ai,arg) in ctxt.mutable_global_reads:
        # If global appears twice, then some keys may get overwritten with same type
        read_globals["global_{}".format(arg.global_.id)]=arg.data_type
    builder.create_message_type("{invocation}_begin", read_globals)
    
    write_globals={}
    for (ai,arg) in ctxt.global_writes:
        # If global appears twice, then some keys may get overwritten with same type
        write_globals["global_{}".format(arg.global_.id)]=arg.data_type
    builder.create_message_type("{invocation}_end", write_globals)
    
    indirect_dats=set()
    for (ai,arg) in ctxt.indirect_reads | ctxt.indirect_writes:
        indirect_dats.add(arg.dat)
    for dat in indirect_dats:
        with builder.subst(dat=dat.id):
            builder.merge_message_type("dat_{dat}", { "value":dat.data_type } )

def create_state_tracking_variables(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        with builder.subst(set=set.id):
            builder.add_device_state("set_{set}", "{invocation}_in_progress", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_read_send_mask", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_read_recv_count", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_write_send_mask", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_write_recv_count", scalar_uint32)
            
            builder.add_device_property("set_{set}", "{invocation}_read_recv_total", scalar_uint32)
            builder.add_device_property("set_{set}", "{invocation}_write_recv_total", scalar_uint32)

def create_indirect_landing_pads(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ( ctxt.indirect_reads | ctxt.indirect_writes ):
        with builder.subst(index=ai, set=ctxt.stat.iter_set.id):
            builder.add_device_state("set_{set}", "{invocation}_arg{index}_buffer", arg.dat.data_type)

def create_global_landing_pads(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ( ctxt.mutable_global_reads | ctxt.global_writes ):
        with builder.subst(name=arg.global_.id, set=ctxt.stat.iter_set.id):
            builder.merge_device_state("set_{set}", "global_{name}", arg.data_type)
            
def create_all_state_and_properties(ctxt:InvocationContext, builder:GraphTypeBuilder):
    create_state_tracking_variables(ctxt, builder)
    create_indirect_landing_pads(ctxt, builder)
    create_global_landing_pads(ctxt, builder)
    
def create_read_send_masks(ctxt:InvocationContext, builder:GraphTypeBuilder):
    read_sends={}
    for (ai,arg) in ctxt.indirect_reads:
        read_sends.setdefault(arg.to_set,set()).add( arg.dat )
    
    for (set_,dats) in read_sends.items():
        read_send_set=["0"] + [ builder.s("{invocation}_dat{dat}_read_send",dat=dat.id) for dat in dats ]
        read_send_set="(" + "|".join(read_send_set) + ")"
        
        with builder.subst(set=set_.id, read_send_set=read_send_set):
            builder.add_device_shared_code("set_{set}", 
                """
                const uint32_t {invocation}_{set}_read_send_mask_all = {read_send_set};
                """
                )

def create_write_send_masks(ctxt:InvocationContext, builder:GraphTypeBuilder):
    """ Creates a mask called {invocation}_{set}_write_send_mask_all for each involved set."""
    write_sends={}
    for (ai,arg) in ctxt.indirect_writes:
        write_sends.setdefault(arg.to_set,[]).append( (ai,arg) )
    
    for (set,args) in write_sends.items():
        write_send_set=["0"] + [ builder.s("{invocation}_arg{index}_write_send",index=ai) for (ai,arg) in args ]
        write_send_set="(" + "|".join(write_send_set) + ")"
        
        with builder.subst(set=set.id, write_send_set=write_send_set):
            builder.add_device_shared_code("set_{set}", 
                """
                const uint32_t {invocation}_{set}_write_send_mask_all = {write_send_set};
                """
                )

def create_invocation_execute(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        write_send_set=["0"] + [ builder.s("{invocation}_arg{index}_write_send", index=ai) for (ai,arg) in ctxt.indirect_writes ]
        write_send_set="(" + ("|".join(write_send_set)) + ")"
        
        with builder.subst(set=set.id, kernel=ctxt.stat.name):
            handler="""
            assert( deviceState->{invocation}_in_progress );
            assert( {invocation}_read_recv_count == deviceProperties->{invocation}_read_recv_total );
            
            deviceState->{invocation}_read_recv_count=0; // Allows us to tell that execute has happened
            deviceState->{invocation}_write_recv_count++; // The increment for this "virtual" receive
            deviceState->{invocation}_write_send_mask = {invocation}_{set}_write_send_mask_all; // In device-local shared code
            """
            
            if set==ctxt.stat.iter_set:
                handler+="kernel_{kernel}(\n"
                for (ai,arg) in enumerate(ctxt.stat.arguments):
                    if isinstance(arg,GlobalArgument):
                        if (ai,arg) in ctxt.mutable_global_reads | ctxt.global_writes:
                            handler+=builder.s("  deviceState->global_{id}",id=arg.global_.id)
                        else:
                            handler+=builder.s("  graphProperties->global_{id}", id=arg.global_.id)
                    elif isinstance(arg,DirectDatArgument):
                        handler+=builder.s("  deviceState->dat_{dat}",dat=arg.dat.id)
                    elif isinstance(arg,IndirectDatArgument):
                        handler+=builder.s("  deviceState->{invocation}_arg{index}_buffer", index=ai)
                    else:
                        raise RuntimeError("Unexpected arg type.")
                    if ai+1<len(ctxt.stat.arguments):
                        handler+=","
                    handler+="\n"
                handler+=");\n"
            
            builder.add_output_pin("set_{set}", "{invocation}_execute", "executeMsgType",
                handler
            )



def create_invocation_begin(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        with builder.subst(set=set.id):
            handler="""
            // Standard edge trigger for start of invocation
            if(!deviceState->{invocation}_in_progress){{
                deviceState->{invocation}_in_progress=1;
                deviceState->{invocation}_read_send_mask = {invocation}_{set}_read_send_mask_all;
            }}
            deviceState->{invocation}_read_recv_count++;
            """
            if set==ctxt.stat.iter_set:
                for (ai,arg) in ctxt.mutable_global_reads:
                    handler+="""
                    copy_value(deviceState->global_{}, message->global_{});"
                    """.format(arg.global_.id)
            builder.add_input_pin("set_{set}", "{invocation}_begin", "{invocation}_begin", None, None, handler)

def create_invocation_end(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        with builder.subst(set=set.id):
            handler="""
            assert(deviceState->{invocation}_in_progress);
            assert( deviceState->{invocation}_read_send_mask == 0 );
            assert( deviceState->{invocation}_write_send_mask == 0 );
            assert( deviceState->{invocation}_read_recv_count == deviceProperties->{invocation}_read_recv_total );
            assert( deviceState->{invocation}_write_recv_count == deviceProperties->{invocation}_write_recv_total );
            
            deviceState->{invocation}_in_progress=0;
            deviceState->{invocation}_read_recv_count=0;
            deviceState->{invocation}_write_send_count=0;
            """
            if set==ctxt.stat.iter_set:
                for (ai,arg) in ctxt.global_writes:
                    handler+="""
                    copy_value(message->global_{}, deviceState->global_{});
                    """.format(arg.global_.id,arg.global_.id)
            else:
                for (ai,arg) in ctxt.global_writes:
                    handler+="""
                    zero_value(message->global_{});
                    """.format(arg.global_.id)
            builder.add_output_pin("set_{set}", "{invocation}_end", "{invocation}_end", handler)
        

def create_indirect_read_sends(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for dat in ctxt.get_indirect_read_dats():
        with builder.subst(dat=dat.id, set=dat.set.id):
            builder.add_output_pin("set_{set}", "{invocation}_dat{dat}_read_send", "dat_{dat}",
                """
                assert(deviceState->{invocation}_read_send_mask & RTS_FLAG_{invocation}_dat{dat}_read_send);
                copy_value(message->value, deviceState->dat_{dat});
                deviceState->{invocation}_read_send_mask &= ~RTS_FLAG_{invocation}_dat{dat}_read_send;
                """
            )

def create_indirect_read_recvs(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ctxt.indirect_reads:
        with builder.subst(set=ctxt.stat.iter_set.id, index=ai, dat=arg.dat.id):
            builder.add_input_pin("set_{set}", "{invocation}_arg{index}_read_recv", "dat_{dat}", None, None,
                """
                assert(deviceState->{invocation}_read_recv_count < deviceProperties->{invocation}_read_recv_total);
                // Standard edge trigger for start of invocation
                if(!deviceState->{invocation}_in_progress){{
                    deviceState->{invocation}_in_progress=1;
                    deviceState->{invocation}_read_send_mask = {invocation}_{set}_read_send_mask_all;
                }}
                deviceState->{invocation}_read_recv_count++;
                copy_value(deviceState->{invocation}_arg{index}_buffer, message->value);                
                """
            )


            
def create_indirect_write_sends(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ctxt.indirect_writes:
        with builder.subst(index=ai, set=arg.iter_set.id, dat=arg.dat.id):
            builder.add_output_pin("set_{set}", "{invocation}_arg{index}_write_send", "dat_{dat}",
                """
                assert(deviceState->{invocation}_write_send_mask & RTS_FLAG_{invocation}_arg{index}_write_send);
                copy_value(message->value, deviceState->{invocation}_arg{index}_buffer);
                deviceState->{invocation}_write_send_mask &= ~RTS_FLAG_{invocation}_arg{index}_write_send;
                """
            )

def create_indirect_write_recvs(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ctxt.indirect_writes:
        with builder.subst(set=ctxt.stat.iter_set.id, index=ai, dat=arg.dat.id):
            handler="""
                assert(deviceState->{invocation}_read_recv_count < deviceProperties->{invocation}_read_recv_total);
                // Standard edge trigger for start of invocation
                if(!deviceState->{invocation}_in_progress){{
                    deviceState->{invocation}_in_progress=1;
                    deviceState->{invocation}_read_send_mask = {invocation}_{set}_read_send_mask_all;
                }}
                deviceState->{invocation}_write_recv_count++;
            """
            if arg.access_mode==AccessMode.WRITE or arg.access_mode==AccessMode.RW:
                handler+="""
                copy_value(deviceState->{invocation}_arg{index}_buffer, message->value);                
                """
            elif arg.access_mode==AccessMode.INC:
                handler+="""
                inc_value(deviceState->{invocation}_arg{index}_buffer, message->value);                
                """
            else:
                raise RuntimeError("Unexpected access mode {}".format(arg.access_mode))
            builder.add_input_pin("set_{set}", "{invocation}_arg{index}_read_recv", "dat_{dat}", None, None, handler)

def create_rts(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        with builder.subst(set=set.id):
            rts="""
            if(deviceState->{invocation}_in_progress) {{
              *readyToSend |= deviceState->{invocation}_read_send_mask;
              *readyToSend |= deviceState->{invocation}_write_send_mask;
              if(deviceState->{invocation}_read_recv_count == deviceProperties->{invocation}_read_recv_total){{
                *readyToSend |= RTS_FLAG_{invocation}_execute;
              }}
              if(deviceState->{invocation}_read_recv_count==0
                  && deviceState->{invocation}_write_recv_count==deviceProperties->{invocation}_write_recv_total
                  && deviceState->{invocation}_read_send_mask==0
                  && deviceState->{invocation}_write_send_mask==0
              ){{
                *readyToSend |= RTS_FLAG_{invocation}_end;
              }}
            }} // if(deviceState->{invocation}_in_progress)
            """
            builder.add_rts_clause("set_{set}", rts)


def compile_invocation(spec:SystemSpecification, builder:GraphTypeBuilder, stat:ParFor):
    ctxt=InvocationContext(spec, stat)
    
    with builder.subst(invocation=ctxt.invocation):
        create_all_messages(ctxt,builder)
        
        create_all_state_and_properties(ctxt, builder)
        create_read_send_masks(ctxt, builder)
        create_write_send_masks(ctxt, builder)

        create_invocation_begin(ctxt,builder)
        create_indirect_read_sends(ctxt,builder)
        create_indirect_read_recvs(ctxt,builder)
        create_invocation_execute(ctxt,builder)
        create_indirect_write_sends(ctxt,builder)
        create_indirect_write_recvs(ctxt,builder)
        create_invocation_end(ctxt,builder)
        create_rts(ctxt,builder)
        

#~ def compile_invocation(spec:SystemSpecification, builder:GraphTypeBuilder, stat:ParFor):
    #~ ctxt=InvocationContext(spec, stat)
    
    #~ with builder.subst(base=ctxt.invocation):
        #~ directDevType="set_"+stat.iter_set.id
        
        #~ triggerMsgType=builder.create_message_type(
            #~ "{base}_trigger",
            #~ { "global_"+mg.global_.id : mg.data_type for (i,mg) in ctxt.mutable_global_reads }
        #~ )
        
        #~ completeMsgType=builder.create_message_type(
            #~ "{base}_complete",
            #~ { "global_"+mg.global_.id : mg.data_type for (i,mg) in ctxt.global_writes }
        #~ )
        
        #~ for (i,arg) in ctxt.mutable_global_reads | ctxt.global_writes:
            #~ # If it doesn't already exist, we need a buffer
            #~ builder.merge_device_state(directDevType, "global_"+arg.global_.id, arg.data_type)
        
        #~ for (i,arg) in ctxt.indirect_reads | ctxt.indirect_writes:
            #~ # We need a working area for the argument in the state of the home set
            #~ builder.add_device_state(directDevType, "{base}_arg"+str(i), arg.data_type)
                    
        #~ ###########################################
        #~ ## Deal with indirect reads.
        #~ ## - Message broadcast from global to all indirect sets
        #~ ## - Indirect sets then broadcast each dat involved
        
        #~ builder.add_device_state(directDevType, "{base}_received", scalar_uint32)
        #~ total_read_args_pending=0
        #~ for (index,arg) in ctxt.indirect_reads:
            #~ indirectDevType="set_"+arg.to_set.id
            #~ with builder.subst(index=index, dat_name=arg.dat.id):
                
                #~ dataMsgType=builder.create_message_type("{base}_indirect_arg{index}", { "value":arg.data_type })
                
                #~ # On the indirect device we need to flag whether the dat send is pending
                #~ builder.add_device_state(directDevType, "{base}_send_pending_arg{index}", scalar_uint32)
                #~ builder.add_input_pin(indirectDevType, "{base}_trigger_arg{index}", triggerMsgType, None, None,
                    #~ # Receive handler
                    #~ """
                    #~ assert(deviceState->{base}_trigger_arg{index}==0);
                    #~ deviceState->{base}_trigger_arg{index}=1;
                    #~ """
                #~ )
                #~ builder.add_output_pin(indirectDevType, "{base}_send_arg{index}", dataMsgType,
                    #~ # Ready to send
                    #~ """
                    #~ if(deviceState->{base}_trigger_arg{index}){{
                        #~ *readyToSend |= RTS_FLAG_{base}_trigger_arg{index};
                    #~ }}
                    #~ """,
                    #~ # Send handler
                    #~ """
                    #~ assert(deviceState->{base}_trigger_arg{index});
                    #~ copy_value(message->value, deviceState->{dat_name});
                    #~ deviceState->{base}_trigger_arg{index}=0;
                    #~ """
                #~ )

                #~ # And when the message arrives, just copy it into the landing pad
                #~ builder.add_input_pin(directDevType,"{base}_recv_arg{index}", dataMsgType, None, None,
                    #~ """
                    #~ copy_value(deviceState->{base}_arg{index}, message->value);
                    #~ deviceState->{base}_received++;
                    #~ """
                #~ )
                
                #~ total_read_args_pending+=1
        
        #~ ############################################
        #~ ## Deal with parallel iteration over set
        #~ ## - Receive message from globals with any constants, and to indicate kernel should start
        #~ ## - Also wait for any indirect reads
        
        #~ builder.add_device_state(directDevType, "{base}_ready", scalar_uint32)
        #~ builder.add_input_pin(directDevType,"{base}_trigger", triggerMsgType, None, None,
            #~ """"
            #~ assert(!*deviceState->{base}_ready);
            #~ deviceState->{base}_received++;
            #~ """
        #~ )
        #~ for (ai,arg) in ctxt.mutable_global_reads:
            #~ builder.extend_input_pin_handler(directDevType, "{base}_trigger",
                #~ """
                #~ copy_value(deviceState->{}, message->{});
                #~ """.format(arg.global_.id,arg.global_.id)
            #~ )
        
        #~ # This tracks how many outputs have been sent. Each index >1 represents an indirect
        #~ # output argument, and index==1 represents the final completion message.
        #~ builder.add_device_state(directDevType, "{base}_to_send", scalar_uint32)
                
        #~ with builder.subst(exec_thresh=total_read_args_pending, output_count=len(ctxt.indirect_writes)):
            #~ builder.add_output_pin(directDevType, "{base}_execute", "executeMsgType",
                #~ """
                #~ if(deviceState->{base}_received=={exec_thresh}){{
                  #~ *readyToSend |= RTS_FLAG_{base}_execute;
                #~ }}
                #~ """,
                #~ """
                
                    #~ TODO
                    
                #~ deviceState->{base}_to_send = 1+{output_count};    
                #~ """
            #~ )
        
        #~ ## Send all the output indirect values
        
        #~ ordered_indirect_writes=list(ctxt.indirect_writes) # Impose an ordering on the outputs
        #~ for (order,(index,arg)) in enumerate(ordered_indirect_writes):
            #~ with builder.subst(trigger_num=1+order, dat_name=arg.dat.id, index=index):
                #~ indirectDevType="set_"+arg.to_set.id
                
                #~ # Might already exist if it is a RW type, so merge
                #~ dataMsgType=builder.merge_message_type("{base}_indirect_arg{index}", { "value":arg.data_type })
            
                
                #~ builder.add_output_pin(directDevType, "{base}_send_arg{index}", dataMsgType,
                    #~ """
                    #~ if(deviceState->{base}_to_send=={trigger_num}){{
                        #~ *readyToSend|=RTS_FLAG_{base}_send_arg{index};
                    #~ }}
                    #~ """,
                    #~ """
                    #~ copy_value(message->value, deviceState->{base}_arg{index});
                    #~ deviceState->{base}_to_send--;
                    #~ """
                #~ )
                
                #~ builder.add_input_pin(indirectDevType, "{base}_recv_arg{index}", dataMsgType, None, None,
                    #~ """
                    #~ copy_value(deviceState->{dat_name}, message->value);
                    #~ deviceState->{base}_received++;
                    #~ """
                #~ )
        
        #~ ## TODO : for all devices in indirect receive set, send completion when all indirects received
                
        
        #~ ## Send back the complete message from the home set
        
        

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
            kernels.append(stat)
    for stat in kernels:
        compile_invocation(spec,builder,stat)
    
    return builder
            

if __name__=="__main__":
    logging.basicConfig(level=4)
    
    (spec,inst,code)=build_system()
    builder=sync_compiler(spec,code)
    
    builder.build_and_save(sys.stdout)
    
