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

w=16
if len(sys.argv)>1:
    w=int(sys.argv[1])    
    
h=int(w*0.75);
if len(sys.argv)>2:
    h=int(sys.argv[2])
    
if(0==(w%2)):
    w=w+1;
if(0==(h%2)):
    h=h+1;
 

assert w>=3 and h>=3

# Each cell is a hexagon of width 2*d.
#
#      |<- d ->|<- d ->|
#
# -                ----|----
# |               /         \             /
# h              /           \           /
# |             /             \         /
# -    +-------+               +-------+
#     /         \             /         \
#    /           \           /           \
#   /             \         /             \
#   \              ----|----
#
# h=cos(pi/6)*d
# volume/hex = 3*d*h  =3*cos(pi/6)*d^2
# area/face = d
#
# The centre of cell (i,j) is at:
#   ( d*(1+2*i+(j%2)), h*(1+j) )
#
#
#   x x x x
#  x + + + x
#   + + + +
#  x + + + x
#   x x x x


dd=1.0/w
hh=math.cos(math.pi*6)

alpha=1
dt=dd*dd / (6*alpha) * 0.5

weightOther = dt*alpha/(h*h)
weightSelf = 1.0 - 6*weightOther

graphType=graphTypes["gals_heat"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]


instName="heat_hex_{}_{}".format(w,h)

properties={"weightSelf":weightSelf, "weightOther":weightOther}

res=GraphInstance(instName, graphType, properties)

nodes={}

assert (w%2)==1
assert (h%2)==1

for y in range(0,h):
    sys.stderr.write(" Devices : Row {} of {}\n".format(y, h))
    for x in range(0,w):
        if (x+y)%2==0:
            continue # No grid point here
        
        edgeX = x==0 or x==w-1
        nearX = x==1 or x==w-2
        edgeY = y==0 or y==h-1
        nearY = y==1 or y==h-2
        
        assert(not(edgeX and edgeY))
        
        (fx,fy) = ( dd*(1+2*x+(y%2)), hh*(1+y) )
            
        if edgeX != edgeY:
            nhood=3 if edgeX else 2
            
            props={ "bias":0, "amplitude":1.0, "phase":1, "frequency": 70*dt*((x/float(w))+(y/float(h))), "neighbours":nhood }
            di=DeviceInstance(res,"v_{}_{}".format(x,y), dirichletType, [fx,fy], props)
            nodes[(x,y)]=di
            res.add_device_instance(di)
            sys.stderr.write("  ({},{})\n".format(x,y))
        elif not (edgeX or edgeY):
            props={ "initValue":urand()*2-1 }
            di=DeviceInstance(res,"c_{}_{}".format(x,y), devType, [fx,fy], props)
            nodes[(x,y)]=di
            res.add_device_instance(di)
            sys.stderr.write("  ({},{})\n".format(x,y))
            
def add_channel(x,y,dx,dy):
    ox=x+dx
    oy=y+dy
    if ox<0 or ox>= w:
        return
    if oy<0 or oy>= h:
        return
    sys.stderr.write("   ({},{}) -> ({},{})\n".format(ox,oy,x,y));
    dst=nodes[ (x,y) ]
    src=nodes[ ( ox,oy ) ]
    ei=EdgeInstance(res,dst,"in", src,"out", None)
    res.add_edge_instance(ei)

for y in range(0,h):
    sys.stderr.write(" Edges : Row {} of {}\n".format( y,h))
    for x in range(0,w):
        sys.stderr.write("    {},{}\n".format(x,y))
        edgeX = x==0 or x==w-1
        edgeY = y==0 or y==h-1
        
        if (x+y)%2==0:
            continue # No grid point here
        #if (edgeX and edgeY):
        #    continue # No corners
        
        if not edgeY:
            add_channel(x,y, -2,0)
            add_channel(x,y, +2,0)
        add_channel(x,y, -1,-1)
        add_channel(x,y, +1,-1)
        add_channel(x,y, +1,+1)
        add_channel(x,y, -1,+1)        

save_graph(res,sys.stdout)        
