import typing
from typing import Any, Dict, List

import sys
import logging

from contextlib import contextmanager

from mini_op2.framework.core import *
from mini_op2.framework.control_flow import Statement, ParFor, Seq, UserCode, CompositeStatement, CheckState, While
from mini_op2.framework.system import SystemSpecification
from mini_op2.framework.builder import GraphTypeBuilder, raw

import mini_op2.framework.kernel_translator

scalar_uint32=DataType(shape=(),dtype=numpy.uint32)

_unq_id_to_obj={} # type:Dict[Any,str]
_unq_obj_to_id={} # type:Dict[str,Any]


def make_unique_id(obj:Any,base:str) -> str:
    name=base
    while name in _unq_id_to_obj:
        name="{}_{}".format(name,len(_unq_id_to_obj))
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
        for (i,arg) in self.indirect_writes:
            res.setdefault(arg.iter_set,[]).append( (i,arg) )
        return res
        
    def get_all_involved_sets(self) -> typing.Set[ Set ] :
        """Return a list of all sets involved as direct, indirect read, or indirect write"""
        if isinstance(self.stat,ParFor):
            return set([self.stat.iter_set] + [ arg.to_set for (ai,arg) in (self.indirect_reads | self.indirect_writes) ])
        else:
            return set([])
    
    def __init__(self, spec:SystemSpecification, stat:Union[ParFor,UserCode]) -> None:
        super().__init__()
        self.spec=spec
        self.stat=stat
        
        is_par_for=isinstance(stat,ParFor)
        is_scalar=isinstance(stat,UserCode)
        
        if stat.id:
            self.invocation=stat.id
        else:
            # Bind the invocation id at this point, as we can't make it unique earlier on
            self.invocation=get_unique_id(stat)
            stat.id=self.invocation
            logging.info("Binding %s to %s", stat, self.invocation)
        
        self.const_global_reads=set() # type:List[(int,GlobalArgument)]
        self.mutable_global_reads=set() # type:List[(int,GlobalArgument)]
        self.global_writes=set() # type:List[(int,GlobalArgument)]
        self.indirect_reads=set() # type:List[(int,IndirectArgument)]
        self.indirect_writes=set() # type:List[(int,IndirectArgument)]
        self.direct_reads=set() # type:List[(int,DirectDatArgument)]
        self.direct_writes=set() # type:List[(int,DirectDatArgument)]
        for (i,arg) in enumerate(stat.arguments):
            if isinstance(arg,IndirectDatArgument):
                assert is_par_for
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
                elif arg.access_mode==AccessMode.WRITE:
                    assert is_scalar
                    self.global_writes.add( (i,arg) )
                elif arg.access_mode==AccessMode.RW:
                    assert is_scalar
                    self.global_writes.add( (i,arg) )
                    self.global_reads.add( (i,arg) )
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
    
    for (ai,arg) in ctxt.indirect_reads | ctxt.indirect_writes:
        dat=arg.dat
        with builder.subst(dat=dat.id,arity=-arg.index):
            builder.merge_message_type("dat_{dat}", { "value":dat.data_type } )
            if arg.index<0 and (ai,arg) in ctxt.indirect_writes:
                dt=DataType(dat.data_type.dtype, (-arg.index,)+dat.data_type.shape)
                builder.merge_message_type("dat_{dat}_x{arity}", { "value":dt } )
    
def create_state_tracking_variables(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        with builder.subst(set=set.id):
            builder.add_device_state("set_{set}", "{invocation}_in_progress", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_read_send_mask", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_read_recv_count", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_write_send_mask", scalar_uint32)
            builder.add_device_state("set_{set}", "{invocation}_write_recv_count", scalar_uint32)
            
            builder.add_device_property("set_{set}", "{invocation}_write_recv_total", scalar_uint32)

def create_indirect_landing_pads(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ( ctxt.indirect_reads | ctxt.indirect_writes ):
        with builder.subst(index=ai, set=ctxt.stat.iter_set.id):
            if arg.index < 0:
                vtype=DataType(arg.dat.data_type.dtype, (-arg.index,)+arg.dat.data_type.shape)
                builder.add_device_state("set_{set}", "{invocation}_arg{index}_buffer", vtype)
            else:
                builder.add_device_state("set_{set}", "{invocation}_arg{index}_buffer", arg.dat.data_type)

def create_global_landing_pads(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ( ctxt.mutable_global_reads | ctxt.global_writes ):
        with builder.subst(name=arg.global_.id, set=ctxt.stat.iter_set.id):
            builder.merge_device_state("set_{set}", "global_{name}", arg.data_type)
            
def create_all_state_and_properties(ctxt:InvocationContext, builder:GraphTypeBuilder):
    builder.add_graph_property("{invocation}_total_responding_devices", scalar_uint32)
    create_state_tracking_variables(ctxt, builder)
    create_indirect_landing_pads(ctxt, builder)
    create_global_landing_pads(ctxt, builder)
    
def create_read_send_masks(ctxt:InvocationContext, builder:GraphTypeBuilder):
    read_sends={ s:set() for s in ctxt.get_all_involved_sets() }
    for (ai,arg) in ctxt.indirect_reads:
        read_sends[arg.to_set].add( arg.dat )
    
    for (set_,dats) in read_sends.items():
        read_send_set=["0"] + [ builder.s("RTS_FLAG_{invocation}_dat_{dat}_read_send",dat=dat.id) for dat in dats ]
        read_send_set="(" + "|".join(read_send_set) + ")"
        
        with builder.subst(set=set_.id, read_send_set=read_send_set):
            builder.add_device_shared_code("set_{set}", 
                """
                const uint32_t {invocation}_{set}_read_send_mask_all = {read_send_set};
                """
                )

def create_read_recv_counts(ctxt:InvocationContext, builder:GraphTypeBuilder):
    
    # Start off at 1, as that represents {invocation}_begin
    read_recvs={ s:1 for s in ctxt.get_all_involved_sets() }
    
    for (ai,arg) in ctxt.indirect_reads:
        if arg.index<0:
            read_recvs[arg.iter_set] += -arg.index # We'll receive multiple values on this pin
        else:
            read_recvs[arg.iter_set] += 1 # Only get a single value
    
    for (set_,dats) in read_recvs.items():
        with builder.subst(set=set_.id, read_recv_count=read_recvs[set_]):
            builder.add_device_shared_code("set_{set}", 
                """
                const uint32_t {invocation}_{set}_read_recv_total = {read_recv_count};
                """
                )

def create_write_send_masks(ctxt:InvocationContext, builder:GraphTypeBuilder):
    """ Creates a mask called {invocation}_{set}_write_send_mask_all for each involved set."""
    write_sends={ s:set() for s in ctxt.get_all_involved_sets() }
    for (ai,arg) in ctxt.indirect_writes:
        write_sends[arg.iter_set].add( (ai,arg) )
    
    for (s,args) in write_sends.items():
        for (ai,arg) in args:
            logging.info("write send mask set=%s, arg=%s, to_set=%s, iter_set=%s", s.id, arg, arg.to_set.id, arg.iter_set.id)
        
        write_send_set=["0"] + [ builder.s("RTS_FLAG_{invocation}_arg{index}_write_send",index=ai) for (ai,arg) in args ]
        write_send_set="(" + "|".join(write_send_set) + ")"
        
        with builder.subst(set=s.id, write_send_set=write_send_set):
            builder.add_device_shared_code("set_{set}", 
                """
                const uint32_t {invocation}_{set}_write_send_mask_all = {write_send_set};
                """
                )

def create_invocation_execute(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        
        with builder.subst(set=set.id, kernel=ctxt.stat.name):
            handler="""
            assert( deviceState->{invocation}_in_progress );
            assert( deviceState->{invocation}_read_recv_count == {invocation}_{set}_read_recv_total );
            
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
                            handler+=builder.s("  (double*)graphProperties->global_{id}", id=arg.global_.id)
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
                    copy_value(deviceState->global_{}, message->global_{});
                    """.format(arg.global_.id,arg.global_.id)
            builder.add_input_pin("set_{set}", "{invocation}_begin", "{invocation}_begin", None, None, handler)

def create_invocation_end(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        with builder.subst(set=set.id):
            handler="""
            assert(deviceState->{invocation}_in_progress);
            assert( deviceState->{invocation}_read_send_mask == 0 );
            assert( deviceState->{invocation}_write_send_mask == 0 );
            assert( deviceState->{invocation}_read_recv_count == 0 );
            assert( deviceState->{invocation}_write_recv_count == deviceProperties->{invocation}_write_recv_total );
            
            deviceState->{invocation}_in_progress=0;
            deviceState->{invocation}_read_recv_count=0;
            deviceState->{invocation}_write_recv_count=0;
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
            builder.add_output_pin("set_{set}", "{invocation}_dat_{dat}_read_send", "dat_{dat}",
                """
                assert(deviceState->{invocation}_read_send_mask & RTS_FLAG_{invocation}_dat_{dat}_read_send);
                copy_value(message->value, deviceState->dat_{dat});
                deviceState->{invocation}_read_send_mask &= ~RTS_FLAG_{invocation}_dat_{dat}_read_send;
                """
            )

def create_indirect_read_recvs(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ctxt.indirect_reads:
        with builder.subst(set=ctxt.stat.iter_set.id, index=ai, dat=arg.dat.id):
            props={"index": DataType(numpy.uint8,shape=())}
            builder.add_input_pin("set_{set}", "{invocation}_arg{index}_read_recv", "dat_{dat}", props, None,
                """
                assert(deviceState->{invocation}_read_recv_count < {invocation}_{set}_read_recv_total);
                // Standard edge trigger for start of invocation
                if(!deviceState->{invocation}_in_progress){{
                    deviceState->{invocation}_in_progress=1;
                    deviceState->{invocation}_read_send_mask = {invocation}_{set}_read_send_mask_all;
                }}
                deviceState->{invocation}_read_recv_count++;
                """)
            if arg.index<0:
                builder.extend_input_pin_handler("set_{set}", "{invocation}_arg{index}_read_recv",
                """
                copy_value(deviceState->{invocation}_arg{index}_buffer[edgeProperties->index], message->value);                
                """)
            else:
                builder.extend_input_pin_handler("set_{set}", "{invocation}_arg{index}_read_recv",
                """
                copy_value(deviceState->{invocation}_arg{index}_buffer, message->value);                
                """)


            
def create_indirect_write_sends(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ctxt.indirect_writes:
        with builder.subst(index=ai, set=arg.iter_set.id, dat=arg.dat.id, arity=-arg.index):
            if arg.index>=0:
                builder.add_output_pin("set_{set}", "{invocation}_arg{index}_write_send", "dat_{dat}",
                    """
                    assert(deviceState->{invocation}_write_send_mask & RTS_FLAG_{invocation}_arg{index}_write_send);
                    copy_value(message->value, deviceState->{invocation}_arg{index}_buffer);
                    deviceState->{invocation}_write_send_mask &= ~RTS_FLAG_{invocation}_arg{index}_write_send;
                    """
                )
            else:
                builder.add_output_pin("set_{set}", "{invocation}_arg{index}_write_send", "dat_{dat}_x{arity}",
                    """
                    assert(deviceState->{invocation}_write_send_mask & RTS_FLAG_{invocation}_arg{index}_write_send);
                    copy_value(message->value, deviceState->{invocation}_arg{index}_buffer);
                    deviceState->{invocation}_write_send_mask &= ~RTS_FLAG_{invocation}_arg{index}_write_send;
                    """
                )

def create_indirect_write_recvs(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for (ai,arg) in ctxt.indirect_writes:
        with builder.subst(set=arg.to_set.id, index=ai, dat=arg.dat.id):
            handler="""
                assert(deviceState->{invocation}_write_recv_count < deviceProperties->{invocation}_write_recv_total);
                // Standard edge trigger for start of invocation
                if(!deviceState->{invocation}_in_progress){{
                    deviceState->{invocation}_in_progress=1;
                    deviceState->{invocation}_read_send_mask = {invocation}_{set}_read_send_mask_all;
                }}
                deviceState->{invocation}_write_recv_count++;
            """
            if arg.access_mode==AccessMode.WRITE or arg.access_mode==AccessMode.RW:
                handler+="""
                copy_value(deviceState->dat_{dat}, message->value);                
                """
            elif arg.access_mode==AccessMode.INC:
                handler+="""
                inc_value(deviceState->dat_{dat}, message->value);                
                """
            else:
                raise RuntimeError("Unexpected access mode {}".format(arg.access_mode))
            builder.add_input_pin("set_{set}", "{invocation}_arg{index}_write_recv", "dat_{dat}", None, None, handler)

def create_rts(ctxt:InvocationContext, builder:GraphTypeBuilder):
    for set in ctxt.get_all_involved_sets():
        with builder.subst(set=set.id):
            rts="""
            if(deviceState->{invocation}_in_progress) {{
              *readyToSend |= deviceState->{invocation}_read_send_mask;
              *readyToSend |= deviceState->{invocation}_write_send_mask;
              if(deviceState->{invocation}_read_recv_count == {invocation}_{set}_read_recv_total){{
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


def create_kernel_function(ctxt:InvocationContext, builder:GraphTypeBuilder):
    import importlib
    
    func=ctxt.stat.kernel
    name=func.__name__
    module=importlib.import_module(func.__module__)
    
    code=mini_op2.framework.kernel_translator.kernel_to_c(module, name)
    
    builder.add_device_shared_code_raw("set_{}".format(ctxt.stat.iter_set.id), code)


def create_invocation_tester(testIndex:int, isLast:bool, ctxt:InvocationContext, builder:GraphTypeBuilder):
    with builder.subst(testIndex=testIndex,isLast=int(isLast)):
        handler="""
            assert(deviceState->end_received==0);
            assert(deviceState->test_state==2*{testIndex});
            deviceState->test_state++; // Odd value means we are waiting for the return
            deviceState->end_received=0;
            """
        for (ai,arg) in ctxt.mutable_global_reads:
            handler+=builder.s("""
            copy_value(message->global_{name}, graphProperties->test_{invocation}_{name}_in);
            """,name=arg.global_.id)
        
        builder.add_output_pin("tester", "{invocation}_begin", "{invocation}_begin", handler)
        
        handler="""
            assert(deviceState->test_state==2*{testIndex}+1);
            assert(deviceState->end_received < graphProperties->{invocation}_total_responding_devices);
            deviceState->end_received++;
        """
        # Collect any inc's to global values
        for (ai,arg) in ctxt.global_writes:
            assert arg.access_mode==AccessMode.INC
            handler+=builder.s("""
            inc_value(deviceState->global_{name}, message->global_{name});
            """,name=arg.global_.id)
        # Check whether we have finished
        handler+="""
            if(deviceState->end_received == graphProperties->{invocation}_total_responding_devices){{
        """
        # Remove for now - not clear how to do this.
        if False:
            # ... and if so, try to check the results are "right"
            for (ai,arg) in ctxt.global_writes:
                handler+=builder.s("""
                    check_value(deviceState->global_{name}, graphProperties->test_{invocation}_{name}_out);
                """,name=arg.global_.id)
            
        handler+="""
                if( {isLast} ){{
                    handler_exit(0);
                }}else{{
                    deviceState->test_state++; // start the next invocation
                    deviceState->end_received=0;
                }}
            }}
        """
        builder.add_input_pin("tester", "{invocation}_end", "{invocation}_end", None, None, handler)
        
        builder.add_rts_clause("tester",
        """
        if(deviceState->test_state==2*{testIndex}){{
            *readyToSend = RTS_FLAG_{invocation}_begin;
        }}
        """
        )


"""

/* The state machine is a single function, which takes as input
   a pointer to a state number, and returns the parallel_for loop
   to execute.
   
   To avoid the potential for infinite loops, each transition in
   the spec state machine is a message that doesn't fire.
   
*/
   

void state_machine(
    device_state_t *devState,
    bool *cancel,
    uint32_t *readyToSend
){
    *cancel=0;
    *readyToSend=RTS_FLAG_control;

    while(1){
    switch(devState->state){
    case S0:
        ...
        devState->state=S1; Sequential step
        break;
    case S2:
        devState->state=S3; // Begin invocation wibble
        *readyToSend=RTS_FLAG_wibble_begin;
    case S3:
        // Finish invocation wibble
        goto S0;
    };
}

int state=0;
while(1){
    int invocation=state_machine();
    begin_invocation(invocation);
    wait end_invocation(invocation);
}
"""

_state_counter=0

def make_state():
    global _state_counter
    _state_counter+=1
    return _state_counter

_statement_to_state={}

def get_statement_state(s:Statement):
    global _statement_to_state
    if s in _statement_to_state:
        return _statement_to_state[s]
    ss=make_state()
    _statement_to_state[s]=ss
    return ss

def create_controller_states(s:Statement):
    get_statement_state(s)
    if isinstance(s, CompositeStatement):
        for c in s.children():
            create_controller_states(c)

def render_controller_statement_seq(s:Seq, next_state:int):
    children=list(s.children())
    assert len(children)>0
    
    handler=raw("""
    case {this_state}: // Seq. fall-through to first statement
        handler_log(4, "Seq {this_state} begin.");
    """.format(this_state=get_statement_state(s), first_child_state=get_statement_state(children[0])))
    
    for (i,c) in enumerate(children):
        if i+1==len(children):
            local_next_state=next_state
        else:
            local_next_state=get_statement_state(children[i+1])
        bit=render_controller_statement(c,local_next_state)
        logging.info("bit = %s", bit)
        assert isinstance(bit,raw)
        handler+=bit
    
    return handler
    
def render_controller_statement_while(s:While, next_state:int):
    children=list(s.children())
    assert len(children)>0
    
    cond_state=get_statement_state(s)
    body_state=get_statement_state(children[0])
    tail_state=make_state()
    
    handler=raw("""
    case {this_state}: // While. Condition state.
        handler_log(4, "While {this_state}, evaluate.");
        kernel_{user_code_id}(
    """.format(this_state=get_statement_state(s),user_code_id=s.id))
    
    args=list(s.arguments)
    for (i,arg) in enumerate(args):
        if isinstance(arg.global_,MutableGlobal):
            handler+="        deviceState->global_{}".format(arg.global_.id)
        elif isinstance(arg.global_,ConstGlobal):
            handler+="        graphProperties->global_{}".format(arg.global_.id)
        else:
            raise RuntimeError("Unknown arg type {}.".format(type(arg)))
        if i+1!=len(args):
            handler+=","
        handler+="\n"
    
    handler+="""
        );
        handler_log(4, "While {this_state}, condition = %d.", deviceState->global__cond_[0]);
        if(deviceState->global__cond_[0]){{
            deviceState->state={body_state};
        }}else{{
            deviceState->state={next_state};
        }}
        break;
    """.format(this_state=cond_state, body_state=body_state, next_state=next_state)
        
    
    for (i,c) in enumerate(children):
        if i+1==len(children):
            local_next_state=tail_state
        else:
            local_next_state=get_statement_state(children[i+1])
        bit=render_controller_statement(c,local_next_state)
        logging.info("bit = %s", bit)
        assert isinstance(bit,raw)
        handler+=bit
    
    handler+="""
    case {tail_state}:
        deviceState->state={cond_state};
        break;
    """.format(tail_state=tail_state,cond_state=cond_state)
    
    return handler
    
def render_controller_statement_par_for(s:ParFor, next_state:int):
    return raw("""
    case {this_state}: // ParFor
        {{
          *readyToSend=RTS_FLAG_{invocation}_begin;
          deviceState->state={next_state};
        }}
        break;
    """.format(invocation=s.id, next_state=next_state, this_state=get_statement_state(s)))
    
def render_controller_statement_user(s:UserCode, next_state:int):
    handler=raw("""
    case {this_state}: // UserCode
        {{
          handler_log(4, "User code {user_code_id} begin.");
          kernel_{user_code_id}(
    """.format(this_state=get_statement_state(s), user_code_id=s.id))
    
    args=list(s.arguments)
    for (i,arg) in enumerate(args):
        if isinstance(arg.global_,MutableGlobal):
            handler+="        deviceState->global_{}".format(arg.global_.id)
        elif isinstance(arg.global_,ConstGlobal):
            handler+="        graphProperties->global_{}".format(arg.global_.id)
        else:
            raise RuntimeError("Unknown arg type {}.".format(type(arg)))
        if i+1!=len(args):
            handler+=","
        handler+="\n"
    
    handler+="""
          );
          handler_log(4, "User code {user_code_id} end.");
        }}
        deviceState->state={next_state};
        break;
    """.format(next_state=next_state, user_code_id=s.id)
    
    return handler
    
def render_controller_statement_check_state(s:CheckState, next_state:int):
    handler=raw("""
    case {this_state}: // CheckState
        {{
    """.format(this_state=get_statement_state(s)))
    
    handler+="      // TODO : CheckState, pattern='{}'".format(s.pattern)
    
    handler+="""
        }}
        deviceState->state={next_state};
        break;
    """.format(next_state=next_state)
    
    return handler
    

def render_controller_statement(s:Statement, next_state:int):
    if isinstance(s,Seq):
        return render_controller_statement_seq(s, next_state)
    elif isinstance(s,ParFor):
        return render_controller_statement_par_for(s, next_state)
    elif isinstance(s,UserCode):
        return render_controller_statement_user(s, next_state)
    elif isinstance(s,CheckState):
        return render_controller_statement_check_state(s, next_state)
    elif isinstance(s,While):
        return render_controller_statement_while(s, next_state)
    else:
        raise RuntimeError("Didn't understand state of type {}.".format(type(s)))

def compile_global_controller(gi:str, spec:SystemSpecification, builder:GraphTypeBuilder, code:Statement):
    create_controller_states(code) # Make sure every statement has an entry state
    
    builder.add_device_state(gi, "rts", scalar_uint32)
    builder.add_device_state(gi, "state", scalar_uint32)
    
    # This will be used as a hidden global, and captures the value
    # of the condition for If and While
    assert "_cond_" in spec.globals
    
    start_state=get_statement_state(code)
    finish_state=make_state()
    
    builder.add_rts_clause(gi, "*readyToSend = deviceState->rts;\n")
    
    with builder.subst(start_state=start_state):
        builder.add_input_pin(gi, "__init__", "__init__", None, None,
            """
            deviceState->rts=RTS_FLAG_control;
            deviceState->state={start_state};
            handler_log(4, "rts=%x, state=%d", deviceState->rts, deviceState->state);
            """
        )
    
    handler=raw("""
    handler_log(4, "rts=%x, state=%d", deviceState->rts, deviceState->state);
    *doSend=0;  // disable this send...
    deviceState->rts=RTS_FLAG_control; // ... but say that we want to send again (by default)
    switch(deviceState->state){
    """)
    handler+=render_controller_statement(code, finish_state)
    handler+="""
    case {finish_state}:
        handler_log(4, "Hit finish state.");
        handler_exit(0);
        break;
    default:
        handler_log(3, "Unknown state id %d.", deviceState->state);
        assert(0);
    }}
    """.format(finish_state=finish_state)
    
    builder.create_message_type("control", {})
    builder.add_output_pin(gi, "control", "control", handler)
    
    for user_code in find_scalars_in_code(code):
        name=user_code.id
        assert user_code.ast
        src=mini_op2.framework.kernel_translator.scalar_to_c(user_code.ast, name)
        builder.add_device_shared_code("controller", raw(src))
    
    for k in code.all_statements():
        if isinstance(k,While):
            name=k.id
            src=mini_op2.framework.kernel_translator.scalar_to_c(k.expr_ast, name)
            builder.add_device_shared_code("controller", raw(src))


def compile_invocation(spec:SystemSpecification, builder:GraphTypeBuilder, ctxt:InvocationContext, emitted_kernels:set):    
    create_all_messages(ctxt,builder)
    
    create_all_state_and_properties(ctxt, builder)
    create_read_send_masks(ctxt, builder)
    create_read_recv_counts(ctxt, builder)
    create_write_send_masks(ctxt, builder)
    
    if ctxt.stat.kernel not in emitted_kernels:
        create_kernel_function(ctxt,builder)
        emitted_kernels.add(ctxt.stat.kernel)

    create_invocation_begin(ctxt,builder)
    create_indirect_read_sends(ctxt,builder)
    create_indirect_read_recvs(ctxt,builder)
    create_invocation_execute(ctxt,builder)
    create_indirect_write_sends(ctxt,builder)
    create_indirect_write_recvs(ctxt,builder)
    create_invocation_end(ctxt,builder)
    create_rts(ctxt,builder)
    

def find_kernels_in_code(code:Statement) -> List[ParFor]:
    kernels=[] # type:List[ParFor]
    for stat in code.all_statements():
        if isinstance(stat,ParFor):
            id=make_unique_id(stat, stat.name)
            kernels.append(stat)
    return kernels
    
def find_scalars_in_code(code:Statement) -> List[UserCode]:
    kernels=[] # type:List[UserCode]
    for stat in code.all_statements():
        if isinstance(stat,UserCode):
            id=make_unique_id(stat, "scalar")
            kernels.append(stat)
    return kernels


def sync_compiler(spec:SystemSpecification, code:Statement):
    builder=GraphTypeBuilder("op2_inst")
    
    builder.add_shared_code_raw(
    """
    #include <cmath>
    
    template<class T,unsigned N>
    void copy_value(T (&x)[N], const T (&y)[N]){
        for(unsigned i=0; i<N; i++){
            x[i]=y[i];
        }
    }
    
    template<class T,unsigned N,unsigned M>
    void copy_value(T (&x)[N][M], const T (&y)[N][M]){
        for(unsigned i=0; i<N; i++){
            for(unsigned j=0; j<M; j++){
                x[i][j]=y[i][j];
            }
        }
    }
    
    /*template<class T>
    void copy_value(T &x, const T (&y)[1]){
        x[0]=y[0];
    }*/
    
    template<class T,unsigned N>
    void inc_value(T (&x)[N], const T (&y)[N]){
        for(unsigned i=0; i<N; i++){
            x[i]+=y[i];
        }
    }
    
    template<class T,unsigned N>
    void zero_value(T (&x)[N]){
        for(unsigned i=0; i<N; i++){
            x[i]=0;
        }
    }
    
    /* Mainly for debug. Used to roughly check that a calculated value is correct, based
        on "known-good" pre-calculated values. Leaves a lot to be desired... */
    template<class T,unsigned N>
    void check_value(T (&got)[N], T (&ref)[N] ){
        for(unsigned i=0; i<N; i++){
            auto diff=std::abs( got[i] - ref[i] );
            assert( diff < 1e-6 ); // Bleh...
        }
    }
    """)
    
    builder.create_message_type("executeMsgType", {})
    
    # Support two kinds of global. Only one can be wired into an instance.
    builder.create_device_type("controller") # This runs the actual program logic
    builder.create_device_type("tester") # This solely tests each invocation in turn
    builder.add_device_state("tester", "test_state", DataType(shape=(), dtype=numpy.uint32))
    builder.add_device_state("tester", "end_received", DataType(shape=(), dtype=numpy.uint32))
    for global_ in spec.globals.values():
        if isinstance(global_,MutableGlobal):
            builder.add_device_state("controller", "global_{}".format(global_.id), global_.data_type)
            builder.add_device_state("tester", "global_{}".format(global_.id), global_.data_type)
        elif isinstance(global_,ConstGlobal):
            builder.add_graph_property("global_{}".format(global_.id), global_.data_type)
        else:
            raise RuntimeError("Unexpected global type : {}", type(global_))
            
    builder.merge_message_type("__init__", {})
    for s in spec.sets.values():
        with builder.subst(set="set_"+s.id):
            builder.create_device_type("{set}")
            init_handler=""
            for dat in s.dats.values():
                with builder.subst(dat=dat.id):
                    builder.add_device_property("{set}", "init_dat_{dat}", dat.data_type)
                    builder.add_device_state("{set}", "dat_{dat}", dat.data_type)
                    init_handler+=builder.s("       copy_value(deviceState->dat_{dat}, deviceProperties->init_dat_{dat});\n")
            builder.add_input_pin("{set}", "__init__", "__init__", None, None, init_handler)
    
    
    kernels=find_kernels_in_code(code)
            
    emitted_kernels=set()
    for (i,stat) in enumerate(kernels):
        ctxt=InvocationContext(spec, stat)
        with builder.subst(invocation=ctxt.invocation):
            compile_invocation(spec,builder,ctxt, emitted_kernels)
            create_invocation_tester(i, i+1==len(kernels), ctxt, builder) 
    
    compile_global_controller("controller", spec, builder, code)
    
    return builder
          
def load_model(args:List[str]):
    import mini_op2.apps.airfoil
    import mini_op2.apps.aero
    import mini_op2.apps.iota_sum
    import mini_op2.apps.dot_product
    import mini_op2.apps.odd_even_dot_product
    import mini_op2.apps.scalar_seq
    import mini_op2.apps.scalar_while
    
    model="airfoil"
    if len(args)>1:
        model=args[1]
    
    if len(args)>2:
        srcFile=args[2]
    elif model=="airfoil":
        srcFile="meshes/airfoil_1.5625%.hdf5"
    elif model=="aero":
        srcFile="meshes/aero_1.5625%.hdf5"
    elif model=="iota_sum":
        srcFile=8
    elif model=="dot_product":
        srcFile=8
    elif model=="odd_even_dot_product":
        srcFile=4
    elif model=="scalar_seq":
        srcFile=None
    elif model=="scalar_while":
        srcFile=None
    else:
        raise RuntimeError("Don't know a default file.")
    
    if model=="airfoil":
        build_system=mini_op2.apps.airfoil.build_system
    elif model=="aero":
        build_system=mini_op2.apps.aero.build_system
    elif model=="iota_sum":
        srcFile=int(srcFile)
        build_system=mini_op2.apps.iota_sum.build_system
    elif model=="dot_product":
        srcFile=int(srcFile)
        build_system=mini_op2.apps.dot_product.build_system
    elif model=="odd_even_dot_product":
        srcFile=int(srcFile)
        build_system=mini_op2.apps.odd_even_dot_product.build_system
    elif model=="scalar_seq":
        srcFile=None
        build_system=mini_op2.apps.scalar_seq.build_system
    elif model=="scalar_while":
        srcFile=None
        build_system=mini_op2.apps.scalar_while.build_system
    else:
        raise RuntimeError("Don't know this model.")

    return build_system(srcFile)

if __name__=="__main__":
    logging.basicConfig(level=4)
    
    (spec,inst,code)=load_model(sys.argv)
    builder=sync_compiler(spec,code)
    
    xml=builder.build_and_render()
    sys.stdout.write(xml)
    
