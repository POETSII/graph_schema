#!/usr/bin/env python3

import io
import sys
import os
import re
from typing import *

from graph.core import GraphType, DeviceType, OutputPin, InputPin, MessageType, GraphInstance, DeviceInstance, EdgeInstance
from graph.core import TypedDataSpec, TupleTypedDataSpec, ScalarTypedDataSpec, ArrayTypedDataSpec
from graph.load_xml import load_graph_instance



_opencl_scalar_type_map={
    "int8_t":"char",
    "uint8_t":"uchar",
    "int16_t":"short",
    "uint16_t":"ushort",
    "int32_t":"int",
    "uint32_t":"uint",
    "int64_t":"long",
    "uint64_t":"ulong",
    "float":"float",
    "double":"double"
}



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

def adapt_handler(handler:str) -> str:
    return rewrite_handler_log_instances(handler)

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
        alt_type=_opencl_scalar_type_map[dt.type]
        return f"{alt_type} {name}"
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

def convert_json_init_to_c_init(t,v):
    if isinstance(v,(int,float)):
        assert isinstance(t,ScalarTypedDataSpec)
        return str(v)
    elif isinstance(v,bool):
        assert isinstance(t,ScalarTypedDataSpec)
        return str("1" if v else "0" )
    elif isinstance(v,dict):
        assert isinstance(t,TupleTypedDataSpec)

        return "{"+ ",".join([convert_json_init_to_c_init(e, v[e.name]) for e in t.elements_by_index ]) +"}"
    elif isinstance(v,list):
        assert isinstance(t,ArrayTypedDataSpec)
        return "{"+ ",".join([convert_json_init_to_c_init(t.type, e) for e in v ]) +"}"
    else:
        raise RuntimeError("Unknown json element type")

def typed_struct_instance_as_init(type,inst):
    if type is not None:
        assert type.is_refinement_compatible(inst)
        inst=type.expand(inst)
        if inst != {}:
            return convert_json_init_to_c_init(type, inst)
        else:
            return "{}"
    else:
        assert inst is None
        return "{}"


def render_graph_type_as_opencl(gt:GraphType, dst:io.TextIOBase):
    ##############################################################
    ## Patch in stdint.h types

    dst.write("""
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long int64_t;
typedef unsigned long uint64_t;


""")


    ###############################################################
    ## Do all the structs

    render_typed_data_spec_as_struct(gt.properties,  f"{gt.id}_properties_t", dst)
    for mt in gt.message_types.values(): # type:MessageType
        render_typed_data_spec_as_struct( mt.message, f"{mt.id}_message_t", dst)
    
    for dt in gt.device_types.values(): # type:DeviceType
        render_typed_data_spec_as_struct( dt.properties, f"{gt.id}_{dt.id}_properties_t", dst)
        render_typed_data_spec_as_struct( dt.state, f"{gt.id}_{dt.id}_state_t", dst)
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

    ##############################################################
    ## Shared code.
    ##
    ## Currently shared code is tricky, as we don't have an easy
    ## mechanism to isolate handlers, so per-device shared code is
    ## hard. In principle this could be done using clCompile and clLink,
    ## but it adds a lot of complexity. It could also be done using
    ## macros, which probably makes more sense but is more than I
    ## can be bothered with now
    ##
    ## For now we just dump all shared code out into the same translation
    ## unit. Any naming conflicts will have to be dealt with by the app writer.

    dst.write("////////////////////////////\n// Graph shared code\n\n")
    for sc in gt.shared_code:
        dst.write(adapt_handler(sc)+"\n")
    for dt in gt.device_types.values(): # type:DeviceType
        dst.write(f"////////////////////////////\n// Device {dt.id} shared code\n\n")
        for sc in dt.shared_code:
            dst.write(adapt_handler(sc)+"\n")

    #################################################################
    ## Then the handlers
    ##
    ## We'll emit per device functions in order to deal with things like
    ## per device scope

    for dt in gt.device_types.values(): # type:DeviceType
        shared_prefix=f"""
        typedef {gt.id}_{dt.id}_properties_t {dt.id}_properties_t;
        typedef {gt.id}_{dt.id}_state_t {dt.id}_state_t;
        typedef {dt.id}_properties_t DEVICE_PROPERTIES_T;
        typedef {dt.id}_state_t DEVICE_STATE_T;
        const GRAPH_PROPERTIES_T *graphProperties=(const GRAPH_PROPERTIES_T*)_gpV;
        const DEVICE_PROPERTIES_T *deviceProperties=(const DEVICE_PROPERTIES_T*)_dpV;
        """
        for (i,op) in enumerate(dt.outputs_by_index):
            shared_prefix+=f"const uint RTS_FLAG_{op.name} = 1<<{i};"
            shared_prefix+=f"const uint RTS_INDEX_{op.name} = {i};"

        dst.write(f"""
        void calc_rts_{dt.id}(uint _dev_address_, const void *_gpV, const void *_dpV, const void *_dsV, uint *readyToSend)
        {{
            {shared_prefix}
            const DEVICE_STATE_T *deviceState=(const DEVICE_STATE_T*)_dsV;
            //////////////////
            {adapt_handler(dt.ready_to_send_handler)}
        }}
        """)

        dst.write(f"""
        void do_init_{dt.id}(uint _dev_address_, const void *_gpV, const void *_dpV, void *_dsV)
        {{
            {shared_prefix}
            DEVICE_STATE_T *deviceState=(DEVICE_STATE_T*)_dsV;
            //////////////////
            {adapt_handler(dt.init_handler)}
        }}
        """)

        assert dt.on_hardware_idle_handler==None or dt.on_hardware_idle_handler.strip()=="", "Hardware idle not supported yet"
        assert dt.on_device_idle_handler==None or dt.on_device_idle_handler.strip()=="", "Device idle not supported yet"

        for ip in dt.inputs_by_index:
            dst.write(f"""
            void do_recv_{dt.id}_{ip.name}(uint _dev_address_, const void *_gpV, const void *_dpV, void *_dsV, const void *_epV, void *_esV, const void *_msgV)
            {{
                {shared_prefix}
                DEVICE_STATE_T *deviceState=(DEVICE_STATE_T*)_dsV;
                typedef {ip.message_type.id}_message_t MESSAGE_T;
                typedef {dt.id}_{ip.name}_properties_t EDGE_PROPERTIES_T;
                typedef {dt.id}_{ip.name}_state_t EDGE_STATE_T;
                const EDGE_PROPERTIES_T *edgeProperties=(const EDGE_PROPERTIES_T*)_epV;
                EDGE_STATE_T *edgeState=(EDGE_STATE_T*)_esV;
                const MESSAGE_T *message=(const MESSAGE_T*)_msgV;
                //////////////////
                {adapt_handler(ip.receive_handler)}
            }}
            """)
        
        dst.write(f"""
        void do_recv_{dt.id}(uint _dev_address_, uint pin_index, const void *_gpV, const void *_dpV, void *_dsV, const void *_epV, void *_esV, const void *_msgV) {{
        """)
        for (i,ip) in enumerate(dt.inputs_by_index):
            dst.write(f"  if(pin_index=={i}){{  do_recv_{dt.id}_{ip.name}(_dev_address_, _gpV, _dpV, _dsV, _epV, _esV, _msgV); }} else"+"\n")
        dst.write("  { assert(0); } ")
        dst.write("}\n")

        for op in dt.outputs_by_index:
            dst.write(f"""
            void do_send_{dt.id}_{op.name}(uint _dev_address_, const void *_gpV, const void *_dpV, void *_dsV, int *sendIndex, int *doSend,  void *_msgV)
            {{
                {shared_prefix}
                DEVICE_STATE_T *deviceState=(DEVICE_STATE_T*)_dsV;
                typedef {op.message_type.id}_message_t MESSAGE_T;
                MESSAGE_T *message=(MESSAGE_T*)_msgV;
                //////////////////
                {adapt_handler(op.send_handler)}
            }}
            """)

        dst.write(f"""
        void do_send_{dt.id}(uint device_address, uint pin_index, const void *_gpV, const void *_dpV, void *_dsV, int *sendIndex, int *doSend, void *_msgV) {{
        """)

        for (i,ip) in enumerate(dt.outputs_by_index):
            dst.write(f"  if(pin_index=={i}){{  do_send_{dt.id}_{ip.name}(device_address, _gpV, _dpV, _dsV, sendIndex, doSend, _msgV); }} else"+"\n")
        dst.write("  { assert(0); }\n")
        dst.write("}\n")

    dst.write("void do_init(uint device_address, uint device_type_index, const void *_gpV, const void *_dpV, void *_dsV){\n")
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"    if(device_type_index=={i}){{ do_init_{dt.id}(device_address, _gpV, _dpV, _dsV); }}")
    dst.write("  { assert(0); }\n")
    dst.write("}\n\n")

    dst.write("void calc_rts(uint device_address, uint device_type_index, const void *_gpV, const void *_dpV, void *_dsV, uint *readyToSend){\n")
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"    if(device_type_index=={i}){{ calc_rts_{dt.id}(device_address, _gpV, _dpV, _dsV, readyToSend); }}")
    dst.write("  { assert(0); }\n")
    dst.write("}\n\n")

    dst.write("void do_send(uint device_address, uint device_type_index, uint pin_index, const void *_gpV, const void *_dpV, void *_dsV, int *sendIndex, int *doSend, void *_msgV){\n")
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"    if(device_type_index=={i}){{ do_send_{dt.id}(device_address, pin_index, _gpV, _dpV, _dsV, sendIndex, doSend, _msgV); }}")
    dst.write("  { assert(0); }\n")
    dst.write("}\n\n")

    dst.write("void do_recv(uint device_address, uint device_type_index, uint pin_index, const void *_gpV, const void *_dpV, void *_dsV, const void *_epV, void *_esV, const void *_msgV){\n")
    for (i,dt) in enumerate(gt.device_types.values()):
        dst.write(f"    if(device_type_index=={i}){{ do_recv_{dt.id}(device_address, pin_index, _gpV, _dpV, _dsV, _epV, _esV, _msgV); }}")
    dst.write("  { assert(0); }\n")
    dst.write("}\n\n")


def render_graph_instance_as_opencl(gi:GraphInstance, dst:io.TextIOBase):
    gt=gi.graph_type

    #################################################################
    ## Initial hacking to fix topology params
    device_types_by_index=list(gt.device_types.values())
    devs=list(gi.device_instances.values())
    for (i,d) in enumerate(devs):
        # Naughty: hacking in the address
        d.address=i
        d.device_type_index=device_types_by_index.index(d.device_type)
    
    incoming_edges={} # Map of dst_address -> [ edge_instance ]
    outgoing_edges={} # Map of (src_address,src_pin) -> [ edge_instance ]

    for ei in gi.edge_instances.values(): # type: EdgeInstance
        incoming=incoming_edges.setdefault(ei.dst_device.id, [])
        outgoing=outgoing_edges.setdefault( (ei.src_device.id,ei.src_pin.name) , [])
        incoming_index=len(incoming)
        ei.incoming_index = incoming_index # Naughty: patch it in
        incoming.append( ei )
        outgoing.append( ei )

    #####################################################################
    # Write out all the landing zones
    for d in devs:  # type: DeviceType
        dst.write(f"""
            message_payload_t {d.id}__payloads[{len(d.device_type.outputs_by_index)}];
            POETS_ATOMIC_INT {d.id}__ref_counts[{len(d.device_type.outputs_by_index)}] = {{ { ",".join(["POETS_ATOMIC_VAR_INIT(0)" for i in range(len(d.device_type.outputs_by_index))]) } }};     

            """)    


    ######################################################################
    # Do incoming edges
    for d in devs:
        incoming=incoming_edges.get(d.id, [])

        ## First handle any properties/state

        for ei in incoming:
            if ei.properties!=None:
                dst.write(f"""
                    const {ei.dst_device.device_type.id}_{ei.dst_pin.name}_properties_t {ei.dst_device.id}_{ei.dst_pin.name}_{ei.incoming_index}__properties = 
                        { typed_struct_instance_as_init(ei.dst_pin.properties,ei.properties) };
                    """)
            if ei.state!=None:
                dst.write(f"""
                    const {ei.dst_device.device_type.id}_{ei.dst_pin.name}_state_t {ei.dst_device.id}_{ei.dst_pin.name}_{ei.incoming_index}__state = 
                        { typed_struct_instance_as_init(ei.dst_pin.state,ei.state) };
                    """)
                

        ## Then deal with hookups into graph

        make_incoming_entry=lambda ei: f"""
            {{
                {ei.dst_pin.index}, //uint pin_index;
                {ei.src_device.id}__payloads + {ei.src_pin.index}, //message_payload_t *payload;
                {ei.src_device.id}__ref_counts + {ei.src_pin.index}, //uint *ref_count;
                {"0" if ei.properties==None else f"&{ei.dst_device.id}_{ei.dst_pin.name}_{ei.incoming_index}__properties" }, //const void *edge_properties;
                {"0" if ei.state==None else f"&{ei.dst_device.id}_{ei.dst_pin.name}_{ei.incoming_index}__state" } //void *edge_state;
            }}
            """

        dst.write(f"""
            incoming_edge {d.id}__incoming_edges[{len(incoming)}] =
            {{
                {
                    ",".join([ make_incoming_entry(ei) for ei in incoming ])
                }
            }};
            POETS_ATOMIC_INT {d.id}__incoming_landing_bits[{len(incoming)}] = {{ 0 }};
            """)
        incoming=None

    ####################################################################
    # do outgoing edges and ports
    for d in devs:
        # Each outgoing edge identifies a bit to set in the target bit mask
        make_outgoing_entry=lambda ei: f"""{{ {ei.dst_device.id}__incoming_landing_bits, {ei.incoming_index} }}\n"""
        
        for op in d.device_type.outputs_by_index:
            outgoing=outgoing_edges.get( (d.id,op.name), [] )
            dst.write(f"""
                outgoing_edge {d.id}_p{op.index}__outgoing_edges[] = {{
                    {",".join(make_outgoing_entry(ei) for ei in outgoing)}
                }};
                """)
            outgoing=None

        make_outgoing_port=lambda op: f"""
        {{
            {len( outgoing_edges.get( (d.id,op.name), []) ) }, //unsigned num_outgoing_edges;
            {d.id}_p{op.index}__outgoing_edges, //outgoing_edge *outgoing_edges;
            {d.id}__payloads+{op.index}, //message_payload_t *payload;
        }}
        """

        dst.write(f"""
output_pin {d.id}__output_pins[{len(d.device_type.outputs)}] =
{{
    {",".join(make_outgoing_port(op) for op in d.device_type.outputs_by_index)}
}};
        """)

    ##################################################################################
    ## Properties and state
    for d in devs:
        dst.write(f"{gt.id}_{d.device_type.id}_properties_t {d.id}__properties={ typed_struct_instance_as_init(d.device_type.properties, d.properties) };\n")
        dst.write(f"{gt.id}_{d.device_type.id}_state_t {d.id}__state={ typed_struct_instance_as_init(d.device_type.state, d.state) };\n")

    #####################################################################################
    ## Device info

    dst.write("__global device_info devices[]={\n")
    for (i,d) in enumerate(devs):
        if i!=0:
            dst.write(",\n")
        dst.write(f"""
    {{
        {d.address}, // address
        {len( incoming_edges.get(d.id,[]) ) }, //uint num_incoming_edges;
        {d.id}__incoming_edges, //incoming_edge *incoming_edges;  // One entry per incoming edge
        {d.id}__incoming_landing_bits, //uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

        {len(d.device_type.outputs)}, //unsigned num_output_pins;
        {d.id}__output_pins, //const output_pin *output_pins;  // One entry per pin
        {d.id}__ref_counts, //uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

        {d.device_type_index}, //unsigned device_type_index;
        &{d.id}__properties, //const void *device_properties;
        &{d.id}__state //void *device_state;
    }}    
""")
    dst.write("};\n")

    dst.write(f"""
    {gt.id}_properties_t G_properties={typed_struct_instance_as_init(gi.graph_type.properties, gi.properties)};

    __kernel void kinit()
{{
    init(&G_properties, devices);
}}

__kernel void kstep(unsigned count)
{{
    for(unsigned i=0; i<count;i++){{
        step(&G_properties, devices);
    }}
}}

const uint __TOTAL_DEVICES__={len(devs)};
""")



if __name__=="__main__":
    path=sys.argv[1]
    gi=load_graph_instance(path,path)
    gt=gi.graph_type

    if False:
        # Useful for debugging using normal C++ parser. Must not be used when generating true cl code
        sys.stdout.write("""
        typedef uint atomic_uint;

        #define ATOMIC_VAR_INIT(x) x
        #define __global 
        #define __kernel
        """)

    sys.stdout.write('#include "kernel_softswitch.cl"\n\n')

    render_graph_type_as_opencl(gt, sys.stdout)

    render_graph_instance_as_opencl(gi, sys.stdout)
