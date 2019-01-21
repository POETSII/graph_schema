#!/usr/bin/env python3

import logging
import numpy
import math

from mini_op2.framework.core import DataType, Parameter, AccessMode
from mini_op2.framework.system import SystemSpecification, SystemInstance, load_hdf5_instance
from mini_op2.framework.control_flow import *

from numpy import ndarray as nda

seq_double=typing.Sequence[float]
seq_float=typing.Sequence[float]
seq_int=typing.Sequence[int]


from numpy import sqrt, fabs


def save_soln(qold:seq_double, q:seq_double):
    for i in range(4):
        qold[i]=q[i]
    
def adt_calc(
    gam:seq_double, gm1:seq_double, cfl:seq_double,
    x1:seq_double, x2:seq_double, x3:seq_double, x4:seq_double, q:seq_double, adt:seq_double
  ):
  ri = 1.0 / q[0];
  u = ri * q[1];
  v = ri * q[2];
  c = sqrt(gam[0] * gm1[0] * (ri * q[3] - 0.5 * (u * u + v * v)));

    # Note: These are re-ordered to introduce some amount of floating-point
    # difference against original

  dx = x2[0] - x1[0];
  dy = x2[1] - x1[1];
  adt[0] = fabs(u * dy - v * dx) + c * sqrt(dx * dx + dy * dy);
  
  dx = x1[0] - x4[0];
  dy = x1[1] - x4[1];
  adt[0] += fabs(u * dy - v * dx) + c * sqrt(dx * dx + dy * dy);
  
  dx = x4[0] - x3[0];
  dy = x4[1] - x3[1];
  adt[0] += fabs(u * dy - v * dx) + c * sqrt(dx * dx + dy * dy);

  dx = x3[0] - x2[0];
  dy = x3[1] - x2[1];
  adt[0] += fabs(u * dy - v * dx) + c * sqrt(dx * dx + dy * dy);

  
  adt[0] = adt[0] * (1.0 / cfl[0])
  
def res_calc(
    gm1:seq_double, eps:seq_double,
    x1:seq_double, x2:seq_double,
    q1:seq_double, q2:seq_double,
    adt1:seq_double, adt2:seq_double,
    res1:seq_double, res2:seq_double
    ):
        # double dx,dy,mu, ri, p1,vol1, p2,vol2, f;

        dx = x1[0] - x2[0]
        dy = x1[1] - x2[1]
        ri   = 1.0/q1[0]
        p1   = gm1[0]*(q1[3]-0.5*ri*(q1[1]*q1[1]+q1[2]*q1[2]))
        vol1 =  ri*(q1[1]*dy - q1[2]*dx)
        ri   = 1.0/q2[0]
        p2   = gm1[0]*(q2[3]-0.5*ri*(q2[1]*q2[1]+q2[2]*q2[2]))
        vol2 =  ri*(q2[1]*dy - q2[2]*dx)
        mu = 0.5*((adt1[0])+(adt2[0]))*eps[0]
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

def bres_calc(
    eps:seq_double, qinf:seq_double, gm1:seq_double,
    bound:seq_int,
    x1:seq_double,
    x2:seq_double,
    q1:seq_double,
    adt1:seq_double,
    res1:seq_double
):
    dx = x1[0] - x2[0]
    dy = x1[1] - x2[1]
    ri = 1.0/q1[0]
    p1 = gm1[0]*(q1[3]-0.5*ri*(q1[1]*q1[1]+q1[2]*q1[2]))
    if (bound[0]==1):
        res1[1] += + p1*dy
        res1[2] += - p1*dx
    else:
        vol1 =  ri*(q1[1]*dy - q1[2]*dx)
        ri   = 1.0/qinf[0]
        p2   = gm1[0]*(qinf[3]-0.5*ri*(qinf[1]*qinf[1]+qinf[2]*qinf[2]))
        vol2 =  ri*(qinf[1]*dy - qinf[2]*dx)
        mu = (adt1[0])*eps[0]
        f = 0.5*(vol1* q1[0]         + vol2* qinf[0]        ) + mu*(q1[0]-qinf[0])
        res1[0] += f
        f = 0.5*(vol1* q1[1] + p1*dy + vol2* qinf[1] + p2*dy) + mu*(q1[1]-qinf[1])
        res1[1] += f
        f = 0.5*(vol1* q1[2] - p1*dx + vol2* qinf[2] - p2*dx) + mu*(q1[2]-qinf[2])
        res1[2] += f
        f = 0.5*(vol1*(q1[3]+p1)     + vol2*(qinf[3]+p2)    ) + mu*(q1[3]-qinf[3])
        res1[3] += f

def update(
    qold:seq_double,
    q:seq_double,
    res:seq_double,
    adt:seq_double,
    rms:seq_double
):
    adti = 1.0/(adt[0])
    for n in range(4):
        ddel    = adti*res[n]
        q[n]   = qold[n] - ddel
        res[n] = 0.0
        rms[0]  += ddel*ddel


def build_system(srcFile:str="../airfoil/new_grid.h5", maxiter=1000) -> (SystemInstance,Statement):

    WRITE=AccessMode.WRITE
    READ=AccessMode.READ
    INC=AccessMode.INC
    RW=AccessMode.RW
    
    sys=SystemSpecification()

    gam=sys.create_const_global("gam", DataType(shape=(1,)))
    gm1=sys.create_const_global("gm1", DataType(shape=(1,)))
    eps=sys.create_const_global("eps", DataType(shape=(1,)))
    cfl=sys.create_const_global("cfl", DataType(shape=(1,)))
    qinf=sys.create_const_global("qinf",DataType(shape=(4,)))

    rms=sys.create_mutable_global("rms", DataType(shape=(1,)))
    iterm1=sys.create_mutable_global("iterm1",DataType(shape=(1,),dtype=numpy.uint32))
    iter=sys.create_mutable_global("iter",DataType(shape=(1,),dtype=numpy.uint32))
    k=sys.create_mutable_global("k",DataType(shape=(1,),dtype=numpy.uint32))
    
    cells=sys.create_set("cells")
    edges=sys.create_set("edges")
    bedges=sys.create_set("bedges")
    nodes=sys.create_set("nodes")
    
    sizeof_cells=sys.get_sizeof_set(cells)

    p_x=sys.create_dat(nodes, "p_x", DataType(shape=(2,)))
    p_q=sys.create_dat(cells, "p_q", DataType(shape=(4,)))
    p_qold=sys.create_dat(cells, "p_qold", DataType(shape=(4,)))
    p_adt=sys.create_dat(cells, "p_adt", DataType(shape=(1,)))
    p_res=sys.create_dat(cells, "p_res", DataType(shape=(4,)))
    p_bound=sys.create_dat(bedges, "p_bound", DataType(dtype=numpy.int,shape=(1,)))

    pcell=sys.create_map("pcell", cells, nodes, 4)
    pedge=sys.create_map("pedge", edges, nodes, 2)
    pbedge=sys.create_map("pbedge", bedges, nodes, 2)
    pecell=sys.create_map("pecell", edges, cells, 2)
    pbecell=sys.create_map("pbecell", bedges, cells, 1)

    inst=load_hdf5_instance(sys,srcFile)

    refFile=h5py.File(srcFile)
        

    print_iter=0

    def debug_post_adt(instance:SystemInstance):
        global print_iter
        vals=instance.dats[p_adt]
        for i in range(instance.sets[cells]):
            print('<CP dev="c{}" key="post-adt-{}">"adt": {}</CP>'.format(i,print_iter,vals[i]))

    def debug_pre_update(instance:SystemInstance):
        global print_iter
        vals=instance.dats[p_res]
        for i in range(instance.sets[cells]):
            print('<CP dev="c{}" key="pre-update-{}">"res": {}</CP>'.format(i,print_iter,vals[i]))
        
    def debug_post_update(instance:SystemInstance):
        global print_iter
        vals=instance.dats[p_q]
        for i in range(instance.sets[cells]):
            print('<CP dev="c{}" key="post-update-{}">"q": {}</CP>'.format(i,print_iter,vals[i]))
        print_iter+=1

        

    code=RepeatForCount(maxiter,iterm1,
        """
        iter[0]=iterm1[0]+1 # Original code uses 1-based for loops
        """,
        ParFor(
            save_soln,
            cells,
            p_qold(WRITE),
            p_q(READ)
        ),
        RepeatForCount(2, k,        
            ParFor(
                adt_calc,
                cells,
                gam(READ), gm1(READ), cfl(READ),
                p_x(READ,pcell,0),
                p_x(READ,pcell,1),
                p_x(READ,pcell,2),
                p_x(READ,pcell,3),
                p_q(RW),
                p_adt(WRITE)
            ),
            #Debug(
            #    debug_post_adt
            #),
            ParFor(
                res_calc,
                edges,
                gm1(READ), eps(READ),
                p_x(READ,pedge,0),
                p_x(READ,pedge,1),
                p_q(READ,pecell,0),
                p_q(READ,pecell,1),
                p_adt(READ,pecell,0),
                p_adt(READ,pecell,1),
                p_res(INC,pecell,0),
                p_res(INC,pecell,1)
            ),
            ParFor(
                bres_calc,
                bedges,
                eps(READ), qinf(READ), gm1(READ), 
                p_bound(READ),
                p_x(READ,pbedge,0),
                p_x(READ,pbedge,1),
                p_q(READ,pbecell),
                p_adt(READ,pbecell),
                p_res(INC,pbecell)
            ),
            #Debug(
            #    debug_pre_update
            #),
            #UserCode(
            #    reset_rms_to_zero,
            #    rms(RW)
            #),
            """
            rms[0]=0
            """,
            ParFor(
                update,
                cells,
                p_qold(READ),
                p_q(RW),
                p_res(RW),
                p_adt(READ),
                rms(INC)
            ),
            #Debug(
            #    debug_post_update
            #)
            CheckState(refFile, "/output/iter{iter}_k{k}")
        ),
        """
        #handler_log(4, " %d  %10.5e ", iter[0], sqrt(rms[0] / sizeof_cells[0] ))
        #if (iter[0]%100)==0:
        #    handler_log(3, " %d  %10.5e ", iter[0], sqrt(rms[0] / sizeof_cells[0] ))
        rms[0]=rms[0]
        """
    )
    code.on_bind_spec(sys)
    return (sys,inst,code)
    
