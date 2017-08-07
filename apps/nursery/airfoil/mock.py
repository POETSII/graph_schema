#!/usr/bin/env python3

import h5py
import math
import sys

class Set(object):
    def __init__(self,id):
        self.id=id
        

class Globals:
    def __init__(self):
        self.gam=0
        self.gm1=0
        self.cfl=0
        self.eps=0
        self.mach=0
        self.alpha=0
        self.qinf=[0]*4

class Node(Set):
    def __init__(self,id):
        super().__init__(id)
        self.x=[0.0,0.0]

class Edge(Set):
    def __init__(self,id):
        super().__init__(id)

    def res_calc(self,g,x1,x2,q1,q2,adt1,adt2):
        # double dx,dy,mu, ri, p1,vol1, p2,vol2, f;

        gm1=g.gm1
        eps=g.eps
        res1=[0.0,0.0,0.0,0.0]
        res2=[0.0,0.0,0.0,0.0]

        dx = x1[0] - x2[0]
        dy = x1[1] - x2[1]
        ri   = 1.0/q1[0]
        p1   = gm1*(q1[3]-0.5*ri*(q1[1]*q1[1]+q1[2]*q1[2]))
        vol1 =  ri*(q1[1]*dy - q1[2]*dx)
        ri   = 1.0/q2[0]
        p2   = gm1*(q2[3]-0.5*ri*(q2[1]*q2[1]+q2[2]*q2[2]))
        vol2 =  ri*(q2[1]*dy - q2[2]*dx)
        mu = 0.5*((adt1)+(adt2))*eps
        f = 0.5*(vol1* q1[0]         + vol2* q2[0]        ) + mu*(q1[0]-q2[0])
        res1[0] += f
        res2[0] -= f
        f = 0.5*(vol1* q1[1] + p1*dy + vol2* q2[1] + p2*dy) + mu*(q1[1]-q2[1])
        res1[1] += f
        res2[1] -= f
        f = 0.5*(vol1* q1[2] - p1*dx + vol2* q2[2] - p2*dx) + mu*(q1[2]-q2[2])
        res1[2] += f
        res2[2] -= f
        f = 0.5*(vol1*(q1[3]+p1)     + vol2*(q2[3]+p2)    ) + mu*(q1[3]-q2[3])
        res1[3] += f
        res2[3] -= f

        return (res1,res2)

class BEdge(Set):
    def __init__(self,id):
        super().__init__(id)
        self.bound=0 # scalar
        self.t=0

    def bres_calc(self,g,x1,x2,q1,adt1):
        bound=self.bound
        res1=[0.0,0.0,0.0,0.0]
        eps=g.eps
        qinf=g.qinf
        gm1=g.gm1
        
        # HACK, to change airflow direction
        qinf=list(qinf)
        qinf1=qinf[1] * math.cos(self.t) - qinf[2]*math.sin(self.t)
        qinf2=qinf[1] * math.sin(self.t) + qinf[2]*math.cos(self.t)
        qinf[1]=qinf1
        qinf[2]=qinf2

        dx = x1[0] - x2[0]
        dy = x1[1] - x2[1]
        ri = 1.0/q1[0]
        p1 = gm1*(q1[3]-0.5*ri*(q1[1]*q1[1]+q1[2]*q1[2]))
        if (bound==1):
            res1[1] += + p1*dy
            res1[2] += - p1*dx
        else:
            vol1 =  ri*(q1[1]*dy - q1[2]*dx)
            ri   = 1.0/qinf[0]
            p2   = gm1*(qinf[3]-0.5*ri*(qinf[1]*qinf[1]+qinf[2]*qinf[2]))
            vol2 =  ri*(qinf[1]*dy - qinf[2]*dx)
            mu = (adt1)*eps
            f = 0.5*(vol1* q1[0]         + vol2* qinf[0]        ) + mu*(q1[0]-qinf[0])
            res1[0] += f
            f = 0.5*(vol1* q1[1] + p1*dy + vol2* qinf[1] + p2*dy) + mu*(q1[1]-qinf[1])
            res1[1] += f
            f = 0.5*(vol1* q1[2] - p1*dx + vol2* qinf[2] - p2*dx) + mu*(q1[2]-qinf[2])
            res1[2] += f
            f = 0.5*(vol1*(q1[3]+p1)     + vol2*(qinf[3]+p2)    ) + mu*(q1[3]-qinf[3])
            res1[3] += f

        self.t+=0.002

        return (res1,)


class Cell(Set):
    def __init__(self,id):
        super().__init__(id)
        self.q=[0.0,0.0,0.0,0.0]
        self.qold=[0.0,0.0,0.0,0.0]
        self.adt=0.0
        self.res=[0.0,0.0,0.0,0.0]

    def save_soln(self,g):
        self.qold=list(self.q)

    def adt_calc(self,g,x1,x2,x3,x4):
        #double dx,dy, ri,u,v,c;
        q=self.q
        adt=0.0
        gam=g.gam
        gm1=g.gm1
        cfl=g.cfl
        
        print( (x1[0],x1[1]),(x2[0],x2[1]),(x3[0],x3[1]),(x4[0],x4[1]), )

        ri =  1.0/q[0]
        u  =   ri*q[1]
        v  =   ri*q[2]
        c  = math.sqrt(gam*gm1*(ri*q[3]-0.5*(u*u+v*v)))
        dx = x2[0] - x1[0]
        dy = x2[1] - x1[1]
        adt += math.fabs(u*dy-v*dx) + c*math.sqrt(dx*dx+dy*dy)
        dx = x3[0] - x2[0]
        dy = x3[1] - x2[1]
        adt += math.fabs(u*dy-v*dx) + c*math.sqrt(dx*dx+dy*dy)
        dx = x4[0] - x3[0]
        dy = x4[1] - x3[1]
        adt += math.fabs(u*dy-v*dx) + c*math.sqrt(dx*dx+dy*dy)
        dx = x1[0] - x4[0]
        dy = x1[1] - x4[1]
        adt += math.fabs(u*dy-v*dx) + c*math.sqrt(dx*dx+dy*dy)
        adt = adt / cfl

        self.adt=adt

        return ()

    def update(self,g):
        qold=self.qold
        q=self.q
        res=self.res
        adt=self.adt
        rms=0.0

        adti = 1.0/(adt)
        for n in range(4):
            ddel    = adti*res[n]
            q[n]   = qold[n] - ddel
            res[n] = 0.0
            rms  += ddel*ddel
        
        return (rms,)


def load_map(fromList, toList, data):
    return [
        tuple(j for j in data[i])
        for
        i in range(len(fromList))
    ]

def load_dat(set, name, data):    
    for i in range(len(set)):
        if len(data[0])==1:
            setattr(set[i], name, data[i][0])
        else:
            setattr(set[i], name, list(data[i]))

def create_square(n):
    #   0  1  2  3  4  5  6  7  8=2*n
    # 0 p--b--p--b--p--b--p--b--p
    #   |     |     |     |     |
    # 1 b  c  e  c  e  c  e  c  b
    #   |     |     |     |     |
    # 2 p--e--p--e--p--e--p--e--p
    #   |     |     |     |     |
    # 3 b  c  e  c  e  c  e  c  b
    #   |     |     |     |     |
    # 4 p--b--p--b--p--b--p--b--p
    #   |     |     |     |     |
    # 5 b  c  e  c  e  c  e  c  b
    #   |     |     |     |     |
    # 6 p--b--p--b--p--b--p--b--p
    #   |     |     |     |     |
    # 7 b  c  e  c  e  c  e  c  b
    #   |     |     |     |     |
    # 8 p--b--p--b--p--b--p--b--p
    #   0  1  2  3  4  5  6  7  8=2*n
    
    
    qinf=[1.        ,  0.47328639,  0.        ,  2.61200015]
    
    g=Globals()
    g.qinf=qinf
    g.gam=1.3999999761581421
    g.gm1=0.39999997615814209
    g.cfl=0.89999997615814209
    g.mach=0.40000000596046448
    g.alpha=0.052359877559829883
    
    nodes={}
    cells={}
    edges={}
    bedges={}
    for x in range(0,2*n+1):
        for y in range(0,2*n+1):
            if (x+y)%2==0:  # Either a node or a cell
                if x%2==0: # A node
                    nodes[(x,y)]=Node(len(nodes))
                    nodes[(x,y)].x=[x/n,y/n]
                else:  # A cell
                    cells[(x,y)]=Cell(len(cells))
            else: # Either an edge or a bedge
                if x==0 or x==2*n or y==0 or y==2*n:
                    bedges[(x,y)]=BEdge(len(bedges))
                else:
                    edges[(x,y)]=Edge(len(edges))
    
    pcell=[None]*len(cells)
    for ((x,y),cell) in cells.items():
        # clock-wise list
        pcell[cell.id]=( nodes[(x-1,y-1)].id,nodes[(x+1,y-1)].id,nodes[(x+1,y+1)].id,nodes[(x-1,y+1)].id )
        cell.q=list(qinf)
        
    pedge=[None]*len(edges)
    pecell=[None]*len(edges)
    for ((x,y),edge) in edges.items():
        if x%2==0: # vert edge
            pedge[edge.id]=( nodes[(x,y-1)].id,nodes[(x,y+1)].id )
            pecell[edge.id]=( cells[(x-1,y)].id,cells[(x+1,y)].id )
        else: # horz edge
            pedge[edge.id]=( nodes[(x+1,y)].id,nodes[(x-1,y)].id )
            pecell[edge.id]=( cells[(x,y-1)].id,cells[(x,y+1)].id )
            
    pbedge=[None]*len(bedges)
    pbecell=[None]*len(bedges)
    for ((x,y),bedge) in bedges.items():
        if x==0:
            pbedge[bedge.id]=( nodes[(x,y+1)].id , nodes[(x,y-1)].id )
            pbecell[bedge.id]=( cells[(x+1,y)].id , )
            bedge.bound=1
        elif x==2*n:
            pbedge[bedge.id]=( nodes[(x,y-1)].id , nodes[(x,y+1)].id )
            pbecell[bedge.id]=( cells[(x-1,y)].id , )
            bedge.bound=1
        elif y==0:
            pbedge[bedge.id]=( nodes[(x-1,y)].id , nodes[(x+1,y)].id )
            pbecell[bedge.id]=( cells[(x,y+1)].id , )
            bedge.bound=1
        else:
            pbedge[bedge.id]=( nodes[(x+1,y)].id , nodes[(x-1,y)].id )
            pbecell[bedge.id]=( cells[(x,y-1)].id , )
            bedge.bound=1

    def to_set(m):
        return [nn for (ni,nn) in sorted([(n.id,n) for n in m.values()]) ]
        
     
    return {
        "globals" : g,
        "nodes" : to_set(nodes),
        "edges" : to_set(edges),
        "bedges" : to_set(bedges),
        "cells" : to_set(cells),
        "pedge" : pedge,
        "pecell" : pecell,
        "pbedge" : pbedge,
        "pbecell" : pbecell,
        "pcell" : pcell
    }

def load(src):
    g=Globals()
    
    g.gam=src["gam"][0]
    g.gm1=src["gm1"][0]
    g.cfl=src["cfl"][0]
    g.eps=src["eps"][0]
    g.mach=src["mach"][0]
    g.alpha=src["alpha"][0]
    g.qinf=list(src["qinf"])


    nodes = [ Node(i) for i in range(src["nodes"][0]) ]
    edges = [ Edge(i) for i in range(src["edges"][0]) ]
    bedges = [ BEdge(i) for i in range(src["bedges"][0]) ]
    cells = [ Cell(i) for i in range(src["cells"][0]) ]

    sys.stderr.write("Loading dats\n")            
    load_dat(nodes, "x", src["p_x"])    
    load_dat(cells, "q", src["p_q"])    
    load_dat(cells, "qold", src["p_qold"])    
    load_dat(cells, "adt", src["p_adt"])    
    load_dat(cells, "res", src["p_res"])    
    load_dat(bedges, "bound", src["p_bound"])    

    sys.stderr.write("Loading maps\n")
    pedge = load_map(edges, nodes, src["pedge"]) # EdgeIndex -> [NodeIndex]*2
    pecell = load_map(edges, cells, src["pecell"]) # EdgeIndex -> [CellIndex]*2
    pbedge = load_map(bedges, nodes, src["pbedge"]) # BEdgeIndex -> [NodeIndex]*2
    pbecell = load_map(bedges, cells, src["pbecell"]) # BEdgeIndex -> [CellIndex]
    pcell = load_map(cells, nodes, src["pcell"]) # CellIndex -> [NodeIndex]*4

    return {
        "globals" : g,
        "nodes" : nodes,
        "edges" : edges,
        "bedges" : bedges,
        "cells" : cells,
        "pedge" : pedge,
        "pecell" : pecell,
        "pbedge" : pbedge,
        "pbecell" : pbecell,
        "pcell" : pcell
    }
    
def run_model(model,plot_path):
    import plot
    
    globals().update(model)
    
    sys.stderr.write("Running\n")
    niter = 1000
    for iter in range(1, niter + 1):
        sys.stderr.write("  Iter {}\n".format(iter))

        for c in cells:
            c.save_soln(globals)

        for k in range(2):
            
            for ci in range(len(cells)):
                cells[ci].adt_calc(globals,
                    nodes[pcell[ci][0]].x,
                    nodes[pcell[ci][1]].x,
                    nodes[pcell[ci][2]].x,
                    nodes[pcell[ci][3]].x,
                )

            for ei in range(len(edges)):
                (res1Delta,res2Delta)=edges[ei].res_calc(globals,
                    nodes[pedge[ei][0]].x,
                    nodes[pedge[ei][1]].x,
                    cells[pecell[ei][0]].q,
                    cells[pecell[ei][1]].q,
                    cells[pecell[ei][0]].adt,
                    cells[pecell[ei][1]].adt
                )
                for i in range(4):
                    cells[pecell[ei][0]].res[i] += res1Delta[i]
                    cells[pecell[ei][1]].res[i] += res2Delta[i]
            
            for bei in range(len(bedges)):
                (res,)=bedges[bei].bres_calc(globals,
                    nodes[pbedge[bei][0]].x,
                    nodes[pbedge[bei][1]].x,
                    cells[pbecell[bei][0]].q,
                    cells[pbecell[bei][0]].adt
                )
                for i in range(4):
                    cells[pbecell[bei][0]].res[i] += res[i]

            rms=0.0
            for c in cells:
                (rmsDelta,)=c.update(globals)
                rms += rmsDelta
        
        rms = math.sqrt(rms / len(cells) )
        if (iter%100)==0:
            print(" {:d}  {:10.5e} ".format(iter, rms))
            if not plot_path:
                plot.plot_model()
            
        if plot_path:
            plot.plot_model(model, "{}{:04}.png".format(plot_path,iter))
        
    



if __name__=="__main__":
    sys.stderr.write("Loading globals\n")
    #src=h5py.File("new_grid.h5")
    #model=load(src)
    model=create_square(20)
   
    run_model(model,"out")
