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

import argparse


#-----------------------------------------------------------
# For rendering types.. adapted from render_graph_as_softswitch
def render_typed_data(td,dst,indent,prefix, name=None):
    if name==None:
        name=td.name
    assert(name!="_")
    if isinstance(td,ScalarTypedDataSpec):
        dst.write("{0}{2} {1}{3} /*indent={0},type={2},name={3}*/".format(indent, prefix, td.type, name))
    elif isinstance(td,TupleTypedDataSpec):
        for e in td.elements_by_index:
            render_typed_data(e,dst,indent+"  ", prefix)
            dst.write(";\n")
    elif isinstance(td,ArrayTypedDataSpec):
        render_typed_data(td.type,dst,indent,prefix,name="")
        dst.write("{}[{}] /*{}*/".format(name, td.length,name))
    else:
        raise RuntimeError("Unknown data type {}".format(td));

#-----------------------------------------------------------
# returns the single message type for this POLite compatible XML
def getMessageType(graph):
    for mt in graph.message_types.values():
        if not mt.id == "__init__":
            return mt    

#-----------------------------------------------------------
# returns the single device type for this POLite compatible XML
def getDeviceType(graph):
    for dt in graph.device_types.values():
        return dt

#-----------------------------------------------------------
# renders the Cpp file for the POLite executable
def renderCpp(dst, graph):
    dst.write("""
    #include <tinsel.h>
    #include <POLite.h>
    #include "{}.h"

    int main()
    {{
    """.format(graph.id))
 
    dt = getDeviceType(graph)
    mt = getMessageType(graph)

    dst.write("""
         // Point thread structure at base of thread's heap
         PThread<__devicetype_{},__messagetype_{}>*thread = (PThread<__devicetype_{},__messagetype_{}>*) tinselHeapBase();
        
         // invoke interpreter
         thread->run();
    """.format(dt.id,mt.id,dt.id,mt.id))

    dst.write("""
      return 0;
    }
    """)

#-----------------------------------------------------------

#-----------------------------------------------------------
# render the properties header file
def renderPropertiesHeader(dst, graph, inst):
    # get the device type and it's properties 
    dt = getDeviceType(graph)
    dtProps=make_device_type_properties(dt)

    # create a struct type for holding the device type properties (this should probably be const)
    dst.write("#ifndef _DEV_PROPERTIES_H\n")
    dst.write("#define _DEV_PROPERTIES_H\n")
    dst.write("""
    typedef struct __devicetype_{}_properties {{
    """.format(dt.id))
    render_typed_data(dt.properties, dst, " ","", dtProps["DEVICE_TYPE_PROPERTIES_T"])
    dst.write("""}} __devicetype_{}_properties_t; 
    \n""".format(dt.id))

    # build an array of the properties type. Each entry in the array corrosponds to a device instance
    # get the number of device instances in the graph
    total_dev_i = len(inst.device_instances)
    dst.write("    __devicetype_{0}_properties_t __devicetype_{0}_properties[{1}] = {{\n".format(dt.id,total_dev_i))
    for i,di in enumerate(inst.device_instances.values()):
        dst.write("{\n")
        if di.properties:
            for key, value in di.properties.items():
                dst.write("\t{},//{}\n".format(value, key))
        if i == total_dev_i-1: 
            dst.write("}\n")
        else:
            dst.write("},\n")
    dst.write("    }}; /* __devicetype_{}_properties_t */\n\n".format(dt.id)) 

    dst.write("#endif /*_DEV_PROPERTIES_H */")

#-----------------------------------------------------------
# renders the header file for the POLite executable
def renderHeader(dst, graph):
    mt = getMessageType(graph)
    dt = getDeviceType(graph)
    # start with a header guard and include
    dst.write("""
    #ifndef _{0}_H_
    #define _{0}_H_

    #include <POLite.h>
    //#include "{0}_properties.h"\n
    """.format(graph.id))

    # --------------------------------------------------
    # define the output pin send flags
    for op in dt.outputs.values(): 
        dst.write("#define RTS_FLAG_{} 1\n".format(op.name))
        dst.write("    #define OUTPUT_FLAG_{}_{} 1\n".format(dt.id,op.name))

    # ----------------------------------------------------
    # instantiate the message type
    mtProps = make_message_type_properties(mt)
    dst.write("""\n
    struct __messagetype_{} : PMessage {{
       // message params here
    """.format(mt.id))
    render_typed_data(mt.message, dst, " ","", mtProps['MESSAGE_TYPE_T'])
    
    dst.write("""
    };
  
    """)
    # ----------------------------------------------------

    # ----------------------------------------------------
    # instantiate the device type
    dst.write("""
    struct __devicetype_{} : PDevice {{
    // internal
    uint32_t multicast_progress; // tracks how far through the broadcast we are
    //{} multicast_msg; // keep track of the last message for multicasting
    bool *doSend; // stub for send cancelation
    // properties 
    """.format(dt.id, mt.id))
    # instantiate the device properties
    if dt.properties:
        dtProps=make_device_type_properties(dt)
        render_typed_data(dt.properties, dst, " ", "__props_", dtProps["DEVICE_TYPE_PROPERTIES_T"])
    # instantiate the graph properties
    if graph.properties:
        gtProps=make_graph_type_properties(graph)
        render_typed_data(graph.properties, dst, "", "__graph_props_", gtProps["GRAPH_TYPE_PROPERTIES_T"]) 
    #dst.write("""
    #{}_properties_t *deviceProperties;
    #""".format(dt.id))
    dst.write("    // state\n")
    # instantiate the device state 
    render_typed_data(dt.state, dst, " ", "__state_", dtProps["DEVICE_TYPE_STATE_T"])
    
    # build the handler_exit function call
    dst.write("""
    void handler_exit(int code) {
      dest = hostDeviceId(); // get the host Id
      readyToSend = 1; // send a message to it (at the moment all messages to the host cause termination
    }\n
    """)

    # build the rtsHandler
    dst.write("""

    // the ready to send handler 
    void rtsHandler() {
       // if we are done with a multicast then we can do a regular send
       if(multicast_progress < fanOut) {
            readyToSend = 1;
       } else { 
    """)
    readytosend_handler = dt.ready_to_send_handler.replace("deviceState->", "__state_")
    readytosend_handler = readytosend_handler.replace("deviceProperties->", "__props_")
    readytosend_handler = readytosend_handler.replace("graphProperties->", "__graph_props_")
    readytosend_handler = readytosend_handler.replace("*readyToSend", "readyToSend")
    dst.write(readytosend_handler)
    dst.write("\n\t}\n\t}\n")
  

    # build the init handler
    dst.write("""

    // Called once by POLite at the start of execution
    void init() {
        multicast_progress=fanOut;
    """)
    #dst.write(""" 
    #deviceProperties = &{}_properties[thisDeviceId()]; 
    #""".format(dt.id)) # getting a pointer to the properties 
    for ip in dt.inputs.values():
        if ip.name == "__init__": 
            init_handler = ip.receive_handler.replace("deviceState->", "__state_")
            init_handler = init_handler.replace("deviceProperties->", "__props_")
            init_handler = init_handler.replace("graphProperties->", "__graph_props_")
            dst.write(init_handler)
    dst.write("""
               rtsHandler();    
             }\n""")

    # build the send handler
    dst.write("""
    // Send handler
    inline void send(__messagetype_{}* msg) {{
        if(multicast_progress < fanOut) {{
            //msg = &multicast_msg;
            dest = outEdge(multicast_progress);
            multicast_progress++; 
        }} else {{
          multicast_progress=0;
    """.format(mt.id))
    for op in dt.outputs.values(): # There should only be one of these
        send_handler = op.send_handler.replace("deviceState->","__state_")
        send_handler = send_handler.replace("deviceProperties->","__props_")
        send_handler = send_handler.replace("graphProperties->","__graph_props_")
        send_handler = send_handler.replace("message->","msg->")
        dst.write(send_handler)
    dst.write("""
                   //multicast_msg = *msg; // copy the message
                   dest = outEdge(multicast_progress);
                   multicast_progress++;
               }
        rtsHandler();    
        }\n""")

    # build the receive handler
    dst.write("""
    // recv handler
    inline void recv(__messagetype_{}* msg) {{
    """.format(mt.id))
    for ip in dt.inputs.values():
        if ip.name != "__init__":
            recv_handler = ip.receive_handler.replace("deviceState->","__state_")
            recv_handler = recv_handler.replace("deviceProperties->","__props_")
            recv_handler = recv_handler.replace("graphProperties->","__graph_props_")
            recv_handler = recv_handler.replace("message->", "msg->")
            dst.write(recv_handler)
    dst.write("""
               rtsHandler();   
             }\n""")

    dst.write("""
    };
    """)
    # ----------------------------------------------------

    dst.write("""
    #endif /* _{}_H_ */
    """.format(graph.id))
#-----------------------------------------------------------


#-----------------------------------------------------------
# renders the Cpp host file
def renderHostCpp(dst,graph,inst):
    dt = getDeviceType(graph)
    mt = getMessageType(graph)
    total_dev_i = len(inst.device_instances)

    # all the include files
    dst.write("""
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <sys/time.h>
    #include <HostLink.h>
    #include <POLite.h>
    #include "{}.h"\n
    """.format(graph.id))

    #the main function
    dst.write("""
    int main()
    {{

      // Connection to tinsel machine
      HostLink hostLink;

      // Create POETS graph
      PGraph<__devicetype_{0}, __messagetype_{1}> graph;

    """.format(dt.id, mt.id, total_dev_i))

    # instantiate the devices
    dst.write("  // Create the device instances\n") 
    for di in inst.device_instances.values():
        dst.write("      PDeviceId {}_devI = graph.newDevice();\n".format(di.id)) 
    dst.write("\n")

    # instantiate the edges
    dst.write("  // Create the edge instances\n")
    for ei in inst.edge_instances.values():
        edge_src = ei.src_device
        edge_dst = ei.dst_device
        dst.write("     graph.addEdge({}_devI,{}_devI);\n".format(edge_src.id, edge_dst.id)) 

    # place the graph
    dst.write("    graph.map();\n")

    # setup the device properties
    for di in inst.device_instances.values():
        dst.write("  // properties for devI={}\n".format(di.id))
        if di.properties:
            for key, value in di.properties.items():
                dst.write("    graph.devices[{}_devI]->__props_{}={};\n".format(di.id, key, value))
        # instantiate the graph properties
        dst.write("  \t//graph properties\n".format(di.id))
        if inst.properties:
            for key, values in inst.properties.items():
                dst.write("    graph.devices[{}_devI]->__graph_props_{}={};\n".format(di.id, key, value))

    # end of the main function
    dst.write("""
    
    // Write graph does to tinsel machine via hostlink
    graph.write(&hostLink);

    // Load code and trigger execution
    hostLink.boot("code.v", "data.v");

    // Get start time
    struct timeval start, finish, diff;
    gettimeofday(&start, NULL);

    // Trigger execution
    hostLink.go();

    // Wait for handler_exit
    uint32_t resp[4];
    hostLink.recv(resp);
    
    // Get finish time
    gettimeofday(&finish, NULL);

    // Display time
    timersub(&finish, &start, &diff);
    double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
    printf("Time = %lf\\n", duration);

    return 0;
    }
    """)

#-----------------------------------------------------------


#-----------------------------------------------------------
# renders the makefile that is used to build the POLite project
def renderMakefile(dst, graph):
   dst.write("""
# Tinsel root
TINSEL_ROOT=/home/sf306/poets-ecosystem/submodules/tinsel
HL=$(TINSEL_ROOT)/hostlink

ifndef QUARTUS_ROOTDIR
\t$(error Please set QUARTUS_ROOTDIR)
endif

include $(TINSEL_ROOT)/globals.mk

# local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0

.PHONY: all
all: code.v data.v run

code.v: {0}.elf
\tcheckelf.sh {0}.elf
\t$(RV_OBJCOPY) -O verilog --only-section=.text {0}.elf code.v

data.v: {0}.elf
\t$(RV_OBJCOPY) -O verilog --remove-section=.text \\
\t\t--set-section-flags .bss=alloc,load,contents {0}.elf data.v

{0}.elf: {0}.cpp {0}.h link.ld $(INC)/config.h $(INC)/tinsel.h entry.o $(LIB)/lib.o
\t$(RV_CPPC) $(CFLAGS) -Wall -c -DTINSEL -o {0}.o {0}.cpp
\t$(RV_LD) $(LDFLAGS) -T link.ld -o {0}.elf entry.o {0}.o $(LIB)/lib.o

entry.o:
\t$(RV_CPPC) $(CFLAGS) -Wall -c -o entry.o entry.S

$(LIB)/lib.o:
\tmake -C $(LIB)

link.ld: genld.sh
\t./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
\tmake -C $(LIB)

$(HL)/%.o:
\tmake -C $(HL)

run: {0}_host.cpp $(HL)/*.o
\tg++ -O2 -I $(INC) -I $(HL) -o run {0}_host.cpp $(HL)/*.o -ljtag_atlantic -ljtag_client -L $(QUARTUS_ROOTDIR)/linux64/ -Wl,-rpath,$(QUARTUS_ROOTDIR)/linux64 -lmetis

clean:
\trm -f *.o *.elf link.ld *.v run sim
   """.format(graph.id)) 

#-----------------------------------------------------------
# returns true if we have a POLite compatible subset of the XML -- otherwise returns false
def polite_compatible_xml_subset(gt):
    compatible=True
    # we can only have 1 device type
    if len(gt.device_types)!=1:
        sys.stderr.write("XML has more that one device type -- this is not currently compatible\n")
        compatible=False
    
    # each device can only have one input pin and one output pin
    for dt in gt.device_types.values():
        if len(dt.inputs) > 2:
            sys.stderr.write("device type {} has more than 1 input pin -- this is not currently supported\n".format(dt.id))
            compatible=False
        elif len(dt.inputs) == 2: # one of these must be an __init__ pin
            init = False
            for ip in dt.inputs.values():
                if ip.name == "__init__":
                    init=True
            if not init:
                sys.stderr.write("device type {} has two pins but one is not an __init__ pin\n".format(dt.id))
                compatible=False
        if len(dt.outputs)!=1:
            sys.stderr.write("device type {} has more than 1 output pin -- this is not currently supported\n".format(dt.id))
            compatible=False

    return compatible
#-----------------------------------------------------------

# Command line arguments, keeping it simple for the time being.
parser = argparse.ArgumentParser(description='Render graph as POLite.')
parser.add_argument('source', type=str, help='source file (xml graph instance)')
parser.add_argument('--dest', help="Directory to write the output to.", default=".")
parser.add_argument('--threads', help="number of logical threads to use (Not currently configurable)", type=int, default=3072)
parser.add_argument('--output', help="name of the executable binary that runs on the POETS hardware", type=str, default="out")

args = parser.parse_args()

# Load in the XML graph description
if args.source=="-":
    source=sys.stdin
    sourcePath="[graph-type-file]"
else:
    sys.stderr.write("Reading graph type from '{}'\n".format(args.source))
    source=open(args.source,"rt")
    sourcePath=os.path.abspath(args.source)
    sys.stderr.write("Using absolute path '{}' for pre-processor directives\n".format(sourcePath))

(types, instances)=load_graph_types_and_instances(source, sourcePath)

# ---------------------------------------------------------------------------
# Do we have a POLite compatible subset of the XML?
# ---------------------------------------------------------------------------
if len(types)!=1:
    raise RuntimeError("File did not contain exactly one graph type.")

graph=None # graph the one graph
for g in types.values():
    graph=g
    break

# Currently there has to be exactly 1 graph instance in the XML for it to be rendered
assert(len(instances)==1)

inst=None
for g in instances.values():
    inst = g
    break

assert(inst.graph_type.id==graph.id)

if not polite_compatible_xml_subset(graph):
    raise RuntimeError("This graph type contains features not currently supported by this backend.")


# ---------------------------------------------------------------------------
# Start rendering POLite output 
# ---------------------------------------------------------------------------
destPrefix=args.dest

# ----------------------------------------------
# Render header file for tinsel executable 
# ----------------------------------------------
destHPath=os.path.abspath("{}/{}.h".format(destPrefix,graph.id))
destH=open(destHPath, "wt")
sys.stderr.write("Using absolute path '{}' for rendered POLite header file\n".format(destHPath))
renderHeader(destH, graph)

# ----------------------------------------------
# Render header file for device properties 
# ----------------------------------------------
destHPropPath=os.path.abspath("{}/{}_properties.h".format(destPrefix, graph.id))
destProp=open(destHPropPath, "wt")
sys.stderr.write("Using absolute path '{}' for header file containing device properites\n".format(destHPropPath))
renderPropertiesHeader(destProp,graph,inst)

# ----------------------------------------------
# Render source file for tinsel executable 
# ----------------------------------------------
destCppPath=os.path.abspath("{}/{}.cpp".format(destPrefix,graph.id))
destCpp=open(destCppPath, "wt")
sys.stderr.write("Using absolute path '{}' for rendered POLite source file\n".format(destCppPath))
renderCpp(destCpp,graph)

# ----------------------------------------------
# Render host executable 
# ----------------------------------------------
destHostCppPath=os.path.abspath("{}/{}_host.cpp".format(destPrefix,graph.id))
destHostCpp=open(destHostCppPath, "wt")
sys.stderr.write("Using absolute path '{}' for rendered POLite host program source file\n".format(destHostCppPath))
renderHostCpp(destHostCpp,graph,inst)

# ----------------------------------------------
# Render Makefile 
# ----------------------------------------------
destMakefilePath=os.path.abspath("{}/Makefile".format(destPrefix))
destMakefile=open(destMakefilePath, "wt")
sys.stderr.write("Using absolute path '{}' for rendered Makefile\n".format(destMakefilePath))
renderMakefile(destMakefile, graph)
