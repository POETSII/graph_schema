
gam=...
gm1=...
cfl=...
eps=...
mach=...
alpha=...
qinf=...


class Set:
    def __init__(self,id):
        self.id=id

class Node(Set):
    def __init__(self,id,x):
        super().__init_(id)
        self.x=x # Vector of two numbers

class Edge(Set):
    def __init__(self,id):
        super().__init_(id)

    def res_calc(self,x1,x2,q1,q2,adt1,adt2):
        # double dx,dy,mu, ri, p1,vol1, p2,vol2, f;

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
        mu = 0.5*((*adt1)+(*adt2))*eps
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
    def __init__(self,id,bound):
        super().__init_(id)
        self.bound=bound # scalar

    def bres_calc(self,x1,x2,q1,adt1):
        bound=self.bound
        res1=[0.0,0.0,0.0,0.0]

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

        return (res1,)


class Cell(Set):
    def __init__(self,id,q):
        super().__init_(id)
        self.q=q
        self.qold=[0.0,0.0,0.0,0.0]
        self.adt=0.0
        self.res=[0.0,0.0,0.0,0.0]

    def save_soln(self):
        self.qold=list(self.q)

    def adt_calc(self,x1,x2,x3,x4):
        #double dx,dy, ri,u,v,c;
        q=self.q
        adt=0.0

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

    def update(self):
        qold=self.qold
        q=self.q
        res=self.res
        rms=0.0

        adti = 1.0/(*adt)
        for n in range(4):
            ddel    = adti*res[n]
            q[n]   = qold[n] - ddel
            res[n] = 0.0
            rms  += ddel*ddel
        
        return (rms,)


nodes = [] # Lots of Nodes
edges = [] # Lots of Edges
bedges = [] # Lots of BEdges
cells = [] # Lots of Cells

pedge = {} # Edge -> [Node]*2
pecell = {} # Edge -> [Cell]*2
pbedge = {} # BEdge -> [Node]*2
pbecell = {} # BEdge -> [Cell]
pcell = {} # Cell -> [Node]*4

niter = 1000
for i in range(1, niter + 1):

    for c : cells:
        c.save_soln()

    for k in range(2):

        for c in cells:
            c.adt_calc(
                nodes[pcell[c][0]].x,
                nodes[pcell[c][1]].x,
                nodes[pcell[c][2]].x,
                nodes[pcell[c][3]].x,
            )

        for e in edges:
            (res1Delta,res2Delta)=e.res_calc(
                nodes[pedge[e][0]].x,
                nodes[pedge[e][1]].x,
                cells[pecell[e][0]].adt,
                cells[pecell[e][1]].adt
            )
            cells[pecell[e][0]].res += res1Delta
            cells[pecell[e][1]].res += res2Delta
        
        for be : bedges:
            (res,)=be.bres_calc(
                nodes[pbedge[be][0]].x,
                nodes[pbedge[be][1]].x,
                cells[pbecell[be][0]].q,
                cells[pbecell[be][0]].adt
            )
            cells[pbecell[be][0]].res += res

        rms=0.0
        for c in cells:
            (rmsDelta,)=c.update()
            rms += rmsDelta
    
    rms = math.sqrt(rms / len(cells) )
    if i%100==0:
        print " %d  %10.5e " % (i, rms)
