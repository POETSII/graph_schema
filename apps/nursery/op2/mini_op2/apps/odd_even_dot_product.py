#!/usr/bin/env python3

import logging
import numpy
import math
import copy
import sys

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
    
    system=SystemSpecification()
   
    xvals=system.create_set("xvals")
    yvals=system.create_set("yvals")
    points=system.create_set("points")
    oddeven=system.create_set("oddeven")
    
    p_x=system.create_dat(xvals, "p_x", DataType(shape=(1,)))
    p_y=system.create_dat(yvals, "p_y", DataType(shape=(1,)))
    p_prod=system.create_dat(points, "p_prod", DataType(shape=(1,)))
    p_sums=system.create_dat(oddeven, "p_sums", DataType(shape=(1,)))
    
    xlink=system.create_map("xlink", points, xvals, 1)
    ylink=system.create_map("ylink", points, yvals, 1)
    sumlink=system.create_map("sumlink", points, oddeven, 1)
    
    instDict={
        "xvals" : n,
        "yvals" : n,
        "points" : n,
        "oddeven" : 2,
        "p_x" : numpy.array( [[i*1.0] for i in range(n)] ),
        "p_y" : numpy.array( [[i*1.0] for i in range(n)] ),
        "p_prod" : numpy.array( [ [0.0] for i in range(n)] ),
        "xlink" : numpy.array( [[i] for i in range(n)] ),
        "ylink" : numpy.array( [[i] for i in range(n)] ),
        "sumlink" : numpy.array( [[ i%2 ] for i in range(n)] )
    }
    
    inst=SystemInstance(system, instDict)
    
    instOutput={
        "output" : {
            "final" : copy.copy(instDict)
        },
        **instDict
    }
    
    expected=[i*i*1.0 for i in range(n)]
    sumeven=sum( expected[0::2] )
    sumodd=sum( expected[1::2] )
    
    sys.stderr.write("exp=[{},{}]".format(sumeven,sumodd))
    
    instOutput["output"]["final"]["p_prod"] = numpy.array( [ [e] for e in expected ] )
    instOutput["output"]["final"]["p_sums"] = numpy.array( [ [sumeven], [sumodd] ] )

    code=Seq(
        ParFor(
            dot_product,
            points,
            p_x(READ, xlink, 0),
            p_y(READ, ylink, 0),
            p_prod(WRITE),
            p_sums(INC, sumlink, 0)
        ),
        CheckState(instOutput, "/output/final"),
    )
    code.on_bind_spec(sys)
    return (system,inst,code)

if __name__=="__main__":
    logging.basicConfig(level=4,style="{")
    
    (spec,inst,code)=build_system(8)

    code.execute(inst)
