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

def iota_sum(
    iota:seq_double,
    sum:seq_double
) -> None:
    sum[0] += iota[0]

def build_system(n:int) -> (SystemInstance,Statement):

    WRITE=AccessMode.WRITE
    READ=AccessMode.READ
    INC=AccessMode.INC
    RW=AccessMode.RW
    
    sys=SystemSpecification()
    
    sum=sys.create_mutable_global("sum", DataType(shape=(1,)))
    
    nodes=sys.create_set("cells")
    
    p_iota=sys.create_dat(nodes, "p_iota", DataType(shape=(1,)))
    
    instDict={
        "cells" : n,
        "p_iota" : numpy.array( [[i] for i in range(n)] ),
        "sum" : numpy.array( [0.0] )
    }
    
    instOutput={
        "output" : {
            "final" : copy.copy(instDict)
        },
        **instDict
    }
    
    instOutput["output"]["final"]["sum"] = numpy.array( [ n*(n-1)/2 ] )
    
    inst=SystemInstance(sys, instOutput)
    

    code=Seq(
        """
        sum[0] = 0
        """,
        ParFor(
            iota_sum,
            nodes,
            p_iota(READ),
            sum(INC)
        ),
        CheckState(instOutput, "/output/final"),
    )
    code.on_bind_spec(sys)
    return (sys,inst,code)

if __name__=="__main__":
    logging.basicConfig(level=4,style="{")
    
    (spec,inst,code)=build_system(8)

    code.execute(inst)
