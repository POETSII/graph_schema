from graph.load_xml import load_graph
import sys
import os

shapes=["oval","box","diamond","pentagon","hexagon","septagon", "octagon"]
colors=["blue4","red4","green4"]

deviceTypeToShape={}
edgeTypeToColor={}

graph=load_graph(sys.stdin)

print('digraph "{}"{{'.format(graph.id))
print('  sep="+40,40";');
print('  overlap=scalexy;');

if len(graph.graph_type.device_types) <= len(shapes):
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes.pop(0)
else:
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes[0]

if len(graph.graph_type.edge_types) <= len(colors):
    for id in graph.graph_type.edge_types.keys():
        edgeTypeToColor[id]=colors.pop(0)
else:
    for id in graph.graph_type.edge_types.keys():
        edgeTypeToColor[id]="black"

for di in graph.device_instances.values():
    shape=deviceTypeToShape[di.device_type.id]
    pos=di.native_location
    if pos:
        print('  "{}" [shape={},pos="{},{}"];'.format(di.id, shape, pos[0], pos[1]))
    else:
        print('  "{}" [shape={}];'.format(di.id, shape))

addLabels=len(graph.edge_instances) < 100

for ei in graph.edge_instances.values():
    color=edgeTypeToColor[ei.edge_type.id]
    if addLabels:
        print('  "{}" -> "{}" [ headlabel="{}", taillabel="{}", label="{}", color="{}" ];'.format(ei.src_device.id,ei.dst_device.id,ei.dst_port.name,ei.src_port.name, ei.edge_type.id, color ))
    else:
        print('  "{}" -> "{}" [ color="{}" ];'.format(ei.src_device.id,ei.dst_device.id, color ))

print("}")
