#!/usr/bin/env python3

from graph.load_xml import load_graph
from graph.save_xml import save_graph
from lxml import etree

import sys
import os

source=sys.stdin
sourcePath="[XML-file]"

if len(sys.argv) != 2:
    raise RuntimeError("This converts exactly one XML file. Please provide one path to an XML file")

sys.stderr.write("Reading graph type from '{}'\n".format(sys.argv[1]))
source=open(sys.argv[1],"rt")
sourcePath=os.path.abspath(sys.argv[1])

dest=sys.stdout
destPath="[XML-file]"

# LOAD XML
(gt,gi)=load_graph(source, sourcePath)
assert gi, "Need a graph instance"

save_graph(gi, dest)