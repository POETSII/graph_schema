#!/usr/bin/python3


from graph.load_xml import load_graph_types_and_instances
from graph.write_cpp import render_graph_as_cpp

from graph.make_properties import *
from graph.calc_c_globals import *

import sys
import os
import logging

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
            dst.write("0")
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

def render_graph_type_as_softswitch_decls(gt,dst):
    gtProps=make_graph_type_properties(gt)
    dst.write("""
    #ifndef {GRAPH_TYPE_ID}_hpp
    #define {GRAPH_TYPE_ID}_hpp
    
    #include <cstdint>
    
    #include "softswitch.hpp"
    
    #define handler_log softswitch_handler_log  
    
    
    ///////////////////////////////////////////////
    // Graph type stuff

    const unsigned DEVICE_TYPE_COUNT = {GRAPH_TYPE_DEVICE_TYPE_COUNT};
    """.format(**gtProps))
    
    render_typed_data_as_struct(gt.properties,dst,gtProps["GRAPH_TYPE_PROPERTIES_T"])
    
    for mt in gt.message_types.values():
        mtProps=make_message_type_properties(mt)
        render_typed_data_as_struct(mt.message, dst,mtProps["MESSAGE_TYPE_T"])
    
    for dt in gt.device_types.values():
        dtProps=make_device_type_properties(dt)
        
        dst.write("""
        const unsigned DEVICE_TYPE_INDEX_{DEVICE_TYPE_FULL_ID} = {DEVICE_TYPE_INDEX};
        const unsigned INPUT_COUNT_{DEVICE_TYPE_FULL_ID} = {DEVICE_TYPE_INPUT_COUNT};
        const unsigned OUTPUT_COUNT_{DEVICE_TYPE_FULL_ID} = {DEVICE_TYPE_OUTPUT_COUNT};
        """.format(**dtProps))
        
        render_typed_data_as_struct(dt.properties,dst,dtProps["DEVICE_TYPE_PROPERTIES_T"])
        render_typed_data_as_struct(dt.state,dst,dtProps["DEVICE_TYPE_STATE_T"])
        
        for ip in dt.inputs_by_index:
            ipProps=make_input_port_properties(ip)
            dst.write("""
            const unsigned INPUT_INDEX_{INPUT_PORT_FULL_ID} = {INPUT_PORT_INDEX};
            """.format(**ipProps))
            
            render_typed_data_as_struct(ip.properties, dst, ipProps["INPUT_PORT_PROPERTIES_T"])
            render_typed_data_as_struct(ip.state, dst, ipProps["INPUT_PORT_STATE_T"])
            
        for op in dt.outputs_by_index:
            dst.write("""
            const unsigned OUTPUT_INDEX_{OUTPUT_PORT_FULL_ID} = {OUTPUT_PORT_INDEX};
            const uint32_t OUTPUT_FLAG_{OUTPUT_PORT_FULL_ID} = 1<<{OUTPUT_PORT_INDEX};
            """.format(**make_output_port_properties(op)))
    
    dst.write("extern DeviceTypeVTable DEVICE_TYPE_VTABLES[];\n")
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


def render_receive_handler_as_softswitch(port, dst, portProps):
    portProps = portProps or make_input_port_properties(portProps)
    portProps = add_props(portProps, {
        "DEVICE_TYPE_C_LOCAL_CONSTANTS" : calc_device_type_c_locals(port.parent,portProps)
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
    """.format(**portProps))

def render_send_handler_as_softswitch(port, dst, portProps):
    portProps = portProps or make_output_port_properties(portProps)
    portProps = add_props(portProps, {
        "DEVICE_TYPE_C_LOCAL_CONSTANTS" : calc_device_type_c_locals(port.parent,portProps)
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
""".format(**portProps))

def render_device_type_as_softswitch_defs(dt,dst,dtProps):
    render_rts_handler_as_softswitch(dt,dst,dtProps)
    for ip in dt.inputs_by_index:
        render_receive_handler_as_softswitch(ip,dst,make_input_port_properties(ip))
    for op in dt.outputs_by_index:
        render_send_handler_as_softswitch(op,dst,make_output_port_properties(op))
    
    dst.write("InputPortVTable INPUT_VTABLES_{DEVICE_TYPE_FULL_ID}[INPUT_COUNT_{DEVICE_TYPE_FULL_ID}]={{\n".format(**dtProps))
    i=0
    for i in range(len(dt.inputs_by_index)):
        ip=dt.inputs_by_index[i]
        if i!=0:
            dst.write("  ,\n");
        dst.write("""
            {{
                (receive_handler_t){INPUT_PORT_FULL_ID}_receive_handler,
                sizeof(packet_t)+sizeof({INPUT_PORT_MESSAGE_T}),
                sizeof({INPUT_PORT_PROPERTIES_T}),
                sizeof({INPUT_PORT_STATE_T}),
                "{INPUT_PORT_NAME}"
            }}
            """.format(**make_input_port_properties(ip)))
    dst.write("};\n");
    
    dst.write("OutputPortVTable OUTPUT_VTABLES_{DEVICE_TYPE_FULL_ID}[OUTPUT_COUNT_{DEVICE_TYPE_FULL_ID}]={{\n".format(**dtProps))
    i=0
    for i in range(len(dt.outputs_by_index)):
        if i!=0:
            dst.write("  ,\n");
        dst.write("""
            {{
                (send_handler_t){OUTPUT_PORT_FULL_ID}_send_handler,
                sizeof(packet_t)+sizeof({OUTPUT_PORT_MESSAGE_T}),
                "{OUTPUT_PORT_NAME}"
            }}
            """.format(**make_output_port_properties(op)))
    dst.write("};\n");
    
    

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

    dst.write("DeviceTypeVTable DEVICE_TYPE_VTABLES[DEVICE_TYPE_COUNT] = {{".format(**gtProps))
    first=True
    for dt in gt.device_types.values():
        if first:
            first=False
        else:
            dst.write(",\n")
        dst.write("""
        {{
            (ready_to_send_handler_t){GRAPH_TYPE_ID}_{DEVICE_TYPE_ID}_ready_to_send_handler,
            OUTPUT_COUNT_{DEVICE_TYPE_FULL_ID},
            OUTPUT_VTABLES_{DEVICE_TYPE_FULL_ID},
            INPUT_COUNT_{DEVICE_TYPE_FULL_ID},
            INPUT_VTABLES_{DEVICE_TYPE_FULL_ID},
            (compute_handler_t)0, // No compute handler
            "{DEVICE_TYPE_FULL_ID}"
        }}
        """.format(**make_device_type_properties(dt)))
    dst.write("};\n")

def render_graph_instance_as_thread_context(
    dst,
    gi,
    thread_index, # Thread index we are working on
    devices_to_thread,      # Map from device:thread
    thread_to_devices,      # Map from thread:[device]
    edges_in,               # Map from di:{ip:ei}
    edges_out,              # Map from ei:{op:ei}
    props   # Map from graph and device types to properties
    ):
        
    dst.write("//////// Thread {}\n".format(thread_index))    
    
    for di in thread_to_devices[thread_index]:
        if di.device_type.properties:
            render_typed_data_inst(di.device_type.properties, props[di.device_type]["DEVICE_TYPE_PROPERTIES_T"], di.properties, di.id+"_properties", dst,True)
        if di.device_type.state:
            render_typed_data_inst(di.device_type.state, props[di.device_type]["DEVICE_TYPE_STATE_T"], None, di.id+"_state",dst)
        
        for op in di.device_type.outputs_by_index:
            dst.write("address_t {}_{}_addresses[]={{\n".format(di.id, op.name))
            first=True
            for ei in edges_out[di][op]:
                if first:
                    first=False
                else:
                    dst.write(",")
                dstDev=ei.dst_device
                tIndex=devices_to_thread[dstDev]
                tOffset=thread_to_devices[tIndex].index(dstDev)
                dst.write("{{{},{},INPUT_INDEX_{}_{}_{}}}".format(tIndex,tOffset,dstDev.device_type.parent.id, dstDev.device_type.id, ei.dst_port.name))
            dst.write("};\n")
        
        dst.write("OutputPortTargets {}_targets[]={{".format(di.id))
        first=True
        for ei in edges_out[di][op]:
            if first:
                first=False
            else:
                dst.write(",")
            dst.write("{{ {}, {}_{}_addresses }}".format( len(edges_out[di][op]), di.id, op.name ))
        dst.write("};\n")
        
        for ip in di.device_type.inputs_by_index:
            if ip.properties==None and ip.state==None:
                continue
            for ei in edges_in[di][ip]:
                name="{}_{}_{}_{}".format(ei.dst_device.id,ei.dst_port.name,ei.src_device.id,ei.src_port.name)
                if ip.properties:
                    render_typed_data_inst(ip.properties, props[ip]["INPUT_PORT_PROPERTIES_T"], ei.properties, name+"_properties", dst,True)
                if ip.state:
                    render_typed_data_inst(ip.state, props[ip]["INPUT_PORT_STATE_T"], None, name+"_state", dst)

        for ip in di.device_type.inputs_by_index:
            if ip.properties==None and ip.state==None:
                continue
            if len(edges_in[di][ip])==0:
                continue
            dst.write("InputPortBinding {}_{}_bindings[]={{\n".format(di.id,ip.name))
            first=True
            
            def edge_key(ei):
                return ((devices_to_thread[ei.src_device]<<32) +
                    (thread_to_devices[devices_to_thread[ei.src_device]].index(ei.src_device))<<16 +
                    int(props[ei.src_port]["OUTPUT_PORT_INDEX"]))
            
            eiSorted=list(edges_in[di][ip])
            eiSorted.sort(key=edge_key)
            
            for ei in eiSorted:
                if first:
                    first=False
                else:
                    dst.write(",\n")
                name="{}_{}_{}_{}".format(ei.dst_device.id,ei.dst_port.name,ei.src_device.id,ei.src_port.name)
                eProps={
                    "SRC_THREAD_INDEX": devices_to_thread[ei.src_device],
                    "SRC_THREAD_OFFSET": thread_to_devices[devices_to_thread[ei.src_device]].index(ei.src_device),
                    "SRC_PORT_INDEX": props[ei.src_port]["OUTPUT_PORT_INDEX"],
                    "EDGE_PROPERTIES":0,
                    "EDGE_STATE":0
                }
                if ip.properties:
                    eProps["EDGE_PROPERTIES"]="&{}_properties".format(name)
                if ip.state:
                    eProps["EDGE_STATE"]="&{}_state".format(name)
                dst.write("""
                    {{
                        {{
                            {SRC_THREAD_INDEX}, // thread id
                            {SRC_THREAD_OFFSET}, // thread offset
                            {SRC_PORT_INDEX} // port
                        }},
                        {EDGE_PROPERTIES},
                        {EDGE_STATE}
                    }}
                    """.format(**eProps))
            dst.write("};\n");

        dst.write("InputPortSources {}_sources[]={{".format(di.id))
        first=True
        
        for ip in di.device_type.inputs_by_index:
            if first:
                first=False
            else:
                dst.write(",")
            if ip.properties==None and ip.state==None:
                dst.write("{0,0}\n")
            else:
                dst.write("{{ {}, {}_{}_bindings }}\n".format( len(edges_in[di][ip]), di.id, ip.name ))
        dst.write("};\n")
        

    dst.write("""
    DeviceContext DEVICE_INSTANCE_CONTEXTS_thread{THREAD_INDEX}[DEVICE_INSTANCE_COUNT_thread{THREAD_INDEX}]={{
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
            "DEVICE_INSTANCE_STATE" : 0
        }   
        if di.device_type.properties:
            diProps["DEVICE_INSTANCE_PROPERTIES"]="&{}_properties".format(di.id)
        if di.device_type.state:
            diProps["DEVICE_INSTANCE_STATE"]="&{}_state".format(di.id)
        dst.write("""
        {{
            DEVICE_TYPE_VTABLES+DEVICE_TYPE_INDEX_{DEVICE_TYPE_FULL_ID}, // vtable
            {DEVICE_INSTANCE_PROPERTIES},
            {DEVICE_INSTANCE_STATE},
            {DEVICE_INSTANCE_THREAD_OFFSET}, // device index
            {DEVICE_INSTANCE_ID}_targets, // address lists for ports
            {DEVICE_INSTANCE_ID}_sources, // source list for inputs
            "{DEVICE_INSTANCE_ID}", // id
            0, // rtsFlags
            false, // rtc
            0,  // prev
            0   // next
        }}
        """.format(**diProps))
    dst.write("};\n")

def render_graph_instance_as_softswitch(gi,dst,num_threads,device_to_thread):
    props={
        gi.graph_type:make_graph_type_properties(gi.graph_type)
    }
    for dt in gi.graph_type.device_types.values():
        props[dt]=make_device_type_properties(dt)
        for ip in dt.inputs_by_index:
            props[ip]=make_input_port_properties(ip)
        for op in dt.outputs_by_index:
            props[op]=make_output_port_properties(op)
        
    edgesIn={ di:{ port:[] for port in di.device_type.inputs_by_index  } for di in gi.device_instances.values() }
    edgesOut={ di:{ port:[] for port in di.device_type.outputs_by_index } for di in gi.device_instances.values() }
    
    for ei in gi.edge_instances.values():
        edgesIn[ei.dst_device][ei.dst_port].append(ei)
        edgesOut[ei.src_device][ei.src_port].append(ei)
    
    thread_to_devices=[[] for i in range(num_threads)]
    devices_to_thread={}
    for d in gi.device_instances.values():
        if d.id not in device_to_thread:
            logging.error("device {}  not in mapping".format(d.id))
        t=device_to_thread[d.id]
        assert(t>=0 and t<num_threads)
        thread_to_devices[t].append(d)
        devices_to_thread[d]=t
    
    dst.write("""
    #include "{GRAPH_TYPE_ID}.hpp"

    const unsigned THREAD_COUNT={NUM_THREADS};

    const unsigned DEVICE_INSTANCE_COUNT={TOTAL_INSTANCES};
    """.format(GRAPH_TYPE_ID=gi.graph_type.id, NUM_THREADS=num_threads, TOTAL_INSTANCES=len(gi.device_instances)))

    for ti in range(num_threads):
        dst.write("    const unsigned DEVICE_INSTANCE_COUNT_thread{}={};\n".format(ti,len(thread_to_devices[ti])))
    
    for ti in range(num_threads):
        render_graph_instance_as_thread_context(dst,gi,ti,devices_to_thread, thread_to_devices,edgesIn,edgesOut,props)

    render_typed_data_inst(gi.graph_type.properties, props[gi.graph_type]["GRAPH_TYPE_PROPERTIES_T"], gi.properties, gi.id+"_properties", dst,True)

    dst.write("\n")
    dst.write("unsigned softswitch_pthread_count = THREAD_COUNT;\n\n")

    dst.write("PThreadContext softswitch_pthread_contexts[THREAD_COUNT]=\n")
    dst.write("{\n")
    for ti in range(num_threads):
        dst.write("""
        {{
            {},
            &{}_properties,
            DEVICE_TYPE_COUNT,
            DEVICE_TYPE_VTABLES,
            DEVICE_INSTANCE_COUNT_thread{},
            DEVICE_INSTANCE_CONTEXTS_thread{},
            0, // lamport
            0, // rtsHead
            0, // rtsTail
            0, // rtcChecked
            0,  // rtcOffset
            0,  // applLogLevel
            0,  // softLogLevel
            0,  // hardLogLevel
            0,  // currentDevice
            0,  // currentMode
            0   // currentHandler
        }}\n""".format(ti,gi.id,ti,ti))
        if ti+1<num_threads:
            dst.write(",\n")
            
    dst.write("};\n")



import argparse

#logging.getLogger(__name__)

parser = argparse.ArgumentParser(description='Render graph instance as softswitch.')
parser.add_argument('source', type=str, help='source file (xml graph instance)')
parser.add_argument('--dest', help="Directory to write the output to", default=".")
parser.add_argument('--threads', help='number of threads', type=int, default=2)
parser.add_argument('--log-level', dest="logLevel", help='logging level (INFO,ERROR,...)', default='WARNING')

args = parser.parse_args()

logLevel = getattr(logging, args.logLevel.upper(), None)
logging.basicConfig(level=logLevel)

nThreads=args.threads

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
    

render_graph_type_as_softswitch_decls(graph, OutputWithPreProcLineNum(destHpp, destHppPath))
render_graph_type_as_softswitch_defs(graph, OutputWithPreProcLineNum(destCpp, destCppPath))

if(len(instances)>0):
    inst=None
    for g in instances.values():
        inst=g
        break
        
    assert(inst.graph_type.id==graph.id)
    
    partitionInfoKey="dt10.partitions.{}".format(nThreads)
    partitionInfo=(inst.metadata or {}).get(partitionInfoKey,None)
    
    if partitionInfo:
        key=partitionInfo["key"]
        logging.info("Reading partition info from metadata.")
        device_to_thread = { di.id:int(di.metadata[key]) for di in inst.device_instances.values() }
    else:
        logging.info("Generating random partition.")
        device_to_thread = { id:(hash(id)%nThreads) for id in inst.device_instances.keys() }

    destInstPath=os.path.abspath("{}/{}_{}_inst.cpp".format(destPrefix,graph.id,inst.id))
    destInst=open(destInstPath,"wt")
        
    render_graph_instance_as_softswitch(inst,destInst,nThreads,device_to_thread)
