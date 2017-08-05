#!/usr/bin/env python3

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon
from matplotlib.collections import PatchCollection

import numpy as np

import h5py
import math
import sys

def plot_model(model,path=None):
    
    nodes=model["nodes"]
    cells=model["cells"]
    pcell=model["pcell"]
    
    
    fig, ax = plt.subplots()

    (minX,maxX,minY,maxY)=(nodes[0].x[0],nodes[0].x[0],nodes[0].x[1],nodes[0].x[1])
    for n in nodes:
        minX=min(minX,n.x[0])
        maxX=max(maxX,n.x[0])
        minY=min(minY,n.x[1])
        maxY=max(maxY,n.x[1])

    patches=[]
    colors=[]
    qx=[]
    qy=[]
    qu=[]
    qv=[]
    for c in cells:
        quad=np.array([ nodes[ni].x for ni in pcell[c.id] ])
        patches.append( Polygon(quad,True) )
        colors.append(c.q[0])
        cx=sum(quad[:,0])/4
        cy=sum(quad[:,1])/4
        qx.append(cx)
        qy.append(cy)
        qu.append(c.q[1])
        qv.append(c.q[2])
        
    maxLen=max( [math.sqrt(qu[i]**2+qv[i]**2) for i in range(len(qu))])
    
    p = PatchCollection(patches, alpha=0.4)
    p.set_array(np.array(colors))
    ax.add_collection(p)
    ax.set_xlim(-2,+4)
    ax.set_ylim(-3,+3)
    fig.colorbar(p, ax=ax)
  
    plt.quiver(qx,qy,qu,qv)

    if path==None:
        plt.show()
    else:
        plt.savefig(path)
        plt.close()


if __name__=="__main__":
    import mock

    src=h5py.File("new_grid.h5")

    model=mock.load(src)
    plot_model(model)
