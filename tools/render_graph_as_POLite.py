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
         PThread<{},{}>*thread = (Pthread<{},{}>*) tinselHeapBase();
        
         // invoke interpreter
         thread->run();
    """.format(dt.id,mt.id,dt.id,mt.id))

    dst.write("""
      return 0;
    }
    """)

#-----------------------------------------------------------

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

# ----------------------------------------------
# Render Makefile 
# ----------------------------------------------
destMakefilePath=os.path.abspath("{}/Makefile".format(destPrefix))
destMakefile=open(destMakefilePath, "wt")
sys.stderr.write("Using absolute path '{}' for rendered Makefile\n".format(destMakefilePath))
