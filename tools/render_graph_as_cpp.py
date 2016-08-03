from graph.load_xml import load_graph_types_and_instances
from graph.write_cpp import render_graph_as_cpp

import sys

(types,instances)=load_graph_types_and_instances(sys.stdin)

if len(types)!=1:
    raise RuntimeError("File did not contain exactly one graph type.")

graph=None
for g in types.values():
    graph=g
    break

render_graph_as_cpp(graph, sys.stdout)
