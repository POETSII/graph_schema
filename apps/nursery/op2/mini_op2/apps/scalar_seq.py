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

def build_system(n:int) -> (SystemInstance,Statement):

    WRITE=AccessMode.WRITE
    READ=AccessMode.READ
    INC=AccessMode.INC
    RW=AccessMode.RW
    
    sys=SystemSpecification()
    
    x=sys.create_mutable_global("x", DataType(shape=(1,)))
    
    
    instDict={
        "x" : numpy.array( [0.0] )
    }
    
    inst=SystemInstance(sys, instDict)
    
    instOutput={
        "output" : {
            "final" : copy.copy(instDict)
        },
        **instDict
    }
    
    instOutput["output"]["final"]["x"] = numpy.array( [ 1.0 ] )

    code=Seq(
        """
        x[0] = 1
        """,
        CheckState(instOutput, "/output/final"),
    )
    code.on_bind_spec(sys)
    return (sys,inst,code)

if __name__=="__main__":
    logging.basicConfig(level=4,style="{")
    
    (spec,inst,code)=build_system(8)

    code.execute(inst)
