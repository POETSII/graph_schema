from graph.load_xml import load_graph_types_and_instances
from graph.write_cpp import render_graph_as_cpp

from graph.make_properties import *
from graph.calc_c_globals import *

import sys
import os

def render_typed_data_as_type_decl(td,dst,indent,name=None):
    name=name or td.name
    if isinstance(td,ScalarTypedDataSpec):
        dst.write("{}{} {}".format(indent, td.type, name))
    elif isinstance(td,TupleTypedDataSpec):
        dst.write("{}struct {{\n".format(indent))
        for e in td.elements_by_index:
            render_typed_data_as_type_decl(e,dst,indent+"  ")
            dst.write(";\n")
        dst.write("{}}} {}".format(indent,name))
    elif isinstance(td,ArrayTypedDataSpec):
        render_typed_data_as_type_decl(td.type,dst,indent,name="")
        dst.write("{}[{}]".format(name, td.length))
    else:
        raise RuntimeError("Unknown data type.");

def render_typed_data_as_struct(td,dst,name):
    if td:
        dst.write("typedef \n")
        render_typed_data_as_type_decl(td,dst,"  ",name=name)
        dst.write(";\n")
    else:
        dst.write("typedef struct {{}} {};\n".format(name))

def render_graph_type_as_softswitch_decls(gt,dst):
    gtProps=make_graph_type_properties(gt)
    dst.write("""
    #ifndef {GRAPH_TYPE_ID}_hpp
    #define {GRAPH_TYPE_ID}_hpp
    
    #include <cstdint>
    
    #include "softswitch.hpp"
    
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
    print(devProps)
    
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
        if i!=0:
            dst.write("  ,\n");
        dst.write("""
            {{
                (receive_handler_t){INPUT_PORT_FULL_ID}_receive_handler,
                sizeof(packet_t)+sizeof({INPUT_PORT_MESSAGE_T}),
                sizeof({INPUT_PORT_PROPERTIES_T}),
                sizeof({INPUT_PORT_STATE_T})
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
                sizeof(packet_t)+sizeof({OUTPUT_PORT_MESSAGE_T})
            }}
            """.format(**make_output_port_properties(op)))
    dst.write("};\n");
    
    

def render_graph_type_as_softswitch_defs(gt,dst):
    dst.write("""#include "{}.hpp"\n""".format(gt.id))
    
    dst.write("void (*handler_log)(int level, const char *msg, ...) = softswitch_handler_log;\n");
    
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
            (compute_handler_t)0 // No compute handler
        }}
        """.format(**make_device_type_properties(dt)))
    dst.write("};\n")

    
source=sys.stdin
sourcePath="[graph-type-file]"

sys.stderr.write("{}\n".format(sys.argv))

if len(sys.argv)>1:
    if sys.argv[1]!="-":
        sys.stderr.write("Reading graph type from '{}'\n".format(sys.argv[1]))
        source=open(sys.argv[1],"rt")
        sourcePath=os.path.abspath(sys.argv[1])
        sys.stderr.write("Using absolute path '{}' for pre-processor directives\n".format(sourcePath))

(types,instances)=load_graph_types_and_instances(source, sourcePath)

if len(types)!=1:
    raise RuntimeError("File did not contain exactly one graph type.")

graph=None
for g in types.values():
    graph=g
    break


destPrefix="./{}".format(graph.id)

if len(sys.argv)>2:
    sys.stderr.write("Setting graph file output prefix as '{}'\n".format(sys.argv[1]))
    destPrefix=sys.argv[2]

destHppPath=os.path.abspath(destPrefix+".hpp")
destHpp=open(destHppPath,"wt")
sys.stderr.write("Using absolute path '{}' for header pre-processor directives\n".format(destHppPath))

destCppPath=os.path.abspath(destPrefix+"_vtables.cpp")
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
