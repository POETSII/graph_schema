from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph

import sys
import os
import math
import random

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/gals_heat_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src)

urand=random.random

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])

assert n>=3

h=1.0/n
alpha=1

dt=h*h / (4*alpha) * 0.5


assert h*h/(4*alpha) >= dt

leakage=1

weightOther = dt*alpha/(h*h)
weightSelf = (1.0 - 4*weightOther)

weightOther = weightOther * leakage

graphType=graphTypes["gals_heat"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]


instName="heat_{}_{}".format(n,n)

properties={"weightSelf":weightSelf, "weightOther":weightOther}

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if x==n//2 and y==n//2:
            props={ "bias":0, "amplitude":1.0, "phase":1.5, "frequency": 100*dt, "neighbours":4 }
            di=DeviceInstance(res,"v_{}_{}".format(x,y), dirichletType, [x,y], props)
            nodes[(x,y)]=di
            res.add_device_instance(di)
        elif edgeX != edgeY:
            props={ "bias":0, "amplitude":1.0, "phase":1, "frequency": 70*dt, "neighbours":1 }
            di=DeviceInstance(res,"v_{}_{}".format(x,y), dirichletType, [x,y], props)
            nodes[(x,y)]=di
            res.add_device_instance(di)
        elif not (edgeX or edgeY):
            props={ "initValue":urand()*2-1 }
            di=DeviceInstance(res,"c_{}_{}".format(x,y), devType, [x,y], props)
            nodes[(x,y)]=di
            res.add_device_instance(di)
            
def add_channel(x,y,dx,dy):
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    ei=EdgeInstance(res,dst,"in", src,"out", None)
    res.add_edge_instance(ei)

for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        sys.stderr.write("   Col : {}\n".format(y))
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if edgeX and edgeY:
            continue

        sys.stderr.write("  {},{}\n".format(x,y))
        if y!=0 and not edgeX:
            sys.stderr.write("     0,-1\n")
            add_channel(x,y, 0, -1)
        if x!=n-1 and not edgeY:
            add_channel(x,y, +1, 0)
        if y!=n-1 and not edgeX:
            add_channel(x,y, 0, +1)
        if x!=0 and not edgeY:
            add_channel(x,y, -1, 0)        
        

save_graph(res,sys.stdout)        
