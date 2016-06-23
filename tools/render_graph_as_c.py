from graph.load_xml import load_graph
from graph.write_c import render_graph_as_c

import sys

graph=load_graph(sys.stdin)

render_graph_as_c(graph, sys.stdout)
