from graph.load_xml import load_graph
from graph.write_cpp import render_graph_as_cpp

import sys

graph=load_graph(sys.stdin)

render_graph_as_cpp(graph, sys.stdout)
