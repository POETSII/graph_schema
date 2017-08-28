#!/usr/bin/env python3

import logging
import numpy
import math

from mini_op2.core import DataType, Parameter, AccessMode
from mini_op2.system import SystemSpecification, SystemInstance, load_hdf5_instance
from mini_op2.control_flow import *

from numpy import ndarray as nda

seq_double=typing.Sequence[float]
seq_seq_double=typing.Sequence[typing.Sequence[float]]
seq_float=typing.Sequence[float]
seq_int=typing.Sequence[int]


from numpy import sqrt, fabs

def dirichlet(
    res:seq_double
) -> None:
    res[0]=0.0
    
def dotPV(
    p:seq_double,
    v:seq_double,
    c:seq_double
) -> None:
    c[0] += p[0] * v[0]

def dotR(
    r:seq_double,
    c:seq_double
) -> None:
    c[0] += r[0] * r[0]
    
def init_cg(
    r:seq_double,
    c:seq_double,
    u:seq_double,
    v:seq_double,
    p:seq_double
) -> None:
  c[0] += r[0] * r[0]
  p[0] = r[0]
  u[0] = 0
  v[0] = 0


def res_calc(
    Ng2_xi:seq_double,
    wtg2:seq_double,
    gm1:seq_double,
    gm1i:seq_double,
    m2:seq_double,
    x:seq_seq_double,
    phim:seq_seq_double,
    K:seq_double,
    res:seq_seq_double
) -> None:

    #for (int j = 0; j < 4; j++) {
    #    for (int k = 0; k < 4; k++) {
    #        OP2_STRIDE(K, j * 4 + k) = 0;
    #    }
    #}
    for j in range(4):
        for k in range(4):
            K[j*4+k]=0
  
    # for (int i = 0; i < 4; i++) { // for each gauss point            
    for i in range(4):
        det_x_xi = 0.0;
        N_x = [0.0,0.0,0.0,0.0, 0.0,0.0,0.0,0.0]

        a = 0;
        #for (int m = 0; m < 4; m++)
        for m in range(4):
            xv=x[m]
            xv1=xv[1]
            det_x_xi += Ng2_xi[4 * i + 16 + m] * xv1
        #for (int m = 0; m < 4; m++)
        for m in range(4):
          N_x[m] = det_x_xi * Ng2_xi[4 * i + m]

        a = 0;
        #for (int m = 0; m < 4; m++)
        for m in range(4):
          a += Ng2_xi[4 * i + m] * x[m][0]
        #for (int m = 0; m < 4; m++)
        for m in range(4):
          N_x[4 + m] = a * Ng2_xi[4 * i + 16 + m]

        det_x_xi *= a

        a = 0
        #for (int m = 0; m < 4; m++)
        for m in range(4):
          a += Ng2_xi[4 * i + m] * x[m][1];
        #for (int m = 0; m < 4; m++)
        for m in range(4):
          N_x[m] -= a * Ng2_xi[4 * i + 16 + m];

        b = 0.0
        for m in range(4):
          b += Ng2_xi[4 * i + 16 + m] * x[m][0]
        for m in range(4):
          N_x[4 + m] -= b * Ng2_xi[4 * i + m]

        det_x_xi -= a * b

        for j in range(8):
          N_x[j] /= det_x_xi;

        wt1 = wtg2[i] * det_x_xi
        
        u = [0.0, 0.0]
        for j in range(4):
          u[0] += N_x[j] * phim[j][0]
          u[1] += N_x[4 + j] * phim[j][0]
        
        Dk = 1.0 + 0.5 * gm1 * (m2 - (u[0] * u[0] + u[1] * u[1]))
        rho = pow(Dk, gm1i) # wow this might be problematic -> go to log?
        rc2 = rho / Dk

        for j in range(4):
            ressss=res[j]
            #print(ressss)
            tmp = wt1 * rho * (u[0] * N_x[j] + u[1] * N_x[4 + j])
            ressss[0] += tmp
        
        for j in range(4):
          for k in range(4):
            K[j * 4 + k] += \
                wt1 * rho * (N_x[j] * N_x[k] + N_x[4 + j] * N_x[4 + k]) - \
                wt1 * rc2 * (u[0] * N_x[j] + u[1] * N_x[4 + j]) * \
                    (u[0] * N_x[k] + u[1] * N_x[4 + k])
 
def spMV(
    v : seq_seq_double,
    K : seq_double,
    p : seq_seq_double
 ) ->None :
    v[0][0] += K[0] * p[0][0]
    v[0][0] += K[1] * p[1][0]
    v[1][0] += K[1] * p[0][0]
    v[0][0] += K[2] * p[2][0]
    v[2][0] += K[2] * p[0][0]
    v[0][0] += K[3] * p[3][0]
    v[3][0] += K[3] * p[0][0]
    v[1][0] += K[4 + 1] * p[1][0]
    v[1][0] += K[4 + 2] * p[2][0]
    v[2][0] += K[4 + 2] * p[1][0]
    v[1][0] += K[4 + 3] * p[3][0]
    v[3][0] += K[4 + 3] * p[1][0]
    v[2][0] += K[8 + 2] * p[2][0]
    v[2][0] += K[8 + 3] * p[3][0]
    v[3][0] += K[8 + 3] * p[2][0]
    v[3][0] += K[15] * p[3][0]

def update(
    phim:seq_double,
    res:seq_double,
    u:seq_double,
    rms:seq_double
) -> None:
    phim[0] -= u[0]
    res[0] = 0.0;
    rms[0] += u[0] * u[0]

def updateP(
    r:seq_double,
    p:seq_double,
    beta:seq_double
) -> None:
    p[0] = beta[0] * p[0] + r[0]

def updateUR(
    u:seq_double,
    r:seq_double,
    p:seq_double,
    v:seq_double,
    alpha:seq_double
) -> None :
  u[0] += alpha[0] * p[0]
  r[0] -= alpha[0] * v[0]
  v[0] = 0.0


def build_system(srcFile:str="./meshes/FE_mesh.hdf5") -> (SystemInstance,Statement):

    WRITE=AccessMode.WRITE
    READ=AccessMode.READ
    INC=AccessMode.INC
    RW=AccessMode.RW
    
    sys=SystemSpecification()
    
    gam=sys.create_const_global("gam", DataType(shape=(1,)))
    gm1=sys.create_const_global("gm1", DataType(shape=(1,)))
    gm1i=sys.create_const_global("gm1i", DataType(shape=(1,)))
    m2=sys.create_const_global("m2", DataType(shape=(1,)))
    wtg1=sys.create_const_global("wtg1", DataType(shape=(2,)))
    xi1=sys.create_const_global("xi1", DataType(shape=(2,)))
    Ng1=sys.create_const_global("Ng1", DataType(shape=(4,)))
    Ng1_xi=sys.create_const_global("Ng1_xi", DataType(shape=(4,)))
    wtg2=sys.create_const_global("wtg2", DataType(shape=(4,)))
    Ng2=sys.create_const_global("Ng2", DataType(shape=(16,)))
    Ng2_xi=sys.create_const_global("Ng2_xi", DataType(shape=(32,)))
    minf=sys.create_const_global("minf", DataType(shape=(1,)))
    freq=sys.create_const_global("freq", DataType(shape=(1,)))
    kappa=sys.create_const_global("kappa", DataType(shape=(1,)))
    nmode=sys.create_const_global("nmode", DataType(shape=(1,)))
    mfan=sys.create_const_global("mfan", DataType(shape=(1,)))

    c1=sys.create_mutable_global("c1", DataType(shape=(1,)))    
    c2=sys.create_mutable_global("c2", DataType(shape=(1,)))
    alpha=sys.create_mutable_global("alpha", DataType(shape=(1,)))
    c3=sys.create_mutable_global("c3", DataType(shape=(1,)))
    beta=sys.create_mutable_global("beta", DataType(shape=(1,)))
    rms=sys.create_mutable_global("rms", DataType(shape=(1,)))    
    
    iter=sys.create_mutable_global("iter", DataType(dtype=numpy.uint32, shape=(1,)))    
    iterm1=sys.create_mutable_global("iterm1", DataType(dtype=numpy.uint32, shape=(1,)))    
    res0=sys.create_mutable_global("res0")
    res=sys.create_mutable_global("res")
    inner_iter=sys.create_mutable_global("inner_iter", DataType(dtype=numpy.uint32))
    maxiter=sys.create_mutable_global("maxiter", DataType(dtype=numpy.uint32))
    
    cells=sys.create_set("cells")
    bedges=sys.create_set("bedges")
    nodes=sys.create_set("nodes")
    
    p_xm=sys.create_dat(nodes, "p_x", DataType(shape=(2,)))
    p_phim=sys.create_dat(nodes, "p_phim", DataType(shape=(1,)))
    p_resm=sys.create_dat(nodes, "p_resm", DataType(shape=(1,)))
    p_K=sys.create_dat(cells, "p_K", DataType(shape=(16,)))
    p_V=sys.create_dat(nodes, "p_V", DataType(shape=(1,)))
    p_P=sys.create_dat(nodes, "p_P", DataType(shape=(1,)))
    p_U=sys.create_dat(nodes, "p_U", DataType(shape=(1,)))
    
    pbedge=sys.create_map("pbedge", bedges, nodes, 1)
    pcell=sys.create_map("pcell", cells, nodes, 4)

    inst=load_hdf5_instance(sys,srcFile)            
    refFile=h5py.File(srcFile)


    code=RepeatForCount(20,iterm1,
        """
        iter[0]=iterm1[0]+1 # Original code uses 1 base start
        """,
        ParFor(
            res_calc,
            cells,
            Ng2_xi(READ),
            wtg2(READ),
            gm1(READ),
            gm1i(READ),
            m2(READ),
            p_xm(READ,pcell,-4),
            p_phim(READ,pcell,-4),
            p_K(WRITE),
            p_resm(INC,pcell,-4)
        ),
        CheckState(refFile, "/output/iter{iter}_post_res_calc"),
        ParFor(
            dirichlet,
            bedges,
            p_resm(WRITE,pbedge,0)
        ),
        CheckState(refFile, "/output/iter{iter}_post_dirichlet"),
        """
        c1[0]=0
        c2[0]=0
        c3[0]=0
        alpha[0]=0
        beta[0]=0
        """,
        ParFor(
            init_cg,
            nodes,
            p_resm(READ),
            c1(INC),
            p_U(WRITE),
            p_V(WRITE),
            p_P(WRITE)
        ),
        CheckState(refFile, "/output/iter{iter}_post_init_cg"),
        """
        print("  c1 = %g" % (c1[0]) )
        res0[0]=sqrt(c1[0])
        res[0]=res0[0]
        inner_iter[0]=0
        maxiter[0]=200
        print("res=%g, 0.1*res0=%g,  inner_iter=%u, maxiter=%u" % (res[0],0.1*res0[0], inner_iter[0], maxiter[0]) )
        """,
        While( """ (res[0]>0.1*res0[0]) and inner_iter[0] < maxiter[0] """,
            """print("  res=%g, 0.1*res0=%g,  inner_iter=%u, maxiter=%u" % (res[0],0.1*res0[0], inner_iter[0], maxiter[0]) )""",
            ParFor(
                spMV,
                cells,
                p_V(INC,pcell,-4),
                p_K(READ),
                p_P(READ,pcell,-4)
            ),
            CheckState(refFile, "/output/iter{iter}_inner{inner_iter}_post_spMV"),
            ParFor(
                dirichlet,
                bedges,
                p_V(WRITE, pbedge,0)
            ),
            CheckState(refFile, "/output/iter{iter}_inner{inner_iter}_post_dirichlet"),
            """
            c2[0]=0
            """,
            ParFor(
                dotPV,
                nodes,
                p_P(READ),
                p_V(READ),
                c2(INC)
            ),
            CheckState(refFile, "/output/iter{iter}_inner{inner_iter}_post_dotPV"),
            """
            alpha[0]=c1[0]/c2[0]
            """,
            ParFor(
                updateUR,
                nodes,
                p_U(INC),
                p_resm(INC),
                p_P(READ),
                p_V(RW),
                alpha(READ)
            ),
            CheckState(refFile, "/output/iter{iter}_inner{inner_iter}_post_updateUR"),
            """
            c3[0]=0
            """
            ,
            ParFor(
                dotR,
                nodes,
                p_resm(READ),
                c3(INC)
            ),
            CheckState(refFile, "/output/iter{iter}_inner{inner_iter}_post_dotR"),
            """
            beta[0]=c3[0]/c1[0]
            """,
            ParFor(
                updateP,
                nodes,
                p_resm(READ),
                p_P(RW),
                beta(READ)
            ),
            """
            c1[0] = c3[0]
            res[0] = sqrt(c1[0])
            inner_iter[0]+=1
            """,
            CheckState(refFile, "/output/iter{iter}_inner{inner_iter}")
        ),
        """
        rms[0]=0
        """,
        ParFor(
            update,
            nodes,
            p_phim(RW),
            p_resm(WRITE),
            p_U(READ),
            rms(INC)
        ),
        """
        rmsV=sqrt(rms[0])/sqrt(sizeof_nodes[0])
        iterV=iter[0]
        print("rms = %10.5g iter: %d" % ( rmsV, iterV) )
        """,
        CheckState(refFile, "/output/iter{iter}")
    )
    return (sys,inst,code)

if __name__=="__main__":
    logging.basicConfig(level=4,style="{")
    
    (spec,inst,code)=build_system()

    code.execute(inst)
