from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph
import sys
import os
import math

n=256

src=sys.argv[1]
(graphTypes,graphInstances)=load_graph_types_and_instances(src)

graphType=graphTypes["heat"]
devType=graphType.device_types["region"]


instName="heat_{}_{}".format(n,n)

properties=TupleData(instName+"_properties",[
    Float32Data("fireThreshold", 1e-6)
    ])

res=GraphInstance(instName, graphType, properties)

nodes={}
conductance={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        isFixed=(x==0 or x==n-1 or y==0 or y==n-1)
        initialTemp=math.sin(x/10)+math.cos(x/17)
        
        devProps=TupleData("device_properties",[
            BoolData("isFixed", isFixed),
            Float32Data("initialTemp", initialTemp)
            ])
        di=DeviceInstance(res,"n_{}_{}".format(x,y), devType, [x,y], devProps)
        nodes[(x,y)]=di
        res.add_device_instance(di)

        conductance[(x,y)] = 0.1
        
def add_conductor(dst,src,conductance):
    edgeProps=TupleData("edge_properties", [
        Float32Data("conductance", conductance)
        ])
    ei=EdgeInstance(res,dst,"in", src,"out", edgeProps)
    res.add_edge_instance(ei)


for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        centre=nodes[(x,y)]
        if x>0:
            add_conductor(centre, nodes[(x-1,y)], conductance[(x-1,y)])
        if x<n-1:
            add_conductor(centre, nodes[(x+1,y)], conductance[(x,y)])
        if y>0:
            add_conductor(centre, nodes[(x,y-1)], conductance[(x,y-1)])
        if y<n-1:
            add_conductor(centre, nodes[(x,y+1)], conductance[(1,y)])
        

save_graph(res,sys.stdout)        
