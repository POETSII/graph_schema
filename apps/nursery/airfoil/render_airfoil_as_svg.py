#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph

import math
import sys
import os
import random
import colorsys

import svgwrite

appBase=os.path.dirname(os.path.realpath(__file__))

def hsv_to_rgb(h,s,v):
    (r,g,b)=colorsys.hsv_to_rgb(h,s,v)
    return "rgb({},{},{})".format(int(r*255),int(g*255),int(b*255))

def render_model(graph,dest,no_edges=False,no_vectors=False,no_nodes=False):
    dwg = svgwrite.Drawing(filename=dest,debug=True)
    
    cellVolumes=dwg.add(dwg.g(id="cell-volumes"))
    cellVectors=dwg.add(dwg.g(id="cell-vectors"))
    
    edgeLines=dwg.add(dwg.g(id="edge-lines"))
    
    nodeCircles=dwg.add(dwg.g(id="node-circles"))
   
    def line_len(x0,x1):
        dx=x0[0]-x1[0]
        dy=x0[1]-x1[1]
        return math.sqrt(dx*dx+dy*dy)

    def hull_len_area(hull):
        (x0,x1,x2,x3)=hull
        (d01,d12,d23,d30)=(line_len(x0,x1),line_len(x1,x2),line_len(x2,x3),line_len(x3,x0))
        # Area of triangle x0,x1,x2 -> d01,d12,d20
        d20=line_len(x2,x0)
        s=(d01+d12+d20)/2
        a1=math.sqrt(s*(s-d01)*(s-d12)*(s-d20))
        # Area of triangle x0,x2,x3 -> d02,d23,d30
        d02=d20
        s=(d02+d23+d30)/2
        a2=math.sqrt(s*(s-d02)*(s-d23)*(s-d30))
        length=d01+d12+d23+d30
        area=a1+a2
        return (length,area)
                
    
    minX=math.inf
    minY=math.inf
    maxX=-math.inf
    maxY=-math.inf
    
    for di in graph.device_instances.values():
        metadata=di.metadata
        if metadata and ("hull" in metadata):
            for (x,y) in metadata["hull"]:
                minX=min(minX,x)
                maxX=max(maxX,x)
                minY=min(minY,y)
                maxY=max(maxY,y)
    
    
    vector_width=0.02
    vector_circle_radius=0.1
    
    edge_width=0.04
    
    node_circle_radius=0.01
   
    
    
    for di in graph.device_instances.values():
        if di.device_type.id=="cell":
            hull=di.metadata["hull"]
            loc=di.metadata["loc"]
            
            (length,area)=hull_len_area(hull)
            #print(length,area)
            
            pressure=random.random()
            cellVolumes.add(
                dwg.polygon(id=di.id+"-vol",points=hull,fill=hsv_to_rgb(pressure*0.3,0.7,1) )
            )
            
            if not no_vectors:
                angle=random.random()*360
                length=random.random()*10
                vector=cellVectors.add(dwg.g(transform="translate({} {})".format(loc[0],loc[1])))
                vector.add(
                    dwg.line(id=di.id+"-vec",start=(0,0),end=(0,1),stroke="green",stroke_width=vector_width*math.sqrt(area),transform="rotate({}) scale(1,{})".format(angle,length))
                )
                vector.add(
                    dwg.circle(center=(0,0), r=vector_circle_radius*math.sqrt(area), fill="green")
                )
            
    if not no_edges:
        for di in graph.device_instances.values():
            if di.device_type.id=="edge" or di.device_type.id=="bedge":
                hull=di.metadata["hull"]
                dx=hull[1][0]-hull[0][0]
                dy=hull[1][1]-hull[0][1]
                length=math.sqrt(dx*dx+dy*dy)
                edgeLines.add(
                    dwg.line(id=di.id+"-line",start=hull[0],end=hull[1],stroke="blue",stroke_width=edge_width*length)
                )
                edgeLines.add(
                    dwg.line(id=di.id+"-line",start=hull[0],end=[hull[0][0]*0.8+hull[1][0]*0.2,hull[0][1]*0.8+hull[1][1]*0.2] ,stroke="black",stroke_width=length*edge_width*2)
                )

    if not no_nodes:
        for di in graph.device_instances.values():
            if di.device_type.id=="node":
                loc=di.metadata["loc"]
                nodeCircles.add(
                    dwg.circle(id=di.id+"-circle",center=loc,r=node_circle_radius,fill="red")
                )
                
    
    dwg.viewbox(minX,minY,maxX-minX,maxY-minY)
    dwg.save()
    

if __name__=="__main__":
    import mock    
    import argparse

    parser = argparse.ArgumentParser(description='Generate graph for airfoil.')
    parser.add_argument('source', type=str, help='Input xml file.')
    parser.add_argument('-o', dest='output', default="airfoil.svg", help='Where to save the file')
    parser.add_argument('--no-vectors',default=False,action="store_true")
    parser.add_argument('--no-edges',default=False,action="store_true")
    parser.add_argument('--no-nodes',default=False,action="store_true")
    args = parser.parse_args()
    
    (types,instances)=load_graph_types_and_instances(args.source,args.source)
    if len(instances)!=1:
        raise "Expected exactly one instance"
    for i in instances.values():
        graph=i
        break
    
    render_model(graph,args.output, no_vectors=args.no_vectors, no_edges=args.no_edges, no_nodes=args.no_nodes)
    
