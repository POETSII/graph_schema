import typing
from typing import Any, Dict, List

import sys

from contextlib import contextmanager

from mini_op2.framework.core import *
from mini_op2.framework.control_flow import Statement, ParFor
from mini_op2.framework.system import SystemSpecification

from graph.core import GraphInstance

import mini_op2.framework.sync_compiler
from mini_op2.framework.sync_compiler import sync_compiler, find_kernels_in_code, InvocationContext

scalar_uint32=DataType(shape=(),dtype=numpy.uint32)


def build_graph(globalDeviceType:str, spec:SystemSpecification, inst:SystemInstance, code:Statement): 
    graphTypeBuilder=sync_compiler(spec, code)
    graphType=graphTypeBuilder.build()
    
    graph=GraphInstance("op2")
    
    gi=graph.create_device_type("global", globalDeviceType)
    
    devices={}
    for set in spec.sets.values():
        assert False
    
    #kernels=find_kernels_in_code(code:Statement)    
    #for (i,stat) in enumerate(kernels):
    #    ctxt=InvocationContext(spec, stat)
    #    with builder.subst(invocation=ctxt.invocation):
            


if __name__=="__main__":
    logging.basicConfig(level=4)
    
    import mini_op2.apps.airfoil
    import mini_op2.apps.aero
    
    (spec,inst,code)=mini_op2.framework.sync_compiler.load_model(sys.argv)
    
    graph=
    
