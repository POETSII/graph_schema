#/usr/bin/python

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml import save_graph

import sys
import os
import math
import random
import os

sourceFile=sys.argv[1]



class Patch:
    def __init__(self,line):
        parts=[x.strip() for x in line.split(",")]
        assert len(parts)==14, "len=={}, line={}".format(len(parts),line)
        self.id=parts[0]
        self.centre=[float(x) for x in parts[1:3]]
        self.area=float(parts[3])
        self.length=float(parts[4])
        points=parts[5:11]
        self.points=[ (float(points[i*2]),float(points[i*2+1])) for i in range(0,3) ]
        neighbours=parts[11:14]
        self.neighbours=[ n for n in neighbours if n!="_" ]
        
        

with open(sourceFile,"rt") as source:
    sourceLines=source.readlines()

patches=[Patch( line ) for line in sourceLines]
patches={p.id:p for p in patches}
for p in patches.values():
    p.neighbours=[ patches[id] for id in p.neighbours ]
    assert(len(p.neighbours)>=2), "Triangle {} as {} neighbours".format(p.id, len(p.neighbours))


appBase=os.path.dirname(os.path.realpath(__file__))
src=appBase+"/../gals_heat_graph_type.xml"
(graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

h2=sum( p.area for p in patches.values() ) / len(patches)
alpha=1

dt=h2 / (4*alpha) * 0.5


assert h2/(4*alpha) >= dt

leakage=1

weightOther = dt*alpha/h2
weightSelf = (1.0 - 4*weightOther)

weightOther = weightOther * leakage

graphType=graphTypes["gals_heat"]
devType=graphType.device_types["cell"]
dirichletType=graphType.device_types["dirichlet_variable"]

instName="heat_mesh"

urand=random.random


graphProperties={}
graphMetadata={"location.dimensions" : 2}
res=GraphInstance(instName, graphType, graphProperties, graphMetadata)

nodes={}
for p in patches.values():
    props={ "iv":urand()*2-1, "nhood":len(p.neighbours), "wSelf":weightSelf }
    hull=[ [ps[0],ps[1]] for ps in p.points ]
    meta={"x":p.centre[0], "y":p.centre[1], "hull":hull}
    di=DeviceInstance(res,p.id, devType, props, meta)
    nodes[p.id]=di
    res.add_device_instance(di)

for p in patches.values():
    for n in p.neighbours:
        props={"w":weightOther}
        ei=EdgeInstance(res,nodes[p.id],"in", nodes[n.id],"out", props)
        res.add_edge_instance(ei)

save_graph(res,sys.stdout)        
