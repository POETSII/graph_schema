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

def render_model(graph,dest):
    dwg = svgwrite.Drawing(filename=dest,debug=True)
    dwg.viewbox(-10,-8,20,16)
    
    cellVolumes=dwg.add(dwg.g(id="cell-volumes"))
    cellVectors=dwg.add(dwg.g(id="cell-vectors"))
    
    edgeLines=dwg.add(dwg.g(id="edge-lines"))
    
    nodeCircles=dwg.add(dwg.g(id="node-circles"))
    
    print(graph)
    
    vector_width=0.02
    vector_circle_radius=0.05
    
    edge_width=0.04
    
    node_circle_radius=0.05
    
    for di in graph.device_instances.values():
        if di.device_type.id=="cell":
            hull=di.metadata["hull"]
            loc=di.metadata["loc"]
            pressure=random.random()
            cellVolumes.add(
                dwg.polygon(id=di.id+"-vol",points=hull,fill=hsv_to_rgb(pressure*0.3,0.7,1) )
            )
            
            angle=random.random()*360
            length=random.random()*10
            vector=cellVectors.add(dwg.g(transform="translate({} {})".format(loc[0],loc[1])))
            vector.add(
                dwg.line(id=di.id+"-vec",start=(0,0),end=(0,1),stroke="green",stroke_width=vector_width,transform="rotate({}) scale(1,{})".format(angle,length))
            )
            vector.add(
                dwg.circle(center=(0,0), r=vector_circle_radius, fill="green")
            )
        elif di.device_type.id=="edge" or di.device_type.id=="bedge":
            hull=di.metadata["hull"]
            loc=di.metadata["loc"]
            edgeLines.add(
                dwg.line(id=di.id+"-line",start=hull[0],end=hull[1],stroke="blue",stroke_width=edge_width)
            )
            edgeLines.add(
                dwg.line(id=di.id+"-line",start=hull[0],end=[hull[0][0]*0.8+hull[1][0]*0.2,hull[0][1]*0.8+hull[1][1]*0.2] ,stroke="black",stroke_width=edge_width*2)
            )
            
        elif di.device_type.id=="node":
            loc=di.metadata["loc"]
            nodeCircles.add(
                dwg.circle(id=di.id+"-circle",center=loc,r=node_circle_radius,fill="red")
            )
    
    dwg.save()
    

if __name__=="__main__":
    import mock    
    import argparse

    parser = argparse.ArgumentParser(description='Generate graph for airfoil.')
    parser.add_argument('source', type=str, help='Input xml file.')
    parser.add_argument('-o', dest='output', default="airfoil.svg", help='Where to save the file')
    args = parser.parse_args()
    
    (types,instances)=load_graph_types_and_instances(args.source,args.source)
    if len(instances)!=1:
        raise "Expected exactly one instance"
    for i in instances.values():
        graph=i
        break
    
    render_model(graph,args.output)
    
