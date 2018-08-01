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

# Command line arguments, keeping it simple for the time being.
parser = argparse.ArgumentParser(description='Render graph as POLite.')
parser.add_argument('source', type=str, help='source file (xml graph instance)')
parser.add_argument('--dest', help="Directory to write the output to.", default=".")
parser.add_argument('--threads', help="number of logical threads to use", type=int, default=3072)

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
