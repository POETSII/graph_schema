from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph

import sys
import os
import math
import random

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/gals_heat_fix_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

urand=random.random

n=16
if len(sys.argv)>1:
    n=int(sys.argv[1])

assert n>=3

h=1.0/n
alpha=1

dt=h*h / (4*alpha) * 0.5


assert h*h/(4*alpha) >= dt

_fix_max=2**31-1
_fix_min=-_fix_max
_fix_scale=2**24
def toFix(x):
    r=int(x*_fix_scale)
    assert _fix_min < r < _fix_max, "Fixed point overflow converting {}, r={}".format(x,r)
    return r

leakage=1

weightOther = dt*alpha/(h*h)
weightSelf = (1.0 - 4*weightOther)
weightSelfFix = toFix(weightSelf)

weightOther = weightOther * leakage
weightOtherFix = toFix(weightOther)

graphType=graphTypes["gals_heat_fix"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]
exitNodeType=graphType.device_types["exit_node"]

instName="heat_{}_{}".format(n,n)

properties={}

res=GraphInstance(instName, graphType, properties)

nodes={}

for x in range(0,n):
    sys.stderr.write(" Devices : Row {} of {}\n".format(x, n))
    for y in range(0,n):
        meta={"loc":[x,y]}
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if x==n//2 and y==n//2:
            props={ "updateDelta":toFix(5.0*dt), "updateMax":toFix(5.0), "neighbours":4 }
            di=DeviceInstance(res,"v_{}_{}".format(x,y), dirichletType, props, meta)
            nodes[(x,y)]=di
            res.add_device_instance(di)
        elif edgeX != edgeY:
            props={ "updateDelta":toFix(10.0*dt*(x/n)), "updateMax":toFix(1.0), "neighbours":1 }
            di=DeviceInstance(res,"v_{}_{}".format(x,y), dirichletType, props, meta)
            nodes[(x,y)]=di
            res.add_device_instance(di)
        elif not (edgeX or edgeY):
            props={ "iv":toFix(urand()*2-1), "nhood":4, "wSelf":weightSelfFix }
            di=DeviceInstance(res,"c_{}_{}".format(x,y), devType, props, meta)
            nodes[(x,y)]=di
            res.add_device_instance(di)
            
def add_channel(x,y,dx,dy):
    dst=nodes[ (x,y) ]
    src=nodes[ ( (x+dx+n)%n, (y+dy+n)%n ) ]
    if dst.device_type.id=="cell":
        props={"w":weightOtherFix}
    else:
        props=None  
    ei=EdgeInstance(res,dst,"in", src,"out", props)
    res.add_edge_instance(ei)

for x in range(0,n):
    sys.stderr.write(" Edges : Row {} of {}\n".format( x, n))
    for y in range(0,n):
        edgeX = x==0 or x==n-1
        edgeY = y==0 or y==n-1
        if edgeX and edgeY:
            continue

        if y!=0 and not edgeX:
            add_channel(x,y, 0, -1)
        if x!=n-1 and not edgeY:
            add_channel(x,y, +1, 0)
        if y!=n-1 and not edgeX:
            add_channel(x,y, 0, +1)
        if x!=0 and not edgeY:
            add_channel(x,y, -1, 0)        

finished=DeviceInstance(res, "finished",exitNodeType, None, None) 
res.add_device_instance(finished)

for (id,di) in nodes.items():
    res.add_edge_instance(EdgeInstance(res,finished,"done",di,"finished"))

save_graph(res,sys.stdout)        
