#!/usr/bin/env python3

import io
import sys
import os
import re
from typing import *

from graph.core import GraphType, DeviceType, OutputPin, InputPin, MessageType, GraphInstance, DeviceInstance, EdgeInstance
from graph.core import TypedDataSpec, TupleTypedDataSpec, ScalarTypedDataSpec, ArrayTypedDataSpec
from graph.load_xml import load_graph_type



###########################################################
## Some environments (OpenCL) don't like variadic macros,
## so they can be rewritten:
##  handler_log(4, "Wibble")  -> handler_log_0(4, "Wibble")
##  handler_log(4, "Wibble %x", )  -> handler_log_1(4, "Wibble %x", x)
## and so on


def _skip_expr(source:str, pos) -> int:
    while True:
        c=source[pos] # type: str
        #sys.stderr.write(f"  {c}\n")
        if c==')' or c==',':
            return pos
        elif c=='(':
            npos=_skip_expr(source,pos+1)
            assert npos>pos
            assert source[npos]==')'
            pos=npos+1
        elif c=='"':
            pos+=1
            while True:
                c=source[pos]
                pos+=1
                if c=='"':
                    break
                elif c=='\\':
                    pos+=1
                else:
                    pass
        else:
            pos+=1

def _count_args(source:str, pos) -> int:
    nargs=0
    assert source[pos-1]=='(', f"Input = '{source[pos:pos+50]}'"
    while True:
        c=source[pos] # type: str
        #sys.stderr.write(f"{c}\n")
        if c.isspace():
            pos+=1
        else:
            npos=_skip_expr(source,pos)
            assert npos>pos
            pos=npos
            nargs+=1
            assert source[pos]==',' or source[pos]==')'
            if source[pos]==')':
                return nargs
            pos+=1
        
_handler_log_re=re.compile("(\s+)(handler_log)(\s*[(])")

def rewrite_handler_log_instances(handler:str) -> str:
    """OpenCL doesn't allow variadic macros or functions, which makes handler_log a pain to implement.
    
    This function looks for handler_log instances, counts the arguments, and then rewrites into handler_log_N.

    It is _very_ fragile. For example, if 'handler_log(' appears in a string then it will treat it as a call.
    """

    while True:
        m = _handler_log_re.search(handler)
        if m is None:
            break
        pos=m.start(0)
        sys.stderr.write(f"pos = {pos}, chars = {handler[pos:pos+50]}\n")

        nargs=_count_args(handler, pos+len(m.group(0)))
        handler=_handler_log_re.sub( f"\\1handler_log_{nargs-2}\\3", handler, 1)
    
    return handler


####################################################################

def typed_data_spec_to_c_decl(dt:TypedDataSpec, name:Optional[str]=None) -> str:
    if name is None:
        name=dt.name
    if isinstance(dt,TupleTypedDataSpec):
        res="struct {\n"
        for e in dt.elements_by_index:
            res+=typed_data_spec_to_c_decl(e)+";\n"
        res+=f"}} {name}"
        return res
    elif isinstance(dt,ScalarTypedDataSpec):
        return f"{dt.type} {name}"
    elif isinstance(dt,ArrayTypedDataSpec):
        return typed_data_spec_to_c_decl(dt.type, name)+f"[{dt.length}]"
    else:
        raise RuntimeError("Unknown/unsupported data type: {}.".format(dt))

def render_typed_data_spec_as_struct(dt:TupleTypedDataSpec, id:str, dst:io.TextIOBase):
    assert isinstance(dt, (TupleTypedDataSpec,type(None)) )
    dst.write("typedef struct {\n")
    if dt is not None:
        for e in dt.elements_by_index:
            dst.write(typed_data_spec_to_c_decl(e))
            dst.write(";\n")
    dst.write(f"}} {id};\n\n")

class RenderOptions:
    def __init__(self):
        self.dst =sys.stdout # type:io.TextIOBase
        self.precise_activity_flags=True
        self.adapt_handler=lambda x: x

def render_graph_type_structs_as_sprovider(gt:GraphType, options:RenderOptions):
    dst=options.dst
    iprefix="sprovider_impl"
    adapt_handler=options.adapt_handler

    ###############################################################
    ## Do all the structs

    render_typed_data_spec_as_struct(gt.properties,  f"{gt.id}_properties_t", dst)
    for mt in gt.message_types.values(): # type:MessageType
        render_typed_data_spec_as_struct( mt.message, f"{mt.id}_message_t", dst)
    
    for dt in gt.device_types.values(): # type:DeviceType
        render_typed_data_spec_as_struct( dt.properties, f"{gt.id}_{dt.id}_properties_t", dst)
        render_typed_data_spec_as_struct( dt.state, f"{gt.id}_{dt.id}_state_t", dst)
        
        # I think these aliases are required
        dst.write(f"typedef {gt.id}_{dt.id}_properties_t {dt.id}_properties_t;\n")
        dst.write(f"typedef {gt.id}_{dt.id}_state_t {dt.id}_state_t;\n")

        for ip in dt.inputs_by_index:
            render_typed_data_spec_as_struct( ip.properties, f"{dt.id}_{ip.name}_properties_t", dst)
            render_typed_data_spec_as_struct( ip.state, f"{dt.id}_{ip.name}_state_t", dst)
        
    dst.write(f"typedef {gt.id}_properties_t GRAPH_PROPERTIES_T;")

    #################################################################
    ## RTS flag enums

    for dt in gt.device_types.values(): # type:DeviceType
        dst.write(f"enum {dt.id}_RTS_FLAGS {{")
        dst.write(",".join( f"RTS_FLAG_{dt.id}_{op.name} = 1<<{i}" for (i,op) in enumerate(dt.outputs_by_index) ))
        if len(dt.outputs_by_index)==0:
            dst.write(" _fake_RTS_FLAG_to_avoid_emptyness_")
        dst.write("};\n\n")

        dst.write(f"enum {dt.id}_RTS_INDEX {{")
        dst.write(",".join( f"RTS_INDEX_{dt.id}_{op.name} = {i}" for (i,op) in enumerate(dt.outputs_by_index) ))
        if len(dt.outputs_by_index)==0:
            dst.write(" _fake_RTS_INDEX_to_avoid_emptyness_")
        dst.write("};\n\n")



def render_graph_type_handlers_as_sprovider(gt:GraphType, options:RenderOptions):
    dst=options.dst
    iprefix="sprovider_impl"
    adapt_handler=lambda x: x


    ##############################################################
    ## Shared code.
    ##
    ## Currently shared code is tricky, as we don't have an easy
    ## mechanism to isolate handlers in plain C and OpenCL, so per-device shared code is
    ## hard. In principle this could be done using clCompile and clLink,
    ## but it adds a lot of complexity.
    ##
    ## For now we just dump all shared code out into the same translation
    ## unit. Any naming conflicts will have to be dealt with by the app writer.

    ## Another problem is the _ctxt argument, as we can't easily flow it
    ## into shared code.
    ## We will declare a global variable _ctxt which is NULL, and the provider
    ## needs to deal with calls to handler_assert and so on where the _ctxt might
    ## be null.

    dst.write("""
    ////////////////////////////
    // Graph shared code

    // This provides a _ctxt variable for when we aren't in a handler.
    // Ideally this would be a thread-local variable, but it is not
    // clear how to do this in a platform independent way. e.g. it could be:
    // - `__thread` for C
    // - `thread_local` for C++
    // - `__private` for OpenCL; well, except you can't have private globals...
    // - ? for Tinsel. Probably need a TEB like in the old softswitch
    void *_ctxt = 0;
    
    """)

    for sc in gt.shared_code:
        dst.write(adapt_handler(sc)+"\n")

    for dt in gt.device_types.values(): # type:DeviceType
        ##################################################################
        ## Begin device type

        dst.write(f"///////////////////////////////\n// Device {dt.id}\n\n")

        dst.write(f"////////////////////////////\n// Device {dt.id} shared code\n\n")
        dst.write(f"#define DEVICE_PROPERTIES_T {dt.id}_properties_t;\n")
        dst.write(f"#define DEVICE_STATE_T {dt.id}_state_t;\n\n")
        for sc in dt.shared_code:
            dst.write(sc+"\n")
        dst.write(f"\n#undef DEVICE_PROPERTIES_T\n")
        dst.write(f"#undef DEVICE_STATE_T\n\n")

        shared_prefix=f"""
        typedef {dt.id}_properties_t DEVICE_PROPERTIES_T;
        typedef {dt.id}_state_t DEVICE_STATE_T;
        const GRAPH_PROPERTIES_T *graphProperties=(const GRAPH_PROPERTIES_T*)_gpV;
        const DEVICE_PROPERTIES_T *deviceProperties=(const DEVICE_PROPERTIES_T*)_dpdsV;
        DEVICE_STATE_T *deviceState=(DEVICE_STATE_T*)( (((char*)_dpdsV) + ((sizeof(DEVICE_PROPERTIES_T)+3)&0xFFFFFFFCul)));
        """
        for (i,op) in enumerate(dt.outputs_by_index):
            shared_prefix+=f"const uint32_t RTS_FLAG_{op.name} = 1<<{i};\n"
            shared_prefix+=f"const uint32_t RTS_INDEX_{op.name} = {i};\n"

        if options.precise_activity_flags:
            active_flag_postfix=f"""
            {{
                uint32_t _rtsHidden=0;
                bool _readyToComputeHidden=false;
                uint32_t *readyToSend=&_rtsHidden;
                bool *requestCompute=&_readyToComputeHidden;
                ///////////////////////
                // RTS handler
                {adapt_handler(dt.ready_to_send_handler)}
                ///////////////////////
                return (_rtsHidden!=0) || _readyToComputeHidden;
            }}
            """
        else:
            active_flag_postfix=f"""
            return true; // Imprecise activity flags
            """

        dst.write(f"""
        static active_flag_t {iprefix}_calc_rts_{dt.id}(void *_ctxt, const void *_gpV, void *_dpdsV, uint32_t *readyToSend, bool *readyToCompute)
        {{
            {shared_prefix}
            //////////////////
            {adapt_handler(dt.ready_to_send_handler)}
            //////////////////
            return (*readyToSend!=0) || (*readyToCompute);
        }}
        
        static active_flag_t {iprefix}_do_init_{dt.id}(void *_ctxt, const void *_gpV, const void *_dpdsV)
        {{
            {shared_prefix}
            //////////////////
            {adapt_handler(dt.init_handler)}
            //////////////////
            {active_flag_postfix}
        }}

        static active_flag_t {iprefix}_do_hardware_idle_{dt.id}(void *_ctxt, const void *_gpV, const void *_dpdsV)
        {{
            {shared_prefix}
            //////////////////
            {adapt_handler(dt.on_hardware_idle_handler)}
            //////////////////
            {active_flag_postfix}
        }}

        static active_flag_t {iprefix}_do_device_idle_{dt.id}(void *_ctxt, const void *_gpV, const void *_dpdsV)
        {{
            {shared_prefix}
            //////////////////
            {adapt_handler(dt.on_device_idle_handler)}
            //////////////////
            {active_flag_postfix}
        }}  

        """)

        for ip in dt.inputs_by_index:
            dst.write(f"""
            static active_flag_t {iprefix}_do_recv_{dt.id}_{ip.name}(void *_ctxt, const void *_gpV, void *_dpdsV, void *_epesV, const void *_msgV)
            {{
                {shared_prefix}
                typedef {ip.message_type.id}_message_t MESSAGE_T;
                typedef {dt.id}_{ip.name}_properties_t EDGE_PROPERTIES_T;
                typedef {dt.id}_{ip.name}_state_t EDGE_STATE_T;
                const EDGE_PROPERTIES_T *edgeProperties=(const EDGE_PROPERTIES_T*)_epesV;
                EDGE_STATE_T *edgeState=((EDGE_STATE_T*)( ((char*)_epesV) + ((sizeof(EDGE_PROPERTIES_T)+3)&0xFFFFFFFCul)));
                const MESSAGE_T *message=(const MESSAGE_T*)_msgV;
                //////////////////
                {adapt_handler(ip.receive_handler)}
                //////////////////
                {active_flag_postfix}
            }}
            """)
        
        dst.write(f"""
        static active_flag_t {iprefix}_do_recv_{dt.id}(void *_ctxt, unsigned pin_index, const void *_gpV, void *_dpdsV, void *_epesV, const void *_msgV) {{
            {shared_prefix}
            switch(pin_index){{        
                default:  SPROVIDER_UNREACHABLE;
        """)
        for (i,ip) in enumerate(dt.inputs_by_index):
            dst.write(f"""  case {i}:
            {{
                typedef {ip.message_type.id}_message_t MESSAGE_T;
                typedef {dt.id}_{ip.name}_properties_t EDGE_PROPERTIES_T;
                typedef {dt.id}_{ip.name}_state_t EDGE_STATE_T;
                const EDGE_PROPERTIES_T *edgeProperties=(const EDGE_PROPERTIES_T*)_epesV;
                EDGE_STATE_T *edgeState=((EDGE_STATE_T*)( ((char*)_epesV) + ((sizeof(EDGE_PROPERTIES_T)+3)&0xFFFFFFFCul)));
                const MESSAGE_T *message=(const MESSAGE_T*)_msgV;
                ///////////////////////////////
                {adapt_handler(ip.receive_handler)}
                ///////////////////////////////
            }}
            break;
            """)
        dst.write(f"""
        }};
        /////////////////////
        {active_flag_postfix}
        }};

        """)
        

        for op in dt.outputs_by_index:
            dst.write(f"""
            static active_flag_t {iprefix}_do_send_{dt.id}_{op.name}(void *_ctxt, const void *_gpV, const void *_dpdsV, bool *doSend, int *sendIndex,  void *_msgV)
            {{
                {shared_prefix}
                typedef {op.message_type.id}_message_t MESSAGE_T;
                MESSAGE_T *message=(MESSAGE_T*)_msgV;
                //////////////////
                {adapt_handler(op.send_handler)}
                //////////////////
                {active_flag_postfix}
            }}
            """)

        dst.write(f"""
        static active_flag_t {iprefix}_do_send_{dt.id}(void *_ctxt, unsigned pin_index, const void *_gpV, const void *_dpdsV, bool *doSend, int *sendIndex, void *_msgV) {{
        {shared_prefix}
            switch(pin_index){{        
                default:  SPROVIDER_UNREACHABLE;
        """)
        for (i,op) in enumerate(dt.outputs_by_index):
            dst.write(f"""  case {i}:
            {{
                typedef {op.message_type.id}_message_t MESSAGE_T;
                MESSAGE_T *message=(MESSAGE_T*)_msgV;
                //////////////////
                {adapt_handler(op.send_handler)}
                //////////////////
            }}
            break;
            """)
        dst.write(f"""
            }}
            ///////////////////////
            {active_flag_postfix}
        }}
        """)

        dst.write(f"""
        static active_flag_t {iprefix}_try_send_or_compute_{dt.id}(void *_ctxt, const void *_gpV, const void *_dpdsV, int *_action_taken, int *_output_port, unsigned *_message_size, int *_send_index, void *_msgV) {{
            {shared_prefix}
            assert(*_action_taken == -2);
            assert(*_output_port < 0);
            uint32_t _readyToSend=0;
            bool _readyToCompute=false;
            bool _doSend=true;
            bool *doSend=&_doSend;
            {{
                uint32_t *readyToSend=&_readyToSend;
                bool requestCompute=&_readyToCompute;
                ////////////////////////////////////////
                {adapt_handler(dt.ready_to_send_handler)}
                ////////////////////////////////////////
            }}
            if(!_readyToSend){{
                if(!_readyToCompute){{
                    return false;
                }}
                *_action_taken = -1;
                //////////////////////////////////////////
                {adapt_handler(dt.on_device_idle_handler)}
                ////////////////////////////////////////////
            }}else{{
                unsigned _pin_index=__builtin_ctz(_readyToSend);
                switch(_pin_index){{        
                    default:  SPROVIDER_UNREACHABLE;
            """)
        for (i,op) in enumerate(dt.outputs_by_index):
            dst.write(f"""  case {i}:
            {{
                typedef {op.message_type.id}_message_t MESSAGE_T;
                MESSAGE_T *message=(MESSAGE_T*)_msgV;
                //////////////////
                {adapt_handler(op.send_handler)}
                //////////////////
                if(_doSend){{
                    *_output_port={i};
                    *_message_size=sizeof(MESSAGE_T);
                }}
            }}
            break;
            """)
        dst.write(f"""
            }}
            }}
            ///////////////////////
            {active_flag_postfix}
        }}
        """)

        ## Finished device type
        #####################################################

    #####################################################
    ## Now do the standard muxing entry points

    dst.write(f"""static active_flag_t sprovider_do_init(void *_ctxt, unsigned _device_type_index, const void *_gpV,  void *_dpdsV){{
       switch(_device_type_index){{
       default:  SPROVIDER_UNREACHABLE;
    """)
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"   case {i}: return {iprefix}_do_init_{dt.id}(_ctxt, _gpV, _dpdsV);\n")
    dst.write("}\n}\n\n")

    dst.write(f"""static active_flag_t sprovider_do_hardware_idle(void *_ctxt, unsigned _device_type_index, const void *_gpV, void *_dpdsV){{
       switch(_device_type_index){{
       default:  SPROVIDER_UNREACHABLE;
    """)
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"   case {i}: return {iprefix}_do_hardware_idle_{dt.id}(_ctxt, _gpV, _dpdsV);\n")
    dst.write("}\n}\n\n")

    dst.write(f"""static active_flag_t sprovider_do_device_idle(void *_ctxt, unsigned _device_type_index, const void *_gpV, void *_dpdsV){{
       switch(_device_type_index){{
       default:  SPROVIDER_UNREACHABLE;
    """)
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"   case {i}: return {iprefix}_do_device_idle_{dt.id}(_ctxt, _gpV, _dpdsV);\n")
    dst.write("}\n}\n\n")

    dst.write(f"""static active_flag_t sprovider_calc_rts(void *_ctxt, unsigned _device_type_index, const void *_gpV, void *_dpdsV, uint32_t *readyToSend, bool *requestCompute){{
       switch(_device_type_index){{
       default:  SPROVIDER_UNREACHABLE;
    """)
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"   case {i}: return {iprefix}_calc_rts_{dt.id}(_ctxt, _gpV, _dpdsV, readyToSend, requestCompute);\n")
    dst.write("}\n}\n\n")        

    dst.write(f"""static active_flag_t sprovider_do_send(void *_ctxt, unsigned _device_type_index, unsigned _pin_index, const void *_gpV, const void *_dpdsV, bool *_do_send, int *_send_index, void *_msgV){{
        switch(_device_type_index){{
        default: SPROVIDER_UNREACHABLE;
    """)
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"""    case {i}: return {iprefix}_do_send_{dt.id}(_ctxt, _pin_index, _gpV, _dpdsV, _do_send, _send_index, _msgV);\n""")
    dst.write("}\n}\n\n")

    dst.write(f"""static active_flag_t sprovider_try_send_or_compute(void *_ctxt, unsigned _device_type_index, const void *_gpV, const void *_dpdsV, int *_action_taken, int *_output_port, unsigned *_message_size, int *_send_index, void *_msgV){{
        switch(_device_type_index){{
        default: SPROVIDER_UNREACHABLE;
    """)
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"""    case {i}: return {iprefix}_try_send_or_compute_{dt.id}(_ctxt, _gpV, _dpdsV, _action_taken, _output_port, _message_size, _send_index, _msgV);\n""")
    dst.write("}\n}\n\n")

    dst.write(f"""static active_flag_t sprovider_do_recv(void *_ctxt, unsigned _device_type_index, unsigned _pin_index, const void *_gpV, void *_dpdsV, void *_epesV, const void *_msgV){{
        switch(_device_type_index){{
        default: SPROVIDER_UNREACHABLE;
        """)
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"    case {i}: return {iprefix}_do_recv_{dt.id}(_ctxt, _pin_index, _gpV, _dpdsV, _epesV, _msgV);\n")
    dst.write("}}\n\n")


def render_graph_type_as_sprovider(gt:GraphType, options:RenderOptions):
    dst=options.dst

    dst.write(f"""
    #ifndef sprovider_{gt.id}_hpp
    #define sprovider_{gt.id}_hpp

    #include "sprovider.h"
    """)

    render_graph_type_structs_as_sprovider(gt, options)
    render_graph_type_handlers_as_sprovider(gt, options)
    render_graph_type_info_as_sprovider(gt, options)

    dst.write("#endif\n\n")


def render_graph_type_info_as_sprovider(gt:GraphType, options:RenderOptions):
    dst=options.dst
    iprefix="sprovider_impl"
    adapt_handler=lambda x: x

    non_trivial_handler = lambda x: x is not None and x.strip()!=""

    has_any_hardware_idle=any(non_trivial_handler(dt.on_hardware_idle_handler) for dt in gt.device_types.values())
    has_any_device_idle=any(non_trivial_handler(dt.on_device_idle_handler) for dt in gt.device_types.values())
    has_any_indexed_send=any( any(op.is_indexed for op in dt.outputs_by_index) for dt in gt.device_types.values() )

    dst.write(f"""
    SPROVIDER_GLOBAL_CONST int SPROVIDER_DEVICE_TYPE_COUNT = {len(gt.device_types)};

    SPROVIDER_GLOBAL_CONST sprovider_graph_info_t SPROVIDER_GRAPH_TYPE_INFO = {{
        "{gt.id}",
        {1 if has_any_hardware_idle else 0},
        {1 if has_any_device_idle else 0},
        {1 if has_any_indexed_send else 0},
        sizeof(GRAPH_PROPERTIES_T),
        (sizeof(GRAPH_PROPERTIES_T)+3)&0xFFFFFFFCul
    }};

    SPROVIDER_GLOBAL_CONST sprovider_device_info_t SPROVIDER_DEVICE_TYPE_INFO[SPROVIDER_DEVICE_TYPE_COUNT] = {{
    """)
    
    for (i,dt) in enumerate(gt.device_types.values()):
        if i!=0:
            dst.write(",\n")
        dst.write(f"""
        {{
            "{dt.id}",
            {1 if dt.is_external else 0},
            {1 if non_trivial_handler(dt.on_hardware_idle_handler) else 0}, // has_hardware_idle. TODO: be precise
            {1 if non_trivial_handler(dt.on_device_idle_handler) else 0}, // has_device_idle. TODO: be precise
            sizeof({gt.id}_{dt.id}_properties_t),
            (sizeof({gt.id}_{dt.id}_properties_t)+3)&0xFFFFFFFCul,
            sizeof({gt.id}_{dt.id}_state_t),
            (sizeof({gt.id}_{dt.id}_state_t)+3)&0xFFFFFFFCul,
            ((sizeof({gt.id}_{dt.id}_properties_t)+3)&0xFFFFFFFCul) + ((sizeof({gt.id}_{dt.id}_state_t)+3)&0xFFFFFFFCul),
            0,0,
            {{
        """)

        for (j,ip) in enumerate(dt.inputs_by_index):
            mt=ip.message_type
            if j!=0:
                dst.write(",\n")
            dst.write(f"""
            {{
                "{ip.name}",
                {j},
                sizeof({mt.id}_message_t),
                ((sizeof({mt.id}_message_t)+3)&0xFFFFFFFCul),
                sizeof({dt.id}_{ip.name}_properties_t),
                ((sizeof({dt.id}_{ip.name}_properties_t)+3)&0xFFFFFFFCul),
                sizeof({dt.id}_{ip.name}_state_t),
                ((sizeof({dt.id}_{ip.name}_state_t)+3)&0xFFFFFFFCul),
                ((sizeof({dt.id}_{ip.name}_properties_t)+3)&0xFFFFFFFCul) + ((sizeof({dt.id}_{ip.name}_state_t)+3)&0xFFFFFFFCul)
            }}
            """)

        dst.write(f"}},{{")

        for (j,op) in enumerate(dt.outputs_by_index):
            mt=op.message_type
            if j!=0:
                dst.write(",\n")
            dst.write(f"""
            {{
                "{op.name}",
                {j},
                sizeof({mt.id}_message_t),
                ((sizeof({mt.id}_message_t)+3)&0xFFFFFFFCul),
                {1 if op.is_indexed else 0}              
            }}
            """)


        dst.write(f"""
        }}
        }}
        """)
    dst.write(f"""
    }};
    """)

    max_payload_size=0
    for mt in gt.message_types.values():
        if mt.message:
            max_payload_size=max(max_payload_size, mt.message.size_in_bytes())
    dst.write(f"""
    SPROVIDER_GLOBAL_CONST int SPROVIDER_MAX_PAYLOAD_SIZE = {max_payload_size};
    """)

if __name__=="__main__":
    options=RenderOptions()

    path=sys.argv[1]
    gt=load_graph_type(path,path)

    render_graph_type_as_sprovider(gt, options)
