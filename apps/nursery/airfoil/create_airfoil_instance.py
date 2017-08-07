#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph

import h5py
import math
import sys
import os

appBase=os.path.dirname(os.path.realpath(__file__))

def render_model(m):
    src=appBase+"/airfoil_graph_type.xml"
    (graphTypes,_)=load_graph_types_and_instances(src,src)
    graphType=graphTypes["airfoil"]
    
    nodeType=graphType.device_types["node"]
    cellType=graphType.device_types["cell"]
    edgeType=graphType.device_types["edge"]
    bedgeType=graphType.device_types["bedge"]
    printerType=graphType.device_types["printer"]
    
    graph=GraphInstance("airfoil_inst", graphType, {
        "gam":m["globals"].gam,
        "gm1":m["globals"].gm1,
        "cfl":m["globals"].cfl,
        "eps":m["globals"].eps,
        "mach":m["globals"].mach,
        "alpha":m["globals"].alpha,
        "qinf":m["globals"].qinf,
    })
    
    nodes=[ graph.create_device_instance("n{}".format(n.id),nodeType,{"x":n.x,"fanout":0}) for n in m["nodes"] ]
    cells=[ graph.create_device_instance("c{}".format(c.id),cellType,{"qinit":c.q}) for c in m["cells"] ]
    edges=[ graph.create_device_instance("e{}".format(e.id),edgeType) for e in m["edges"] ]
    bedges=[ graph.create_device_instance("be{}".format(be.id),bedgeType,{"bound":int(be.bound)}) for be in m["bedges"] ] 
    printer=graph.create_device_instance("print",printerType,{"fanin":len(cells),"delta_print":100,"delta_exit":1000})
    
    for (ci,nis) in enumerate(m["pcell"]):
        dstNode=cells[ci]
        for (index,ni) in enumerate(nis):
            srcNode=nodes[ni]
            srcNode.properties["fanout"]+=1
            graph.create_edge_instance(dstNode,"pos_in",srcNode,"pos_out",{"index":index})
            graph.create_edge_instance(srcNode,"ack_in",dstNode,"pos_ack_out")
    for (ci,nis) in enumerate(m["pedge"]):
        dstNode=edges[ci]
        for (index,ni) in enumerate(nis):
            srcNode=nodes[ni]
            srcNode.properties["fanout"]+=1
            graph.create_edge_instance(dstNode,"pos_in",srcNode,"pos_out",{"index":index})
            graph.create_edge_instance(srcNode,"ack_in",dstNode,"pos_ack_out")
    for (ci,nis) in enumerate(m["pbedge"]):
        dstNode=bedges[ci]
        for (index,ni) in enumerate(nis):
            srcNode=nodes[ni]
            srcNode.properties["fanout"]+=1
            graph.create_edge_instance(dstNode,"pos_in",srcNode,"pos_out",{"index":index})
            graph.create_edge_instance(srcNode,"ack_in",dstNode,"pos_ack_out")

    # send (q,adt) from cell to edges that surround it, and send the res back
    for (ei,cis) in enumerate(m["pecell"]):
        dstNode=edges[ei]
        for (index,ci) in enumerate(cis):
            srcNode=cells[ci]
            graph.create_edge_instance(dstNode,"q_adt_in",srcNode,"adt_calc",{"index":index})
            graph.create_edge_instance(srcNode,"res_inc_in",dstNode,"res_calc")
    for (bei, (ci,) ) in enumerate(m["pbecell"]):
        dstNode=bedges[bei]
        srcNode=cells[ci]
        graph.create_edge_instance(dstNode,"q_adt_in",srcNode,"adt_calc")
        graph.create_edge_instance(srcNode,"res_inc_in",dstNode,"res_calc")
        
    for c in cells:
        graph.create_edge_instance(printer,"rms_inc",c,"update")
        graph.create_edge_instance(c,"rms_ack",printer,"rms_ack")
    
    return graph
    

if __name__=="__main__":
    import mock
    

    src=h5py.File(appBase+"/new_grid.h5")

    model=mock.load(src)

    graph=render_model(model)
    
    save_graph(graph,sys.stdout)        
