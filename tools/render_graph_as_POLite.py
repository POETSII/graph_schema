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
