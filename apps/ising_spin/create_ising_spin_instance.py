from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph
import sys
import os
import math



src=sys.argv[1]
(graphTypes,graphInstances)=load_graph_types_and_instances(src)

endTime=1000
T=1
J=1
H=0

n=4
if len(sys.argv)>2:
    n=int(sys.argv[2])
if len(sys.argv)>3:
    T=float(sys.argv[3])
    

graphType=graphTypes["ising_spin"]
devType=graphType.device_types["cell"]



instName="heat_{}_{}".format(n,n)

probs=[0]*10
for i in range(0,5):
    for j in range(0,2):
        index = i + 5*j; #'" index == 0,1,... ,9 "/
        my_spin = 2*j - 1;
        sum_nei = 2*i - 4;
        d_E = 2.*(J * my_spin * sum_nei + H * my_spin);
        x = math.exp(-d_E/T)
        probs[index]=x/(1.+x);
        sys.stderr.write("prob[{}] = {}\n".format(index,probs[index]))

properties={
    "endTime":100,
    "width":n,
    "height":n,
    "probabilities":probs
    }

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        devProps={"x":x, "y":y}
        di=DeviceInstance(res,"n_{}_{}".format(x,y), devType, [x,y], devProps)
        nodes[(x,y)]=di
        res.add_device_instance(di)
        
def add_channel(x,y,dx,dy,dir):
    edgeProps={"direction":dir}
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    ei=EdgeInstance(res,dst,"in", src,"out", edgeProps)
    res.add_edge_instance(ei)


for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        centre=nodes[(x,y)]
        add_channel(x,y, 0, -1, 1)
        add_channel(x,y, +1, 0, 2)
        add_channel(x,y, 0, +1, 3)
        add_channel(x,y, -1, 0, 4)        
        

save_graph(res,sys.stdout)        
