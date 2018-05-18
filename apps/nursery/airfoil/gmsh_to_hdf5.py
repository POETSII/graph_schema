#!/usr/bin/env python3
import sys
import re
import numpy as np
import h5py

class Mesh:
    def expect(self,src,pattern):
        got=src.readline().strip();
        if not re.fullmatch(pattern,got):
            raise RuntimeError("Expected to match {}, got {}".format(pattern, got))
    
    def __init__(self,src):
        self.expect(src,"\$\MeshFormat\s*")
        self.expect(src,"2[.]2\s+0\s+[0-9]+\s*")
        self.expect(src,"\$EndMeshFormat\s*")
        
        self.expect(src,"\$Nodes\s*")
        
        nNodes=int(src.readline())
        sys.stderr.write("Node count : {}\n".format(nNodes))
        
        nodePositions=np.zeros( (nNodes,2) )
        nodeIdToIndex={} # In principle they are not contiguous nodes
        for i in range(nNodes):
            (id,x,y,z)=src.readline().strip().split(" ")
            assert float(z)==0
            nodePositions[i][:]=[float(x),float(y)]
            nodeIdToIndex[id]=i
        self.p_x=nodePositions
        
        self.expect(src,"\$EndNodes\s*")
        
        self.expect(src,"\$Elements\s*")
        
        nMaxCells=int(src.readline())
        sys.stderr.write("Max Cell count : {}\n".format(nMaxCells))
        pcell=np.zeros( (nMaxCells,4), dtype=np.uint32 )
        cellIdToIndex={} # In principle they are not contiguous nodes
        for i in range(nMaxCells):
            parts=src.readline().strip().split(" ")
            id=parts[0]
            type=int(parts[1])
            if type==3: # quadrangle
                index=len(cellIdToIndex)
                cellIdToIndex[id]=index
                assert len(parts)==9
                assert int(parts[2])==2, "Expected two tags. {}".format(parts)
                physical=parts[3]
                logical=parts[4]
                pcell[index][:]=[nodeIdToIndex[id] for id in parts[5:]]
                assert len(set(pcell[index]))==4 # Make sure nodes are unique
            elif type==15: # Point
                pass
            elif type==1: # line
                pass
            else:
                assert False, "Unknown type {}".format(type)
        nCells=len(cellIdToIndex)
        pcell.resize((nCells,4))
        sys.stderr.write("Actual Cell count : {}\n".format(nCells))
        self.pcell=pcell
                
        allEdges={}
        
        def add_edge(n1,n2,cell):
            id=(min(n1,n2),max(n1,n2))
            if id==(0,0):
                print(n1,n2,cell)
            allEdges.setdefault( id, [] ).append(cell)
                
        for (cell,(n1,n2,n3,n4)) in enumerate(pcell[:]):
            add_edge(n1,n2,cell)
            add_edge(n2,n3,cell)
            add_edge(n3,n4,cell)
            add_edge(n4,n1,cell)
            
        for (id,ncells) in allEdges.items():
            assert len(ncells)<=2, "Edge {}, Len cells={}, cells={}".format(id, len(ncells), ncells)
            
        nEdges=sum([ len(cells) for cells in allEdges.values()])-len(allEdges)
        nBEdges=len(allEdges)-nEdges
        print("nEdges={}, nBEdges={}".format(nEdges,nBEdges))

        pedge=np.zeros( (nEdges,2), dtype=np.uint32 )
        pecell=np.zeros( (nEdges,2), dtype=np.uint32 )
        pbedge=np.zeros( (nBEdges,2), dtype=np.uint32 )
        pbecell=np.zeros( (nBEdges,1), dtype=np.uint32 )
        
        for (ei,((n1,n2),(c1,c2))) in enumerate([c for c in allEdges.items()  if len(c[1])==2 ]):
            pedge[ei][:]=[n1,n2]
            pecell[ei][:]=[c1,c2]
        self.pedge=pedge
        self.pecell=pecell
        
        for (bei,((n1,n2),(c1,))) in enumerate([c for c in allEdges.items() if len(c[1])==1 ]):
            pbedge[bei][:]=[n1,n2]
            pbecell[bei][:]=[c1]
    
        self.pbedge=pbedge
        self.pbecell=pbecell
        
        self.qinf=np.array([1.        ,  0.47328639,  0.        ,  2.61200015])
        self.gam=1.3999999761581421
        self.gm1=0.39999997615814209
        self.cfl=0.89999997615814209
        self.mach=0.40000000596046448
        self.alpha=0.052359877559829883
        self.eps=0.05000000074505806
        
        self.p_bound=np.zeros( (nBEdges,1) )
        self.p_qold=np.zeros( (nCells,4) )
        self.p_adt=np.zeros( (nCells,1) )
        self.p_res=np.zeros( (nCells,4) )
        self.p_q=np.zeros( (nCells,4) )
        for i in range(4):
            self.p_q[:,i]=self.qinf[i]
        
    def write_hdf5(self,path):
        f = h5py.File(path, "w")
        
        def add_scalar_float(name,value):       
            s=f.create_dataset(name, dtype='f8', shape=(1,))
            s[0]=value
            
        def add_scalar_unsigned(name,value):       
            s=f.create_dataset(name, dtype='i4', shape=(1,))
            s[0]=value
            
        def add_array(name,data):
            s=f.create_dataset(name, data=data)
            
        add_scalar_float("gam",self.gam)
        add_scalar_float("gm1",self.gm1)
        add_scalar_float("cfl",self.cfl)
        add_scalar_float("mach",self.mach)
        add_scalar_float("alpha",self.alpha)
        add_scalar_float("eps",self.eps)
        add_array("qinf",self.qinf)
        
        add_scalar_unsigned("cells",self.pcell.shape[0])
        add_scalar_unsigned("nodes",self.p_x.shape[0])
        add_scalar_unsigned("edges",self.pedge.shape[0])
        add_scalar_unsigned("bedges",self.pbedge.shape[0])
        
        add_array("p_x",self.p_x)
        add_array("p_q",self.p_q)
        add_array("p_qold",self.p_qold)
        add_array("p_adt",self.p_adt)
        add_array("p_res",self.p_res)
        add_array("p_bound",self.p_bound)
        
        add_array("pedge",self.pedge)
        add_array("pbedge",self.pbedge)
        add_array("pecell",self.pecell)
        add_array("pbecell",self.pbecell)
        add_array("pcell",self.pcell)
        
        f.flush()
        f.close()

if __name__=="__main__":
    m=Mesh(sys.stdin)
    m.write_hdf5("tmp.hdf5")
