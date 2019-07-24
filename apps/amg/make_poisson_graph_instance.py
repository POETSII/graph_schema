#!/usr/bin/env python3

import sys
import os
import math
import random

import pyamg
import numpy as np
import scipy.sparse

from make_multigrid import MultiGrid

from graph.core import *

from graph.load_xml import load_graph_types_and_instances
from graph.save_xml_stream import save_graph


def enum_rcv(M):
    coo=M.tocoo()
    for i in range(coo.nnz):
        yield (coo.row[i],coo.col[i],coo.data[i])

def make_poisson_graph_instance(instName,n,m):
    omega=1.0
    iterations=1
    residualTol=1e-3
    
    sys.stderr.write("Creating A\n")
    A = pyamg.gallery.poisson((n,m), format='csr')  # 2D Poisson problem on nxn grid
    
    sys.stderr.write("Creating Grid\n")
    mg=MultiGrid(A, omega=omega, iterations=iterations)

    properties={"residualTol" : residualTol}
    res=GraphInstance(instName, amgGraphType, properties)

    def calc_counts_in(A):
        # xf = A * xc
        # So we want to know how many indices of xc cause xf[dst] to change.
        # Which means we want to know the number of ones in row dst of P
        return A.getnnz(axis=1)

    def add_leaves(level, L):
        leafType=amgGraphType.device_types["leaf"]
        fineNodes=[]
        
        coarse_counts_in=calc_counts_in(L.P)
        peer_counts_in=calc_counts_in(L.A) - 1 # Have to subtract one off to remove diagonal as we don't send to self
        
        # Create all the nodes at this level
        for i in range(L.n):
            diName="leaf_{}_{}".format(level,i)
            props={
                "peerCount": int(peer_counts_in[i]),
                "coarseCount" : int(coarse_counts_in[i]),
                "Ad" : L.A[i,i],
                "AdInvOmega":L.AdiagInvOmega[i],
                "omega":L.omega
            }
            meta=None
            di=DeviceInstance(res, diName, leafType, props, None, meta)
            fineNodes.append(di)
            res.add_device_instance(di)
        
        # Create the next coarser level
        coarseNodes=add_level(level+1)
        
        # Add the fine to coarse
        for (dst,src,R) in enum_rcv(L.R): # Convert to co-ordinate form, for easier enumeration
            props={"R":R}
            ei=EdgeInstance(res, coarseNodes[dst],"fine_up", fineNodes[src],"coarse_up", props)
            res.add_edge_instance(ei)

        # Add the coarse to fine
        for (dst,src,P) in enum_rcv(L.P):
            props={"P":P}
            ei=EdgeInstance(res, fineNodes[dst],"coarse_down", coarseNodes[src],"fine_down", props)
            res.add_edge_instance(ei)
        
        # Add the peers
        for (dst,src,A) in enum_rcv(L.A):
            if dst==src:
                continue  # Skip the diagonal, we dont' send messages to self
            props={"A":A}
            ei=EdgeInstance(res, fineNodes[dst],"peer_in", fineNodes[src],"peer_out", props)
            res.add_edge_instance(ei)
            
        return fineNodes
        
    
    def add_branches(level, L):
        branchType=amgGraphType.device_types["branch"]
        fineNodes=[]
        
        coarse_counts_in=calc_counts_in(L.P)
        peer_counts_in=calc_counts_in(L.A) - 1 # Have to subtract one off to remove diagonal as we don't send to self
        fine_counts_in=calc_counts_in(mg.levels[level-1].R) # Inputs come from R for level below
        
        # Create all the nodes at this level
        for i in range(L.n):
            diName="branch_{}_{}".format(level,i)
            props={
                "fineCount": int(fine_counts_in[i]),
                "peerCount": int(peer_counts_in[i]),
                "coarseCount" : int(coarse_counts_in[i]),
                "Ad" : L.A[i,i],
                "AdInvOmega":L.AdiagInvOmega[i],
                "omega":L.omega
            }
            meta=None
            di=DeviceInstance(res, diName, branchType, props, None, meta)
            fineNodes.append(di)
            res.add_device_instance(di)
        
        # Create the next coarser level
        coarseNodes=add_level(level+1)
        sys.stderr.write("Wiring up level {}\n".format(level))
        
        # Add the fine to coarse
        for (dst,src,R) in enum_rcv(L.R): # Convert to co-ordinate form, for easier enumeration
            props={"R":R}
            ei=EdgeInstance(res, coarseNodes[dst],"fine_up", fineNodes[src],"coarse_up", props)
            res.add_edge_instance(ei)

        # Add the coarse to fine
        for (dst,src,P) in enum_rcv(L.P):
            props={"P":P}
            ei=EdgeInstance(res, fineNodes[dst],"coarse_down", coarseNodes[src],"fine_down", props)
            res.add_edge_instance(ei)
        
        # Add the peers
        for (dst,src,A) in enum_rcv(L.A):
            if dst==src:
                continue  # Skip the diagonal, we dont' send messages to self
            props={"A":A}
            ei=EdgeInstance(res, fineNodes[dst],"peer_in", fineNodes[src],"peer_out", props)
            res.add_edge_instance(ei)
            
        return fineNodes

    
    
    
    def add_root(level, L):
        rootType=amgGraphType.device_types["root"]
        
        fine_counts_in=calc_counts_in(mg.levels[level-1].R)
        
        diName="root_0_0"
        props={
            "fineTotal" : int(fine_counts_in[0]),
            "inv_Adiag" : 1.0 / L.A[0,0]
        }
        meta=None
        di=DeviceInstance(res, diName, rootType, props, None, meta)
        res.add_device_instance(di)
        
        return [di]
            

    def add_level(i):
        sys.stderr.write("add_level {}, len(ml.levels)=={}\n".format(i,len(mg.levels)))
        
        L=mg.levels[i]
        
        if i==0:
            return add_leaves(i, L)
        elif i+1==len(mg.levels):
            return add_root(i,L)
        else:
            return add_branches(i, L)

    fineNodes=add_level(0)
    
    # Now add the tests
    bIn=[]
    xRef=[]
    for i in range(8):
        b=np.random.rand(mg.levels[0].n)
        bIn.append(b)
        
        #sys.stderr.write("b={}\n".format(b))
        xt=np.ones(A.shape[0])*5
        for i in range(10):
            preR=b-A*xt
            mg.hcycle(xt,b)
            #sys.stderr.write("iter = {}, r={}\n".format(i,np.linalg.norm(b-A*xt)))
        
        x=mg.ml.solve(b,tol=residualTol)
        xRef.append(x)
    
    exitType=amgGraphType.device_types["exit"]
    exitNode=DeviceInstance(res, "exit_node", exitType, {"nodes":int(mg.levels[0].n)})
    res.add_device_instance(exitNode)

    testerType=amgGraphType.device_types["tester"]
    for i in range(mg.levels[0].n):
        props={
            "x" : [x[i] for x in xRef],
            "b" : [b[i] for b in bIn]
        }
        di=DeviceInstance(res, "tester_{}".format(i), testerType, props)
        res.add_device_instance(di)
        
        ei=EdgeInstance(res, di,"solution", fineNodes[i],"solution", None)
        res.add_edge_instance(ei)
        
        ei=EdgeInstance(res, fineNodes[i],"problem", di,"problem", None)
        res.add_edge_instance(ei)

        ei=EdgeInstance(res, exitNode,"finished", di, "finished", None)
        res.add_edge_instance(ei)
    
    return res
        
if __name__ == "__main__":        
    appBase=os.path.dirname(os.path.realpath(__file__))

    src=appBase+"/amg_graph_type.xml"
    (graphTypes,graphInstances)=load_graph_types_and_instances(src,src)

    amgGraphType=graphTypes["amg"]
    
    n=2
    m=1
    if len(sys.argv)>1:
        n=int(sys.argv[1])
    if len(sys.argv)>2:
        m=int(sys.argv[2])
    
    res=make_poisson_graph_instance("amg_poisson_{}x{}".format(n,m),n,m)
    
    save_graph(res,sys.stdout)        

