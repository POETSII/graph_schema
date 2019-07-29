#!/usr/bin/env python3

from graph.load_xml import load_graph
from graph.save_xml_v4 import save_graph
from lxml import etree

import sys
import os

source=sys.stdin
sourcePath="[XML-file]"

dest=sys.stdout

if len(sys.argv) < 2:
    raise RuntimeError("This converts exactly one XML file. Please provide one path to an XML file")

source=sys.argv[1]
sys.stderr.write("Reading graph type from '{}'\n".format(source))
sourcePath=os.path.abspath(source)

if len(sys.argv)>2:
    dest=sys.argv[2]

# LOAD XML
(type,instance)=load_graph(source, sourcePath)

if instance:
    save_graph(instance, dest)
else:
    save_graph(type, dest)
