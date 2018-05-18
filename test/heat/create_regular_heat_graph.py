from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph
import sys
import os
import math



src=sys.argv[1]
(graphTypes,graphInstances)=load_graph_types_and_instances(src)

n=26
if len(sys.argv)>2:
    n=int(sys.argv[2])

graphType=graphTypes["heat"]
devType=graphType.device_types["region"]


instName="heat_{}_{}".format(n,n)

properties={"fireThreshold":1e-6}

res=GraphInstance(instName, graphType, properties)

nodes={}
conductance={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        isFixed=(x==0 or x==n-1 or y==0 or y==n-1)
        initialTemp=math.sin(x/10)+math.cos(x/17)
        
        devProps={"isFixed":isFixed, "initialTemp":initialTemp}
        di=DeviceInstance(res,"n_{}_{}".format(x,y), devType, [x,y], devProps)
        nodes[(x,y)]=di
        res.add_device_instance(di)

        if 0==(x%5) and 0==(x%7):
            conductance[(x,y)]=0.0
        else:
            conductance[(x,y)]=0.05
        
def add_conductor(dst,src,conductance):
    edgeProps={"conductance":conductance}
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
