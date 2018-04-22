#!/usr/bin/env python3


from graph.load_xml import load_graph_types_and_instances
from graph.write_cpp import render_graph_as_cpp

from graph.make_properties import *
from graph.calc_c_globals import *

import struct
import sys
import os
import logging
import random
import math
import statistics


def print_statistics(dst, baseName, unit, data):
    print("{}Count, -, {}, {}\n".format(baseName, len(data), unit), file=measure_file)
    print("{}Min, -, {}, {}\n".format(baseName, min(data), unit), file=measure_file)
    print("{}Max, -, {}, {}\n".format(baseName, max(data), unit), file=measure_file)

    if len(data)>0:
        print("{}Median, -, {}, {}\n".format(baseName, statistics.median(data), unit), file=measure_file)
        mean=statistics.mean(data)
        print("{}Mean, -, {}, {}\n".format(baseName, mean, unit), file=measure_file)

        if len(data)>1:
            stddev=statistics.stdev(data)
            skewness=sum( [math.pow(float(x),3) for x in data] ) / pow(stddev,3)
            print("{}StdDev, -, {}, {}\n".format(baseName, stddev, unit), file=measure_file)
            print("{}Skewness, -, {}, {}\n".format(baseName, skewness, unit), file=measure_file)


# The alignment to pad to for the next cache line
_cache_line_size=32

# This compacts properties and state down into BLOBs, and
# makes things faster to compile.
_use_BLOB=True

# TODO: This seems to be unneeded now, as static initialiser code has
# been removed by other methods. Remove?
_use_indirect_BLOB=False and _use_BLOB

# If 0, then don't sort edges. If 1, then sort by furthest first. If -1, then sort by closest first
sort_edges_by_distance=0

threads_per_core=16
cores_per_mailbox=4
mailboxes_per_board=16

threads_per_mailbox=threads_per_core*cores_per_mailbox

inter_mbox_cost=5
intra_mbox_cost=1

measure_file=None

def decompose_thread(id):
    return (id//threads_per_mailbox, (id%threads_per_mailbox)//threads_per_core, id%threads_per_core)

def thread_distance(dst,src):
    (srcMbox,srcCore,srcThread)=decompose_thread(src)
    (dstMbox,dstCore,dstThread)=decompose_thread(dst)

    if srcMbox!=dstMbox:
        return abs( srcMbox-dstMbox ) * inter_mbox_cost
    if srcCore!=dstCore:
        return intra_mbox_cost
    return 0


def render_typed_data_as_type_decl(td,dst,indent,name=None):
    if name==None:
        name=td.name
    assert(name!="_")
    if isinstance(td,ScalarTypedDataSpec):
        dst.write("{0}{1} {2} /*indent={0},type={1},name={2}*/".format(indent, td.type, name))
    elif isinstance(td,TupleTypedDataSpec):
        dst.write("{}struct {{\n".format(indent))
        for e in td.elements_by_index:
            render_typed_data_as_type_decl(e,dst,indent+"  ")
            dst.write(";\n")
        dst.write("{}}} {}".format(indent,name))
    elif isinstance(td,ArrayTypedDataSpec):
        render_typed_data_as_type_decl(td.type,dst,indent,name="")
        dst.write("{}[{}] /*{}*/".format(name, td.length,name))
    else:
        raise RuntimeError("Unknown data type {}".format(td));

def render_typed_data_as_struct(td,dst,name):
    if td:
        dst.write("typedef \n")
        render_typed_data_as_type_decl(td,dst,"  ",name=name)
        dst.write(";\n")
    else:
        dst.write("typedef struct {{}} {};\n".format(name))

def render_typed_data_inst_contents(td,ti,dst):
    if isinstance(td,ScalarTypedDataSpec):
        if ti is None:
            dst.write(str(td.default))
        else:
            dst.write("{}".format(ti))
    elif isinstance(td,TupleTypedDataSpec):
        dst.write("{")
        first=True
        for e in td.elements_by_index:
            if first:
                first=False
            else:
                dst.write(",")
            render_typed_data_inst_contents(e,ti and ti.get(e.name,None),dst)
        dst.write("}")
    elif isinstance(td,ArrayTypedDataSpec):
        dst.write("{")
        first=True
        for i in range(td.length):
            if i!=0:
                dst.write(",")
            render_typed_data_inst_contents(td.type,ti and ti[i],dst)
        dst.write("}")
    else:
        raise RuntimeError("Unknkown type {}".format(td))

def render_typed_data_inst(td,tName,ti,iName,dst,const=False):
    if const:
        dst.write("const ")
    dst.write("{} {} = ".format(tName,iName))
    if td:
        render_typed_data_inst_contents(td,ti,dst)
    else:
        dst.write("{}")
    dst.write(";\n")

_scalar_to_struct_format={
    "float":struct.Struct("<f"),
    "double":struct.Struct("<d"),
    "uint8_t":struct.Struct("<B"),
    "int8_t":struct.Struct("<b"),
    "uint16_t":struct.Struct("<H"),
    "int16_t":struct.Struct("<h"),
    "uint32_t":struct.Struct("<I"),
    "int32_t":struct.Struct("<i"),
    "uint64_t":struct.Struct("<Q"),
    "int64_t":struct.Struct("<q"),
    "char":struct.Struct("<c")
}

# TODO : this is probably very expensive
def scalar_to_bytes(val,type):
    return _scalar_to_struct_format[type].pack(val)
    

def convert_typed_data_inst_to_bytes(td,ti):
    if td==None:
        return bytearray([])
    if isinstance(td,ScalarTypedDataSpec):
        if ti is None:
            v=td.default
        else:
            v=ti
        return scalar_to_bytes(v, td.type)
    elif isinstance(td,TupleTypedDataSpec):
        res=bytearray([])
        for e in td.elements_by_index:
            res.extend(convert_typed_data_inst_to_bytes(e,ti and ti.get(e.name,None)))
        return res
    elif isinstance(td,ArrayTypedDataSpec):
        res=bytearray([])
        for i in range(td.length):
            res.extend(convert_typed_data_inst_to_bytes(td.type,ti and ti[i]) )
        return res
    else:
        raise RuntimeError("Unknown type {}".format(td))

def get_typed_data_size(td):
    if td is None:
        return 0
    return len(convert_typed_data_inst_to_bytes(td,None))
 
_address_to_bytes_format=struct.Struct("<IHBB")
 
def convert_address_to_bytes(address,device,pin):
    return _address_to_bytes_format.pack(address,device,pin,0)
    
class StructHolder:    
    """Builds a giant list of structs containing sub-structs."""
    
    def __init__(self,typename,arity):
        self.typename=typename
        self.arity=arity
        self.entries=[] # Vector of (payload,name)
        self.indices={} # Map of name : index
    
    # Add a label without adding any entries
    def add_label(self,name):
        assert name not in self.indices
        index=len(self.entries)
        self.indices[name]=index
        return index
    
    # Returns the offset
    def add(self,name, payload):
        assert isinstance(payload,tuple)
        assert len(payload)==self.arity
        assert name not in self.indices
        index=len(self.entries)
        self.entries.append( (payload,name) )
        self.indices[name]=index
        return index
    
    # Returns an offset
    def find(self,name):
        if name not in self.indices:
            for k in self.indices.keys():
                sys.stderr.write("Key = {}\n".format(k))
            sys.stderr.write("Looking for {}\n".format(name))
        assert name in self.indices
        return self.indices[name]
        
    # Write the array out as an array called name
    def write(self,dst,name):
        dst.write("{} {}[] ={{\n".format(self.typename, name));
        i=0
        for (payload,name) in self.entries:
            dst.write("  // {}, {}\n".format(i, name))
            dst.write("  {")
            dst.write(",".join([str(x) for x in payload]))
            dst.write("}")
            if i<len(self.entries)-1:
                dst.write(",")
            dst.write("\n")
            i=i+1
        dst.write("};\n\n")
    
class BLOBHolder:
    """Builds a BLOB containing small BLOBs. Each sub-BLOB can be aligned
    within the master BLOB, by default to a multiple of 4.
    The overall BLOB should be aligned to at least a cache line.
    """
    
    def __init__(self):
        self.bytes=bytearray([])
        self.offsets={} # Map of name : (offset,size)
        self.segments=[] # List of (offset,size,name)
    
    def pad(self,granularity):
        over = len(self.bytes) % granularity
        if over>0:
            offset=len(self.bytes)
            length=granularity-over
            self.bytes.extend( [0]*length )
            self.segments.append( (offset,length,"__pad__") )
    
    # Returns the offset
    def add(self,name, payload, align=4):
        self.pad(align) # Always align to 32-bit boundary
        assert isinstance(payload,bytearray)
        assert name not in self.offsets
        offset=len(self.bytes)
        length=len(payload)
        self.bytes.extend(payload)
        self.offsets[name]=(offset,length)
        self.segments.append( (offset,length,name) )
        return offset
            
    # Returns an (offset,length) pair
    def find(self,name):
        if name not in self.offsets:
            for k in self.offsets.keys():
                sys.stderr.write("Key = {}\n".format(k))
            sys.stderr.write("Looking for {}\n".format(name))
        assert name in self.offsets
        return self.offsets[name]
        
    # Write the array out as an array of uint8_t called name
    def write(self,dst,name):
        dst.write("uint8_t {}[]  __attribute__((aligned({}))) ={{\n".format(name, _cache_line_size));
        for (offset,length,name) in self.segments:
            dst.write("  // {} +{}, {}\n".format(offset, length, name))
            for b in self.bytes[offset:offset+length]:
                dst.write("{},".format(b))
            dst.write("\n")
        dst.write("  // trailing byte\n")
        dst.write("  0\n")
        dst.write("};\n\n")

#enumerate all message types in the graph and assign a unique numerical ID to the message type
def assign_unique_ident_for_each_message_type(gt):
    numid = 0
    for mt in gt.message_types.values():
        mt.numid = numid
        numid=numid+1

# creates a list of message types and a unique id that is consistent between the threads and the
# executive. This can then be used to cast the host message at the executive end so that the
# message matches the structure defined in the xml.
# TODO: we only need these for the application pins, currently it dumps all message types
def output_unique_ident_for_each_message_type(gt, dst):
    for mt in gt.message_types.values():
        dst.write("""{},{}\n""".format(mt.id, mt.numid)) 

# creates a list of the device_instance application pins and their corrosponding address in the 
# POETS system (sf306: essentially I believe this is acting as a temporary nameserver for the executive)
def output_device_instance_addresses(gi,device_to_thread, thread_to_devices, dst): 
    # format deviceName_inAppPinName, thread addr, dev addr, pinIndex
    for d in gi.device_instances.values(): # iterate through all devices
        deviceName = d.id
        threadId = device_to_thread[d.id]
        for ip in d.device_type.inputs.values():
            if ip.is_application == True: #application input pin, so we print the addr 
                deviceOffset = thread_to_devices[threadId].index(d.id)
                pinIndex = ip.parent.inputs_by_index.index(ip)
                dst.write("""{}_{},{},{},{}\n""".format(deviceName, ip.name,threadId,deviceOffset, pinIndex)) 

def render_graph_type_as_softswitch_decls(gt,dst):
    gtProps=make_graph_type_properties(gt)
    dst.write("""
    #ifndef {GRAPH_TYPE_ID}_hpp
    #define {GRAPH_TYPE_ID}_hpp
    
    #include <stdint.h>
    
    #include "softswitch.hpp"
    #include "softswitch_hostmessaging.hpp"
    
    #ifdef __cplusplus
    extern "C"{{
    #endif
    
    #define handler_log softswitch_handler_log  
    #define handler_exit softswitch_handler_exit
    #define handler_export_key_value softswitch_handler_export_key_value
    
    
    ///////////////////////////////////////////////
    // Graph type stuff

    const unsigned DEVICE_TYPE_COUNT = {GRAPH_TYPE_DEVICE_TYPE_COUNT};
    enum{{ DEVICE_TYPE_COUNT_V = {GRAPH_TYPE_DEVICE_TYPE_COUNT} }};
    """.format(**gtProps))
    
    render_typed_data_as_struct(gt.properties,dst,gtProps["GRAPH_TYPE_PROPERTIES_T"])
    
    for mt in gt.message_types.values():
        mtProps=make_message_type_properties(mt)
        render_typed_data_as_struct(mt.message, dst,mtProps["MESSAGE_TYPE_T"])
    
    for dt in gt.device_types.values():
        dtProps=make_device_type_properties(dt)
        
        dst.write("""
        const unsigned DEVICE_TYPE_INDEX_{DEVICE_TYPE_FULL_ID} = {DEVICE_TYPE_INDEX};
        enum{{ DEVICE_TYPE_INDEX_{DEVICE_TYPE_FULL_ID}_V = {DEVICE_TYPE_INDEX} }};
        const unsigned INPUT_COUNT_{DEVICE_TYPE_FULL_ID} = {DEVICE_TYPE_INPUT_COUNT};
        enum{{ INPUT_COUNT_{DEVICE_TYPE_FULL_ID}_V = {DEVICE_TYPE_INPUT_COUNT} }};
        const unsigned OUTPUT_COUNT_{DEVICE_TYPE_FULL_ID} = {DEVICE_TYPE_OUTPUT_COUNT};
        enum{{ OUTPUT_COUNT_{DEVICE_TYPE_FULL_ID}_V = {DEVICE_TYPE_OUTPUT_COUNT} }};
        """.format(**dtProps))
        
        render_typed_data_as_struct(dt.properties,dst,dtProps["DEVICE_TYPE_PROPERTIES_T"])
        render_typed_data_as_struct(dt.state,dst,dtProps["DEVICE_TYPE_STATE_T"])
        
        for ip in dt.inputs_by_index:
            ipProps=make_input_pin_properties(ip)
            dst.write("""
            const unsigned INPUT_INDEX_{INPUT_PORT_FULL_ID} = {INPUT_PORT_INDEX};
            enum{{ INPUT_INDEX_{INPUT_PORT_FULL_ID}_V = {INPUT_PORT_INDEX} }};
            """.format(**ipProps))
            
            render_typed_data_as_struct(ip.properties, dst, ipProps["INPUT_PORT_PROPERTIES_T"])
            render_typed_data_as_struct(ip.state, dst, ipProps["INPUT_PORT_STATE_T"])
            
        for op in dt.outputs_by_index:
            dst.write("""
            const unsigned OUTPUT_INDEX_{OUTPUT_PORT_FULL_ID} = {OUTPUT_PORT_INDEX};
            enum {{ OUTPUT_INDEX_{OUTPUT_PORT_FULL_ID}_V = {OUTPUT_PORT_INDEX} }};
            const uint32_t OUTPUT_FLAG_{OUTPUT_PORT_FULL_ID} = 1<<{OUTPUT_PORT_INDEX};
            """.format(**make_output_pin_properties(op)))
    
    dst.write("extern const DeviceTypeVTable softswitch_device_vtables[];\n")
    dst.write("""
    #ifdef __cplusplus
    };
    #endif
    """)
    dst.write("#endif\n")

def render_rts_handler_as_softswitch(dev,dst,devProps):
    devProps=devProps or make_device_type_properties(dev)
    devProps=add_props(devProps, {
        "DEVICE_TYPE_C_LOCAL_CONSTANTS" : calc_device_type_c_locals(dev,devProps)
    })
    
    dst.write("""
    uint32_t {GRAPH_TYPE_ID}_{DEVICE_TYPE_ID}_ready_to_send_handler(
        const {GRAPH_TYPE_PROPERTIES_T} *graphProperties,
        const {DEVICE_TYPE_PROPERTIES_T} *deviceProperties,
        const {DEVICE_TYPE_STATE_T} *deviceState
    ){{
      {DEVICE_TYPE_C_LOCAL_CONSTANTS}
    
      uint32_t rts=0;
      uint32_t *readyToSend=&rts;
      // Begin device code
      {DEVICE_TYPE_RTS_HANDLER_SOURCE_LOCATION}
      {DEVICE_TYPE_RTS_HANDLER}
      __POETS_REVERT_PREPROC_DETOUR__
      // End device code
      return rts;
    }}
    """.format(**devProps))


def render_receive_handler_as_softswitch(pin, dst, pinProps):
    pinProps = pinProps or make_input_pin_properties(pinProps)
    pinProps = add_props(pinProps, {
        "DEVICE_TYPE_C_LOCAL_CONSTANTS" : calc_device_type_c_locals(pin.parent,pinProps)
    })
    dst.write("""
    void {INPUT_PORT_FULL_ID}_receive_handler(
        const {GRAPH_TYPE_PROPERTIES_T} *graphProperties,
        const {DEVICE_TYPE_PROPERTIES_T} *deviceProperties,
        {DEVICE_TYPE_STATE_T} *deviceState,
        const {INPUT_PORT_PROPERTIES_T} *edgeProperties,
        {INPUT_PORT_STATE_T} *edgeState,
        const {INPUT_PORT_MESSAGE_T} *message
    ){{
      {DEVICE_TYPE_C_LOCAL_CONSTANTS}

      // Begin custom handler
      {INPUT_PORT_RECEIVE_HANDLER_SOURCE_LOCATION}
      {INPUT_PORT_RECEIVE_HANDLER}
      __POETS_REVERT_PREPROC_DETOUR__
      // End custom handler
    }}
    """.format(**pinProps))

def render_send_handler_as_softswitch(pin, dst, pinProps):
    pinProps = pinProps or make_output_pin_properties(pinProps)
    pinProps = add_props(pinProps, {
        "DEVICE_TYPE_C_LOCAL_CONSTANTS" : calc_device_type_c_locals(pin.parent,pinProps)
    })
    dst.write("""
bool {OUTPUT_PORT_FULL_ID}_send_handler(
    const {GRAPH_TYPE_PROPERTIES_T} *graphProperties,
    const {DEVICE_TYPE_PROPERTIES_T} *deviceProperties,
    {DEVICE_TYPE_STATE_T} *deviceState,
    {OUTPUT_PORT_MESSAGE_T} *message
){{
  bool _doSend=true;
  bool *doSend=&_doSend;
  // Begin custom handler
  {OUTPUT_PORT_SEND_HANDLER_SOURCE_LOCATION}
  {OUTPUT_PORT_SEND_HANDLER}
  __POETS_REVERT_PREPROC_DETOUR__
  // End custom handler
  return doSend;
}}
""".format(**pinProps))

def render_device_type_as_softswitch_defs(dt,dst,dtProps):
    render_rts_handler_as_softswitch(dt,dst,dtProps)
    for ip in dt.inputs_by_index:
        render_receive_handler_as_softswitch(ip,dst,make_input_pin_properties(ip))
    for op in dt.outputs_by_index:
        render_send_handler_as_softswitch(op,dst,make_output_pin_properties(op))
    
    dst.write("InputPinVTable INPUT_VTABLES_{DEVICE_TYPE_FULL_ID}[INPUT_COUNT_{DEVICE_TYPE_FULL_ID}_V]={{\n".format(**dtProps))
    i=0
    for i in range(len(dt.inputs_by_index)):
        ip=dt.inputs_by_index[i]
        if i!=0:
            dst.write("  ,\n");
            
        propertiesSize=get_typed_data_size(ip.properties)
        stateSize=get_typed_data_size(ip.state)
        dst.write("""
            {{
                (receive_handler_t){INPUT_PORT_FULL_ID}_receive_handler,
                sizeof(packet_t)+sizeof({INPUT_PORT_MESSAGE_T}),
                {ACTUAL_PROPERTIES_SIZE}, //sizeof({INPUT_PORT_PROPERTIES_T}),
                {ACTUAL_STATE_SIZE}, //sizeof({INPUT_PORT_STATE_T}),
                "{INPUT_PORT_NAME}",
                {IS_APPLICATION}
            }}
            """.format(ACTUAL_PROPERTIES_SIZE=propertiesSize, ACTUAL_STATE_SIZE=stateSize, **make_input_pin_properties(ip)))
    dst.write("};\n");
    
    dst.write("OutputPinVTable OUTPUT_VTABLES_{DEVICE_TYPE_FULL_ID}[OUTPUT_COUNT_{DEVICE_TYPE_FULL_ID}_V]={{\n".format(**dtProps))
    i=0
    for i in range(len(dt.outputs_by_index)):
        if i!=0:
            dst.write("  ,\n");
        op=dt.outputs_by_index[i]
        dst.write("""
            {{
                (send_handler_t){OUTPUT_PORT_FULL_ID}_send_handler,
                sizeof(packet_t)+sizeof({OUTPUT_PORT_MESSAGE_T}),
                "{OUTPUT_PORT_NAME}",
                {IS_APPLICATION},
                {MESSAGETYPE_NUMID}
            }}
            """.format(**make_output_pin_properties(op)))
    dst.write("};\n");
    
edgeCosts=[]

def render_graph_type_as_softswitch_defs(gt,dst):
    dst.write("""#include "{}.hpp"\n""".format(gt.id))
        
    dst.write(calc_graph_type_c_globals(gt))
    
    if gt.shared_code:
        for c in gt.shared_code:
            dst.write(c)
    
    gtProps=make_graph_type_properties(gt)
    for dt in gt.device_types.values():
        dtProps=make_device_type_properties(dt)
        render_device_type_as_softswitch_defs(dt,dst,dtProps)

    dst.write("const DeviceTypeVTable softswitch_device_vtables[DEVICE_TYPE_COUNT_V] = {{".format(**gtProps))
    first=True
    
    # Have to put these out in the same order as the device indexes
    for (name,dt) in sorted( [ (dt.id,dt) for dt in gt.device_types.values() ] ):
        if first:
            first=False
        else:
            dst.write(",\n")
        dst.write("""
        {{
            (ready_to_send_handler_t){GRAPH_TYPE_ID}_{DEVICE_TYPE_ID}_ready_to_send_handler,
            OUTPUT_COUNT_{DEVICE_TYPE_FULL_ID}_V,
            OUTPUT_VTABLES_{DEVICE_TYPE_FULL_ID},
            INPUT_COUNT_{DEVICE_TYPE_FULL_ID}_V,
            INPUT_VTABLES_{DEVICE_TYPE_FULL_ID},
            (compute_handler_t)0, // No compute handler
            "{DEVICE_TYPE_FULL_ID}"
        }}
        """.format(**make_device_type_properties(dt)))
    dst.write("};\n")

def render_graph_instance_as_thread_context(
    dst,
    globalProperties, # Giant array of read-only BLOBs
    globalState,      # Giant array of read-write BLOBs
    globalOutputPinTargets, # Giant array of outputpin targets
    gi,
    thread_index, # Thread index we are working on
    devices_to_thread,      # Map from device:thread
    thread_to_devices,      # Map from thread:[device]
    edges_in,               # Map from di:{ip:ei}
    edges_out,              # Map from ei:{op:ei}
    props   # Map from graph and device types to properties
    ):
        
    # Make sure we are starting new cache line boundaries for this thread
    globalProperties.pad(_cache_line_size)
    globalState.pad(_cache_line_size)
        
    dst.write("//////// Thread {}\n".format(thread_index))  

    for di in thread_to_devices[thread_index]:
        devicePropertiesOffset=None
        deviceStateOffset=None
        
        if di.device_type.properties:
            devicePropertiesName=di.id+"_properties"
            if not _use_BLOB:
                render_typed_data_inst(di.device_type.properties, props[di.device_type]["DEVICE_TYPE_PROPERTIES_T"], di.properties, devicePropertiesName, dst,True)
            else:
                blob=convert_typed_data_inst_to_bytes(di.device_type.properties, di.properties)
                devicePropertiesOffset=globalProperties.add(devicePropertiesName, blob)
            
        if di.device_type.state:
            deviceStateName=di.id+"_state"
            if not _use_BLOB:
                render_typed_data_inst(di.device_type.state, props[di.device_type]["DEVICE_TYPE_STATE_T"], None, deviceStateName, dst)
            else:
                blob=convert_typed_data_inst_to_bytes(di.device_type.state, None) # TODO : This could be much cheaper, as it is always the same
                deviceStateOffset=globalState.add(deviceStateName, blob)

        render_device_instance_outputs(devices_to_thread, di, dst, edges_out, globalOutputPinTargets, globalProperties,
                                        thread_index, thread_to_devices)

        render_device_instance_inputs(devices_to_thread, di, dst, edges_in, globalProperties, globalState, props,
                                      thread_to_devices)

    dst.write("""
    DeviceContext DEVICE_INSTANCE_CONTEXTS_thread{THREAD_INDEX}[DEVICE_INSTANCE_COUNT_thread{THREAD_INDEX}_V]={{
    """.format(THREAD_INDEX=thread_index))
    
    first=True
    for di in thread_to_devices[thread_index]:
        if first:
            first=False
        else:
            dst.write(",")
        diProps={
            "DEVICE_TYPE_FULL_ID" : props[di.device_type]["DEVICE_TYPE_FULL_ID"],
            "DEVICE_INSTANCE_ID" : di.id,
            "DEVICE_INSTANCE_THREAD_OFFSET" : thread_to_devices[thread_index].index(di),
            "DEVICE_INSTANCE_PROPERTIES" : 0,
            "DEVICE_INSTANCE_STATE" : 0,
            "DEVICE_INSTANCE_TARGETS" : "{}_targets".format(di.id)
        }   
        if di.device_type.properties:
            if not _use_BLOB:
                diProps["DEVICE_INSTANCE_PROPERTIES"]="&{}_properties".format(di.id)
            else:
                (offset,length)=globalProperties.find(di.id+"_properties")
                if not _use_indirect_BLOB:
                    diProps["DEVICE_INSTANCE_PROPERTIES"]="softswitch_pthread_global_properties+{}".format(offset)
                else:
                    diProps["DEVICE_INSTANCE_PROPERTIES"]="(void*){}".format(offset)
        if di.device_type.state:
            if not _use_BLOB:
                diProps["DEVICE_INSTANCE_STATE"]="&{}_state".format(di.id)
            else:
                (offset,length)=globalState.find(di.id+"_state")
                if not _use_indirect_BLOB:
                    diProps["DEVICE_INSTANCE_STATE"]="softswitch_pthread_global_state+{}".format(offset)
                else:
                    diProps["DEVICE_INSTANCE_STATE"]="(void*){}".format(offset)
        if _use_BLOB:
            offset=globalOutputPinTargets.find(diProps["DEVICE_INSTANCE_TARGETS"])
            diProps["DEVICE_INSTANCE_TARGETS"]="softswitch_pthread_output_pin_targets+{}".format(offset)
        
        diProps["DEVICE_INSTANCE_VTABLE_SOURCE"]="softswitch_device_vtables+DEVICE_TYPE_INDEX_{DEVICE_TYPE_FULL_ID}_V".format(**diProps)
        if _use_indirect_BLOB:
            diProps["DEVICE_INSTANCE_VTABLE_SOURCE"]="(DeviceTypeVTable*)DEVICE_TYPE_INDEX_{DEVICE_TYPE_FULL_ID}_V".format(**diProps)
        
        dst.write("""
        {{
            {DEVICE_INSTANCE_VTABLE_SOURCE}, // vtable
            {DEVICE_INSTANCE_PROPERTIES},
            {DEVICE_INSTANCE_STATE},
            {DEVICE_INSTANCE_THREAD_OFFSET}, // device index
            {DEVICE_INSTANCE_TARGETS}, // address lists for pins
            {DEVICE_INSTANCE_ID}_sources, // source list for inputs
            "{DEVICE_INSTANCE_ID}", // id
            0, // rtsFlags
            false, // rtc
            0,  // prev
            0   // next
        }}
        """.format(**diProps))
    dst.write("};\n")


def render_device_instance_inputs(devices_to_thread, di, dst, edges_in, globalProperties, globalState, props,
                                  thread_to_devices):
    for ip in di.device_type.inputs_by_index:
        if ip.properties == None and ip.state == None:
            continue
        for ei in edges_in[di][ip]:
            name = "{}_{}_{}_{}".format(ei.dst_device.id, ei.dst_pin.name, ei.src_device.id, ei.src_pin.name)
            if ip.properties:
                inputPropertiesName = name + "_properties"
                if not _use_BLOB:
                    render_typed_data_inst(ip.properties, props[ip]["INPUT_PORT_PROPERTIES_T"], ei.properties,
                                           inputPropertiesName, dst, True)
                else:
                    blob = convert_typed_data_inst_to_bytes(ip.properties, ei.properties)
                    inputPropertiesOffset = globalProperties.add(inputPropertiesName, blob)
            if ip.state:
                inputStateName = name + "_state"
                if not _use_BLOB:
                    render_typed_data_inst(ip.state, props[ip]["INPUT_PORT_STATE_T"], None, inputStateName, dst)
                else:
                    blob = convert_typed_data_inst_to_bytes(ip.state,
                                                            None)  # TODO : This could be much cheaper, as it is always the same
                    inputStateOffset = globalState.add(inputStateName, blob)
    for ip in di.device_type.inputs_by_index:
        if ip.properties == None and ip.state == None:
            continue
        if len(edges_in[di][ip]) == 0:
            continue
        dst.write("InputPinBinding {}_{}_bindings[]={{\n".format(di.id, ip.name))
        first = True

        def edge_key(ei):
            return (devices_to_thread[ei.src_device],
                    thread_to_devices[devices_to_thread[ei.src_device]].index(ei.src_device),
                    int(props[ei.src_pin]["OUTPUT_PORT_INDEX"]))

        eiSorted = list(edges_in[di][ip])
        eiSorted.sort(key=edge_key)

        for ei in eiSorted:
            if first:
                first = False
            else:
                dst.write(",\n")
            name = "{}_{}_{}_{}".format(ei.dst_device.id, ei.dst_pin.name, ei.src_device.id, ei.src_pin.name)
            eProps = {
                "SRC_THREAD_INDEX": devices_to_thread[ei.src_device],
                "SRC_THREAD_OFFSET": thread_to_devices[devices_to_thread[ei.src_device]].index(ei.src_device),
                "SRC_PORT_INDEX": props[ei.src_pin]["OUTPUT_PORT_INDEX"],
                "EDGE_PROPERTIES": 0,
                "EDGE_STATE": 0
            }
            if ip.properties:
                if not _use_BLOB:
                    eProps["EDGE_PROPERTIES"] = "&{}_properties".format(name)
                else:
                    (offset, length) = globalProperties.find("{}_properties".format(name))
                    if not _use_indirect_BLOB:
                        edgePropertiesBinding = "softswitch_pthread_global_properties+{}".format(offset)
                    else:
                        edgePropertiesBinding = "(const void *){}".format(offset)
                    eProps["EDGE_PROPERTIES"] = edgePropertiesBinding
            if ip.state:
                if not _use_BLOB:
                    eProps["EDGE_STATE"] = "&{}_state".format(name)
                else:
                    (offset, length) = globalState.find("{}_state".format(name))
                    eProps["EDGE_STATE"] = "softswitch_pthread_global_state+{}".format(offset)
            dst.write("""
                    {{
                        {{
                            {SRC_THREAD_INDEX}, // thread id
                            {SRC_THREAD_OFFSET}, // thread offset
                            {SRC_PORT_INDEX}, // pin
                            0
                        }},
                        {EDGE_PROPERTIES},
                        {EDGE_STATE}
                    }}
                    """.format(**eProps))
        dst.write("};\n");
    dst.write("InputPinSources {}_sources[]={{\n".format(di.id))
    first = True
    for ip in di.device_type.inputs_by_index:
        if first:
            first = False
        else:
            dst.write(",")
        if ip.properties == None and ip.state == None:
            dst.write("  {{0,0}} // {}\n".format(ip.name))
        else:
            dst.write("  {{ {}, {}_{}_bindings }}\n // {}\n".format(len(edges_in[di][ip]), di.id, ip.name, ip.name))
    dst.write("};\n")


def render_device_instance_outputs(devices_to_thread, di, dst, edges_out, globalOutputPinTargets, globalProperties,
                                    thread_index, thread_to_devices):
    for op in di.device_type.outputs_by_index:
        addressesName = "{}_{}_addresses".format(di.id, op.name)
        srcId = thread_index
        key = lambda ei: thread_distance(devices_to_thread[ei.dst_device], srcId)

        edges = edges_out[di][op]
        if sort_edges_by_distance != 0:
            # print([key(i) for i in edges])
            edges = sorted(edges, key=key, reverse=sort_edges_by_distance > 0)
            # print([key(i) for i in edges])

        cost = sum([key(x) for x in edges])
        if cost >= len(edgeCosts):
            edgeCosts.extend([0] * (1 + cost - len(edgeCosts)))
        edgeCosts[cost] += 1

        if not _use_BLOB:
            dst.write("address_t {}[]={{\n".format(addressesName))
            first = True
            for ei in edges:
                if first:
                    first = False
                else:
                    dst.write(",")
                dstDev = ei.dst_device
                tIndex = devices_to_thread[dstDev]
                tOffset = thread_to_devices[tIndex].index(dstDev)
                dst.write("  {{{},{},INPUT_INDEX_{}_{}_{}_V, 0}}".format(tIndex, tOffset, dstDev.device_type.parent.id,
                                                                         dstDev.device_type.id, ei.dst_pin.name))
            dst.write("};\n")
        else:
            blob = bytearray([])
            for ei in edges:
                dstDev = ei.dst_device
                tIndex = devices_to_thread[dstDev]
                tOffset = thread_to_devices[tIndex].index(dstDev)
                pin_index = ei.dst_pin.parent.inputs_by_index.index(ei.dst_pin)
                addr = convert_address_to_bytes(tIndex, tOffset, pin_index)
                blob.extend(addr)
            globalProperties.add(addressesName, blob)
    if not _use_BLOB:
        dst.write("OutputPinTargets {}_targets[]={{\n".format(di.id))
    else:
        globalOutputPinTargets.add_label("{}_targets".format(di.id))
    first = True
    for op in di.device_type.outputs_by_index:
        addressesName = "{}_{}_addresses".format(di.id, op.name)
        if not _use_BLOB:
            addressesSource = addressesName
        else:
            (offset, length) = globalProperties.find(addressesName)
            if not _use_indirect_BLOB:
                addressesSource = "(address_t*)(softswitch_pthread_global_properties+{})".format(offset)
            else:
                addressesSource = "(address_t*)({})".format(offset)

        if not _use_BLOB:
            if first:
                first = False
            else:
                dst.write(",")
            dst.write("{{ {}, {} }} // {}\n".format(len(edges_out[di][op]), addressesSource, op.name))
        else:
            globalOutputPinTargets.add(addressesName, (len(edges_out[di][op]), addressesSource))
    if not _use_BLOB:
        dst.write("};\n")  # End of output pin targets


def render_graph_instance_as_softswitch(gi,dst,num_threads,device_to_thread):
    
    globalProperties=BLOBHolder()
    globalProperties.add("__nudge__", bytearray([0]*4))
    
    globalState=BLOBHolder()
    globalState.add("__nudge__", bytearray([0]*4))
    
    globalOutputPinTargets=StructHolder("OutputPinTargets", 2);
    globalOutputPinTargets.add("__nudge__", (0,0))
    
    props={
        gi.graph_type:make_graph_type_properties(gi.graph_type)
    }
    for dt in gi.graph_type.device_types.values():
        props[dt]=make_device_type_properties(dt)
        for ip in dt.inputs_by_index:
            props[ip]=make_input_pin_properties(ip)
        for op in dt.outputs_by_index:
            props[op]=make_output_pin_properties(op)
        
    edgesIn={ di:{ pin:[] for pin in di.device_type.inputs_by_index  } for di in gi.device_instances.values() }
    edgesOut={ di:{ pin:[] for pin in di.device_type.outputs_by_index } for di in gi.device_instances.values() }
    
    for ei in gi.edge_instances.values():
        edgesIn[ei.dst_device][ei.dst_pin].append(ei)
        edgesOut[ei.src_device][ei.src_pin].append(ei)
    
    thread_to_devices=[[] for i in range(num_threads)]
    devices_to_thread={}
    for d in gi.device_instances.values():
        if d.id not in device_to_thread:
            logging.error("device {}  not in mapping".format(d.id))
        t=device_to_thread[d.id]
        assert(t>=0 and t<num_threads)
        thread_to_devices[t].append(d)
        devices_to_thread[d]=t

    # calculate statistics of distribution
    device_per_thread_hist=[]
    device_per_thread_median=0
    for d in thread_to_devices:
        c=len(d)
        while c >= len(device_per_thread_hist):
            device_per_thread_hist.append(0)
        device_per_thread_hist[c] += 1
    for i in range(len(device_per_thread_hist)):
        if device_per_thread_hist[i]!=0:
            print("renderSoftswitchDevicesPerThreadHist, {}, {}, threads\n".format(i, device_per_thread_hist[i]), file=measure_file)
    device_per_thread_counts=[len(x) for x in thread_to_devices]
    print_statistics(dst, "renderSoftswitchDevicesPerThread", "devices/thread", device_per_thread_counts)
    

        
    dst.write("""
    #include "{GRAPH_TYPE_ID}.hpp"

    const unsigned THREAD_COUNT={NUM_THREADS};
    enum{{ THREAD_COUNT_V={NUM_THREADS} }};

    const unsigned DEVICE_INSTANCE_COUNT={TOTAL_INSTANCES};
    enum {{ DEVICE_INSTANCE_COUNT_V={TOTAL_INSTANCES} }};
    """.format(GRAPH_TYPE_ID=gi.graph_type.id, NUM_THREADS=num_threads, TOTAL_INSTANCES=len(gi.device_instances)))

    for ti in range(num_threads):
        dst.write("    const unsigned DEVICE_INSTANCE_COUNT_thread{}={};\n".format(ti,len(thread_to_devices[ti])))
        dst.write("    enum{{ DEVICE_INSTANCE_COUNT_thread{}_V={} }};\n".format(ti,len(thread_to_devices[ti])))
    
    for ti in range(num_threads):
        render_graph_instance_as_thread_context(
            dst,
            globalProperties,globalState,globalOutputPinTargets,
            gi,ti,devices_to_thread, thread_to_devices,edgesIn,edgesOut,props
            )

    graphPropertiesName=gi.id+"_properties"
    if not _use_BLOB:
        render_typed_data_inst(gi.graph_type.properties, props[gi.graph_type]["GRAPH_TYPE_PROPERTIES_T"], gi.properties, graphPropertiesName, dst,True)
        graphPropertiesBinding="&{}".format(graphPropertiesName)
    else:
        blob=convert_typed_data_inst_to_bytes(gi.graph_type.properties, gi.properties)
        graphPropertiesOffset=globalProperties.add(graphPropertiesName, blob)
        graphPropertiesBinding="softswitch_pthread_global_properties+{}".format(graphPropertiesOffset)

    dst.write("\n")
    dst.write("unsigned softswitch_pthread_count = THREAD_COUNT_V;\n\n")

    dst.write("PThreadContext softswitch_pthread_contexts[THREAD_COUNT_V]=\n")
    dst.write("{\n")
    pointersAreRelative=0
    if _use_indirect_BLOB:
        pointersAreRelative=1
    for ti in range(num_threads):
        dst.write("""
        {{
            {},
            {},
            DEVICE_TYPE_COUNT_V,
            softswitch_device_vtables,
            DEVICE_INSTANCE_COUNT_thread{}_V,
            DEVICE_INSTANCE_CONTEXTS_thread{},
            0, // lamport
            0, // rtsHead
            0, // rtsTail
            0, // rtcChecked
            0,  // rtcOffset
            3,  // applLogLevel
            3,  // softLogLevel
            3,  // hardLogLevel
            0,  // currentDevice
            0,  // currentMode
            0,   // currentHandler
            {}  // pointersAreRelative
        }}\n""".format(ti,graphPropertiesBinding,ti,ti,pointersAreRelative))
        if ti+1<num_threads:
            dst.write(",\n")
            
    dst.write("};\n")

    dst.write("const ")
    globalProperties.write(dst, "softswitch_pthread_global_properties")
    
    globalState.write(dst, "softswitch_pthread_global_state")
    
    dst.write("const ");
    globalOutputPinTargets.write(dst, "softswitch_pthread_output_pin_targets")

    print("graphOutputInstCount, -, {}, \n".format(len(edgeCosts)), file=measure_file)
    print_statistics(dst, "renderSoftswitchOutputInstCost", "estimatedTotalOutgoingEdgeDistance", edgeCosts)
  

import argparse

#logging.getLogger(__name__)

parser = argparse.ArgumentParser(description='Render graph instance as softswitch.')
parser.add_argument('source', type=str, help='source file (xml graph instance)')
parser.add_argument('--dest', help="Directory to write the output to", default=".")
parser.add_argument('--threads', help='number of logical threads to use (active threads)', type=int, default=2)
parser.add_argument('--hardware-threads', help='number of threads used in hardware (default=threads)', type=int, default=0)
parser.add_argument('--contraction', help='if threads < hardware-threads, how to do mapping. "dense", "sparse", or "random"', type=str, default="dense")
parser.add_argument('--log-level', dest="logLevel", help='logging level (INFO,ERROR,...)', default='WARNING')
parser.add_argument('--placement-seed', dest="placementSeed", help="Choose a specific random placement", default=None)
parser.add_argument('--destination-ordering', dest="destinationOrdering", help="Should messages be send 'furthest-first', 'random', 'nearest-first'", default="random")
parser.add_argument('--measure', help="Destination for measured properties", default="tinsel.render_softswitch.csv")
parser.add_argument('--message-types', help="a file that prints the message types and their enumerated values. This is used to decode messages at the executive", default="messages.csv")
parser.add_argument('--app-pins-addr-map', help="a file that gives the address map of the input pins, used by the executive to send messages to devices", default="appPinInMap.csv")

args = parser.parse_args()

logLevel = getattr(logging, args.logLevel.upper(), None)
logging.basicConfig(level=logLevel)

nThreads=args.threads
if args.hardware_threads==0:
    hwThreads=nThreads
else:
    hwThreads=args.hardware_threads

if args.destinationOrdering=="furthest-first":
    sort_edges_by_distance=+1
elif args.destinationOrdering=="nearest-first":
    sort_edges_by_distance=-1
elif args.destinationOrdering=="random":
    sort_edges_by_distance=0
else:
    assert False, "Didn't understand destination-ordering={}".format(args.destinationOrdering)

measure_file=open(args.measure, "wt")

if args.source=="-":
    source=sys.stdin
    sourcePath="[graph-type-file]"
else:
    sys.stderr.write("Reading graph type from '{}'\n".format(args.source))
    source=open(args.source,"rt")
    sourcePath=os.path.abspath(args.source)
    sys.stderr.write("Using absolute path '{}' for pre-processor directives\n".format(sourcePath))


(types,instances)=load_graph_types_and_instances(source, sourcePath)

if len(types)!=1:
    raise RuntimeError("File did not contain exactly one graph type.")

graph=None
for g in types.values():
    graph=g
    break

destPrefix=args.dest

destHppPath=os.path.abspath("{}/{}.hpp".format(destPrefix,graph.id))
destHpp=open(destHppPath,"wt")
sys.stderr.write("Using absolute path '{}' for header pre-processor directives\n".format(destHppPath))

destCppPath=os.path.abspath("{}/{}_vtables.cpp".format(destPrefix,graph.id))
destCpp=open(destCppPath,"wt")
sys.stderr.write("Using absolute path '{}' for source pre-processor directives\n".format(destCppPath))

class OutputWithPreProcLineNum:
    def __init__(self,dest,destPath):
        self.dest=dest
        self.destPath=destPath
        self.lineNum=1
        
    def write(self,msg):
        if -1 == msg.find("\n"):
            self.dest.write(msg)
        else:
            for line in msg.splitlines():
                if line.strip()=="__POETS_REVERT_PREPROC_DETOUR__":
                    self.dest.write('#line {} "{}"\n'.format(self.lineNum, self.destPath))
                else:
                    self.dest.write(line+"\n")
                self.lineNum+=1
    
# Get a unique id for each message type which will be sent in the message header so that the executive can decode it
assign_unique_ident_for_each_message_type(graph)
destMessageTypeID=open(args.message_types, "w+")
output_unique_ident_for_each_message_type(graph, destMessageTypeID)

render_graph_type_as_softswitch_decls(graph, OutputWithPreProcLineNum(destHpp, destHppPath))
render_graph_type_as_softswitch_defs(graph, OutputWithPreProcLineNum(destCpp, destCppPath))

if(len(instances)>0):
    inst=None
    for g in instances.values():
        inst=g
        break
        
    assert(inst.graph_type.id==graph.id)

    print("graphDeviceInstCount, -, {}, devices".format(len(inst.device_instances)), file=measure_file)
    print("graphEdgeInstCount, -, {}, edges".format(len(inst.edge_instances)), file=measure_file)
    
    partitionInfoKey="dt10.partitions.{}".format(nThreads)
    partitionInfo=(inst.metadata or {}).get(partitionInfoKey,None)
    
    if partitionInfo:
        key=partitionInfo["key"]
        logging.info("Reading partition info from metadata.")
        device_to_thread = { di.id:int(di.metadata[key]) for di in inst.device_instances.values() }
    else:
        seed=args.placementSeed or 1
        logging.info("Generating random partition from seed {}.".format(seed))
        random.seed(seed)
        device_to_thread = { id:random.randint(0,nThreads-1) for id in inst.device_instances.keys() }
        
    if nThreads!=hwThreads:
        logging.info("Contracting from {} logical to {} physical using {}".format(nThreads,hwThreads,args.contraction))
        assert nThreads<hwThreads
        if args.contraction=="dense":
            logicalToPhysical=[i for i in range(nThreads)]
        elif args.contraction=="sparse":
            scale=hwThreads/nThreads
            logicalToPhysical=[ math.floor(i*scale) for i in range(nThreads)]
        elif args.contraction=="random":
            logicalToPhysical=list(range(hwThreads))
            random.shuffle(logicalToPhysical)
            logicalToPhysical=logicalToPhysical[0:nThreads]
        else:
            assert False, "Unknown contraction method '{}'".format(args.contraction)
    
        device_to_thread = { id:logicalToPhysical[thread] for (id,thread) in device_to_thread.items() }

    destInstPath=os.path.abspath("{}/{}_{}_inst.cpp".format(destPrefix,graph.id,inst.id))
    destInst=open(destInstPath,"wt")
        
    # create a mapping from threads to devices
    thread_to_devices=[[] for i in range(hwThreads)]
    for d in inst.device_instances.values():
        if d.id not in device_to_thread:
            logging.error("device {}  not in mapping".format(d.id))
        t=device_to_thread[d.id]
        assert(t>=0 and t<hwThreads)
        print("""adding device: {} to thread {}""".format(d.id,t))
        thread_to_devices[t].append(d.id)


    # dump the device name -> address mappings into a file for sending messages from the executive
    deviceAddrMap=open(args.app_pins_addr_map,"w+")
    output_device_instance_addresses(inst, device_to_thread, thread_to_devices, deviceAddrMap) 

    render_graph_instance_as_softswitch(inst,destInst,hwThreads,device_to_thread)
