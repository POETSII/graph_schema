#!/usr/bin/env python3

import logging
import numpy
import math
import copy

from mini_op2.framework.core import DataType, Parameter, AccessMode
from mini_op2.framework.system import SystemSpecification, SystemInstance, load_hdf5_instance
from mini_op2.framework.control_flow import *

from numpy import ndarray as nda

seq_double=typing.Sequence[float]
seq_seq_double=typing.Sequence[typing.Sequence[float]]
seq_float=typing.Sequence[float]
seq_int=typing.Sequence[int]


from numpy import sqrt, fabs

def dot_product(
    x:seq_double,
    y:seq_double,
    prod:seq_double,
    g_sum:seq_double
) -> None:
    prod[0] = x[0] * y[0]
    g_sum[0] += prod[0]

def build_system(n:int) -> (SystemInstance,Statement):

    WRITE=AccessMode.WRITE
    READ=AccessMode.READ
    INC=AccessMode.INC
    RW=AccessMode.RW
    
    sys=SystemSpecification()
    
    g_sum=sys.create_mutable_global("g_sum", DataType(shape=(1,)))
    
    xvals=sys.create_set("xvals")
    yvals=sys.create_set("yvals")
    points=sys.create_set("points")
    
    p_x=sys.create_dat(xvals, "p_x", DataType(shape=(1,)))
    p_y=sys.create_dat(yvals, "p_y", DataType(shape=(1,)))
    p_prod=sys.create_dat(points, "p_prod", DataType(shape=(1,)))
    
    xlink=sys.create_map("xlink", points, xvals, 1)
    ylink=sys.create_map("ylink", points, yvals, 1)
    
    instDict={
        "xvals" : n,
        "yvals" : n,
        "points" : n,
        "p_x" : numpy.array( [[i*1.0] for i in range(n)] ),
        "p_y" : numpy.array( [[i*1.0] for i in range(n)] ),
        "p_prod" : numpy.array( [ [0.0] for i in range(n)] ),
        "g_sum" : numpy.array( [0.0] ),
        "xlink" : numpy.array( [[i] for i in range(n)] ),
        "ylink" : numpy.array( [[i] for i in range(n)] )
    }
    
    inst=SystemInstance(sys, instDict)
    
    instOutput={
        "output" : {
            "final" : copy.copy(instDict)
        },
        **instDict
    }
    
    expected=[i*i*1.0 for i in range(n)]
    
    instOutput["output"]["final"]["g_sum"] = numpy.array( [ sum(expected) ] )
    instOutput["output"]["final"]["p_prod"] = numpy.array( [ [e] for e in expected ] )

    code=Seq(
        """
        g_sum[0] = 0
        """,
        ParFor(
            dot_product,
            points,
            p_x(READ, xlink, 0),
            p_y(READ, ylink, 0),
            p_prod(WRITE),
            g_sum(INC)
        ),
        CheckState(instOutput, "/output/final"),
    )
    code.on_bind_spec(sys)
    return (sys,inst,code)

if __name__=="__main__":
    logging.basicConfig(level=4,style="{")
    
    (spec,inst,code)=build_system(8)

    code.execute(inst)
