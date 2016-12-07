from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph

import sys
import os
import math
import random

import scipy.spatial
import numpy as np

import os
appBase=os.path.dirname(os.path.realpath(__file__))

src=appBase+"/gals_heat_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src)

urand=random.random

n=128
if len(sys.argv)>1:
    n=int(sys.argv[1])    
 
# We will generate n random points within the [0,1]^2 square,
# and sqrt(n) on each boundary

sn=math.floor(math.sqrt(n))

border=0.5/sn
scale=1-2*border

def halton(index, base):
    """From wikipedia..."""
    result=0
    f=1
    i=index
    while i>0:
        f = f / base
        result = result + f * (i % base)
        i = i // base
    return result
    
def adjustx(x):
    return math.sqrt(x)
    
def adjusty(y):
    return math.sqrt(y)
    
def adjust(x,y):
    x=adjustx(x)
    y=adjusty(y)
    return [x,y]
    
snx=math.ceil(1.0/adjustx(1.0/sn))
sny=math.ceil(1.0/adjusty(1.0/sn))

#cells=np.array( [ [ urand()*scale+border,urand()*scale+border ] for i in range(0,n) ] )
cells=np.array( [ adjust(halton(i,2)*scale+border, halton(i,3)*scale+border) for i in range(0,n) ] )
left=np.array( [ adjust( 0, i/snx ) for i in range(0,snx) ])   # doesn't include (0,1)
bottom=np.array( [ adjust( i/sn, 1 ) for i in range(0,sn) ]) # doesn't include (1,1)
right=np.array( [ adjust( 1, (i+1)/sn ) for i in range(0,sn) ]) # doesn't include (1,0)
top=np.array( [ adjust( (i+1)/sny, 0 ) for i in range(0,sny) ]) # doesn't include (0,0)

all = np.concatenate( (cells, left, bottom, right, top) );

from scipy.spatial import Voronoi, voronoi_plot_2d

voronoi = Voronoi(all)

import matplotlib.pyplot as plt
voronoi_plot_2d(voronoi)
plt.show()

sys.exit(0)

nodes={}


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
