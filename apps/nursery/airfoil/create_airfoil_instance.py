#!/usr/bin/env python3

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph

from graph.metadata import create_device_instance_key

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
        }
    )


    ###################################################################
    ###################################################################
    ##
    ## Create all devices
    
    o_cells=m["cells"]
    o_nodes=m["nodes"]
    o_pcell=m["pcell"]
    o_pedge=m["pedge"]
    o_pbedge=m["pbedge"]
    
    nodes=[
        graph.create_device_instance("n{}".format(n.id),nodeType,
            {"x":n.x,"fanout":0},
            {"loc":[n.x[0],n.x[1]]}
        )
        for n in m["nodes"]
    ]
    
    def calc_hull(id,map):
        nodes=[o_nodes[i] for i in map[id]]
        return [ node.x for node in nodes ]
    
    def calc_loc(hull):
        (xx,yy)=zip(*hull) # Convert from list of pairs to pair of lists
        return [ sum(xx)/len(xx) , sum(yy)/len(yy) ]
    
    def calc_spatial_props(id,map):
        hull=calc_hull(id,map)
        loc=calc_loc(hull)
        return {"hull":hull,"loc":loc}
    
    cells=[
        graph.create_device_instance("c{}".format(c.id),cellType,
            {"id":c.id,"qinit":c.q},
            calc_spatial_props(c.id,o_pcell)
        )
        for c in m["cells"]
    ]
    
    edges=[
        graph.create_device_instance("e{}".format(e.id),edgeType,
            {"id":e.id},
            calc_spatial_props(e.id,o_pedge)
        )
        for e in m["edges"]
    ]
   
    bedges=[
        graph.create_device_instance("be{}".format(be.id),bedgeType,
            {"bound":int(be.bound),"id":be.id},
            calc_spatial_props(be.id,o_pbedge)
        )
        for be in m["bedges"]
    ] 
    
    printer=graph.create_device_instance("print",printerType,{"fanin":len(cells),"delta_print":100,"delta_exit":1000})
    
    
    ###################################################################
    ###################################################################
    ##
    ## Add all edges
    
    cell_edges=m["cell_edges"]
       
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
            graph.create_edge_instance(srcNode,"res_inc_in",dstNode,"res_calc_res{}".format(index+1))
    for (bei, (ci,) ) in enumerate(m["pbecell"]):
        dstNode=bedges[bei]
        srcNode=cells[ci]
        graph.create_edge_instance(dstNode,"q_adt_in",srcNode,"adt_calc")
        graph.create_edge_instance(srcNode,"res_inc_in",dstNode,"bres_calc")
        
    for c in cells:
        graph.create_edge_instance(printer,"rms_inc",c,"update")
        graph.create_edge_instance(c,"rms_ack",printer,"rms_ack")
    
    return graph
    

if __name__=="__main__":
    import mock    
    import argparse

    parser = argparse.ArgumentParser(description='Generate graph for airfoil.')
    parser.add_argument('source', type=str, help='Input hdf5 mesh, or take new_grid.h5 by default', nargs="?", default=appBase+"/new_grid.h5")
    parser.add_argument('-o', dest='output', default="-", help='Where to save the file (- for stdout)')
    args = parser.parse_args()
    
    src=h5py.File(args.source)
    model=mock.load(src)
    
    #model=mock.create_square(3)
    
    graph=render_model(model)
    
    dst=sys.stdout
    if args.output!="-":
        dst=args.output
    save_graph(graph,dst)
