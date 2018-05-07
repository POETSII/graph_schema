import typing
from typing import Any, Dict, List, Sequence, Optional

import sys
import numpy
import types
import logging
import ast
import io
import random
import re

from types import ModuleType

from mini_op2.framework.core import *
from mini_op2.framework.system import SystemSpecification

from contextlib import contextmanager

READ=AccessMode.READ
WRITE=AccessMode.WRITE
RW=AccessMode.RW

    
def extract_loop_var(e:ast.expr) -> str:
    """Extract the loop variable name for a for loop"""
    if not isinstance(e,ast.Name):
        raise RuntimeError("Expected string loop target")
    return e.id

class VarUses(object):
    def __init__(self, spec:SystemSpecification, code:str):
        self.spec=spec
        self.uses={} # type:Dict[Global,AccessMode]
        self.others=set()
        self.code=code
        self.function_code=None
        self.function=None
        self.args=None
        
    def get_args(self):
        if self.args==None:
            self.args=sorted([ (global_.id,mode) for (global_,mode) in self.uses.items() ])
        return self.args
        
    def dump(self):
        for (g,a) in self.uses.items():
            print("Use : ",(g.id,a))
        for o in self.others:
            print("Other : '{}'".format(o))
            
    def create_func_source(self, name:str):
        # Ensure a consistent order
        args=sorted([ (global_.id,mode) for (global_,mode) in self.uses.items() ])
        source="""from numpy import sqrt, fabs, isnan\n"""
        source+=r"""
import sys
def fprintf_stderr(msg, *args):
    sys.stderr.write(msg % args)
    sys.stderr.write("\n")
    """

        source+="\n"
        source+="def {}(\n".format(name)
        for (i,(a,m)) in enumerate(args):
            source+="  {}".format(a)
            if i+1<len(args):
                source+=","
            source+="  # {}".format(m)
            source+="\n"
        source+=") : \n"
        for l in self.code.split("\n"):
            source +="    "+l+"\n"
        
        return source
        
    def create_func_and_ast(self, name:str):
        try:
            self.function_code=self.create_func_source(name)
            logging.info("Code = %s", self.function_code)
            m=compile(self.function_code,"No-file",'exec')
            
            a=ast.parse(self.function_code)
            f=None
            for s in a.body:
                if isinstance(s,ast.FunctionDef) and s.name==name:
                    f=s
                    break
            
            assert f
            
            module = ModuleType("module_"+name)
            exec(m, module.__dict__)
        except:
            sys.stderr.write(self.function_code)
            raise 
        
        return (getattr(module,name),f)
    
    def create_func(self, name:str):
        return self.create_func_and_ast(name)[0]
     
    def execute(self, inst:SystemSpecification):
        if self.function==None:
            self.function=self.create_func("auto_gen")
        aa = { a.id:inst.globals[a] for a in self.uses }
        try:
            return self.function( **aa )
        except:
            sys.stderr.write(self.function_code+"\n")
            raise
            
            
    def check_has_use(self, g:Global, mode:AccessMode) -> None:
        if g not in self.uses:
            raise RuntimeError("Expected to see global {} in uses.".format(g.id))
        curr=self.uses[g]
        if mode==AccessMode.RW:
            if curr!=mode:
                raise RuntimeError("Expected to see {} for global {}, got {}".format(mode, g.id, curr))
        elif mode==AccessMode.READ:
            if curr!=AccessMode.READ and curr!=AccessMode.RW:
                raise RuntimeError("Expected to see {} for global {}, got {}".format(mode, g.id, curr))
        elif mode==AccessMode.WRITE:
            if curr!=AccessMode.WRITE and curr!=AccessMode.RW:
                raise RuntimeError("Expected to see {} for global {}, got {}".format(mode, g.id, curr))

    def read(self, g:Global) -> Global:
        if g:
            curr=self.uses.setdefault(g,AccessMode.READ)
            if curr==AccessMode.READ or curr==AccessMode.RW:
                pass
            elif curr==AccessMode.WRITE:
                curr=AccessMode.RW
            else:
                # We don't really look for increment : it is covered by RW
                raise RuntimeError("Unexpected access mode for global.")
            self.uses[g]=curr
        return g
        
    def write(self, g:Optional[Global]) -> Global:
        if g:
            curr=self.uses.setdefault(g,AccessMode.WRITE)
            if curr==AccessMode.WRITE or curr==AccessMode.RW:
                pass
            elif curr==AccessMode.READ:
                curr=AccessMode.RW
            else:
                # We don't really look for increment : it is covered by RW
                raise RuntimeError("Unexpected access mode for global.")
            self.uses[g]=curr
        return g

_good_functions=set([
    "sqrt",
    "fabs",
    "print",
    "range",
    "handler_log",
    "isnan",
    "fprintf_stderr"
])

def scan_function_call_expr(uses:VarUses, f:ast.expr) -> None:
    if not isinstance(f,ast.Name):
        raise RuntimeError("Only Name exprs supported.")
    if f.id not in _good_functions:
        raise RuntimeError("Function {} is not allowed here.".format(f.id))


def scan_expression(uses:VarUses, e:ast.expr) -> Optional[Global]:
    if isinstance(e, ast.Name):
        if e.id in uses.spec.globals:
            return uses.spec.globals[e.id]
        else:
            return uses.others.add(e.id)
    elif isinstance(e, ast.Num):
        return None
    elif isinstance(e, ast.Str):
        return None
    elif isinstance(e, ast.Tuple):
        for x in e.elts:
            uses.read(scan_expression(uses, x))
    elif isinstance(e, ast.BinOp):
        uses.read(scan_expression(uses, e.left))
        uses.read(scan_expression(uses, e.right))
        return None
    elif isinstance(e, ast.BoolOp):
        for x in e.values:
            uses.read(scan_expression(uses, x))
        return None
    elif isinstance(e, ast.UnaryOp):
        uses.read(scan_expression(uses, e.operand))
        return None
    elif isinstance(e, ast.Compare):
        assert len(e.ops)==1
        assert len(e.comparators)==1
        uses.read(scan_expression(uses, e.left))
        uses.read(scan_expression(uses, e.comparators[0]))
        return None
    elif isinstance(e, ast.Subscript):
        value=scan_expression(uses, e.value)
        # TODO : Do we need to look at e.context?
        if not isinstance(e.slice, ast.Index):
            raise RuntimeError("Only index slices are supported.")
        uses.read(scan_expression(uses, e.slice.value))
        return value
    elif isinstance(e, ast.Call):
        scan_function_call_expr(uses, e.func) # Make sure it is a "normal" function
        assert len(e.keywords)==0
        for arg in e.args:
            uses.read(scan_expression(uses, arg))
        return None
    else:
        raise RuntimeError("Unsupported expression {}".format(type(e)))

def scan_statement(uses:VarUses, f:ast.stmt) -> None:
    assert uses
    if isinstance(f, ast.Assign):
        assert len(f.targets)==1, "Can't do tuple assignment."
        uses.read(scan_expression(uses, f.value))
        uses.write(scan_expression(uses, f.targets[0]))
    elif isinstance(f, ast.AugAssign):
        uses.read(scan_expression(uses, f.value))
        lhs=scan_expression(uses,f.target)
        uses.read(lhs)
        uses.write(lhs)
    elif isinstance(f, ast.Pass):
        pass
    elif isinstance(f, ast.If):
        uses.read(scan_expression(uses, f.test))
        for s in f.body:
            scan_statement(uses, s)
        for s in f.orelse:
            scan_statement(uses, s)
    elif isinstance(f, ast.For):
        var=extract_loop_var(f.target)
        assert var not in uses.spec.globals
        scan_expression(uses, f.iter)
        for s in f.body:
            scan_statement(uses, s)
        assert len(f.orelse)==0 # Who uses this construct???
    elif isinstance(f, ast.Expr):
        uses.read(scan_expression(uses, f.value))
    elif isinstance(f, ast.Return):
        uses.read(scan_expression(uses, f.value))
    else:
        raise RuntimeError("unsupported statement {}".format(type(f)))
    
    
def _strip_prefix(code:str) -> str :
    """Use the first non-empty line to define a whitespace prefix to strip from all lines"""
    lines=code.split("\n")
    while True:
        assert len(lines)>0
        if len(lines[0].strip())==0:
            lines=lines[1:]
        else:
            break
        
    m=re.match("^([ \t]*)",lines[0])
    prefix=m.group(1)
    for i in range(len(lines)):
        assert lines[i].startswith(prefix)
        lines[i]=lines[i][len(prefix):]
    return "\n".join(lines)
    


def scan_code(spec:SystemSpecification, code:str) -> VarUses:
    code=_strip_prefix(code)
    
    a=ast.parse(code)
    
    assert isinstance(a, ast.Module)

    uses=VarUses(spec, code)
    assert uses
    for s in a.body:
        scan_statement(uses, s)
    return uses


########################################################

import unittest

class TestParsing(unittest.TestCase):
    
    def test_examples(self):
        spec=SystemSpecification()
        c0=spec.create_const_global("c0")
        c1=spec.create_const_global("c1")
        c2=spec.create_const_global("c2")
        c3=spec.create_const_global("c3")
        
        
        u=scan_code(spec, "c0=1")
        u.check_has_use(c0, WRITE)

        u=scan_code(spec, "c0=c0")
        u.check_has_use(c0, RW)

        u=scan_code(spec, "c0+=4")
        u.check_has_use(c0, RW)

        u=scan_code(spec, "c0[0]+=4")
        u.check_has_use(c0, RW)
        
        u=scan_code(spec, "c0[0]+=4")
        u.check_has_use(c0, RW)
        
        u=scan_code(spec, "c0[c1]+=4")
        u.check_has_use(c0, RW)
        u.check_has_use(c1, READ)
        
        u=scan_code(spec, "c0[c1]+=c3")
        u.check_has_use(c0, RW)
        u.check_has_use(c1, READ)
        u.check_has_use(c3, READ)
        
        u=scan_code(spec, "c0[c1]=c3")
        u.check_has_use(c0, WRITE)
        u.check_has_use(c1, READ)
        u.check_has_use(c3, READ)
        
        u=scan_code(spec, "c0 = c3 < sqrt(c2)")
        u.check_has_use(c0, WRITE)
        u.check_has_use(c2, READ)
        u.check_has_use(c3, READ)
        
        u=scan_code(spec, """
    if(c0[0]):
        c1=3
    else:
        c2+=4
        """)
        u.check_has_use(c0, READ)
        u.check_has_use(c1, WRITE)
        u.check_has_use(c2, RW)
        
        u=scan_code(spec, """
    if(sqrt(c0[0])):
        c1=fabs(c3)
    else:
        c2+=4
        """)
        u.check_has_use(c0, READ)
        u.check_has_use(c1, WRITE)
        u.check_has_use(c2, RW)
        u.check_has_use(c3, READ)

        u=scan_code(spec, """
    for x in range(5):
        c0 += c1
        """)
        u.check_has_use(c0, RW)
        u.check_has_use(c1, READ)

        u=scan_code(spec, """
        acc=0
        for i in range(5):
            acc += c1[i]
        c2[0]=acc
        print(acc)
        """)
        u.check_has_use(c1, READ)
        u.check_has_use(c2, WRITE)
        
        t=u.create_func("t")
        t( c1=[0,1,2,3,4], c2=[0] )
    

        
if __name__=="__main__":
    unittest.main()
