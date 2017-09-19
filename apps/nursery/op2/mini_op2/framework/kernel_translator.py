import typing
from typing import Any, Dict, List, Sequence

import sys
import numpy
import types
import logging
import ast
import io
import random

from contextlib import contextmanager

def extract_loop_range(e:ast.expr) -> int:
    """Extract the upper loop range of an expression range(constant)"""
    if not isinstance(e,ast.Call):
        raise RuntimeError("Expected expression of type Call(...)")
    func=e.func
    if not isinstance(func,ast.Name):
        raise RuntimeError("Expected expression of type Str(...)")
    if func.id!="range":
        raise RuntimeError("Expected expression of type 'range'(...)")
    if len(e.args)!=1:
        raise RuntimeError("Expected expression of type 'range'(expr)")
    arg=e.args[0]
    if not isinstance(arg,ast.Num):
        raise RuntimeError("Expected expression of type 'range'(Num)")
    return arg.n
    
def extract_loop_var(e:ast.expr) -> str:
    """Extract the loop variable name for a for loop"""
    if not isinstance(e,ast.Name):
        raise RuntimeError("Expected string loop target")
    return e.id

_translate_operator={
    "Add" : "+",
    "Sub" : "-",
    "Mult" : "*",
    "Div" : "/",
    "Mod" : "%",
    "Eq" : "==",
    "Lt" : "<",
    "Gt" : ">",
    "UAdd" : "+",
    "USub" : "-"
}

def translate_operator(e:ast.operator) -> str:
    return _translate_operator[type(e).__name__]

_translate_function_call={
    "sqrt" : "std::sqrt",
    "fabs" : "std::fabs",
    "pow" : "std::pow",
    "handler_log" : "handler_log",
}

def translate_function_call_expr(e:ast.expr) -> str:
    if not isinstance(e,ast.Name):
        raise RuntimeError("Expected expression of type 'Name', got '{}'".format(type(e)))
    return _translate_function_call[e.id]

def translate_expression(e:ast.expr) -> str:
    if isinstance(e, ast.Name):
        return e.id
    elif isinstance(e, ast.Num):
        return str(e.n)
    elif isinstance(e, ast.Str):
        return str('"'+e.s+'"')
    elif isinstance(e, ast.BinOp):
        left=translate_expression(e.left)
        op=translate_operator(e.op)
        right=translate_expression(e.right)
        return "({}{}{})".format(left,op,right)
    elif isinstance(e, ast.UnaryOp):
        operand=translate_expression(e.operand)
        op=translate_operator(e.op)
        return "({}{})".format(op,operand)
    elif isinstance(e, ast.Compare):
        assert len(e.ops)==1
        assert len(e.comparators)==1
        left=translate_expression(e.left)
        op=translate_operator(e.ops[0])
        right=translate_expression(e.comparators[0])
        return "( {} {} {} )".format(left,op,right)
    elif isinstance(e, ast.Subscript):
        value=translate_expression(e.value)
        # TODO : Do we need to look at e.context?
        if not isinstance(e.slice, ast.Index):
            raise RuntimeError("Only index slices are supported.")
        index=translate_expression(e.slice.value)
        return "({}[{}])".format(value,index)
    elif isinstance(e, ast.Call):
        func=translate_function_call_expr(e.func)
        assert len(e.keywords)==0
        args=[translate_expression(arg) for arg in e.args]
        return "( {}({}) )".format(func, ",".join(args))
    elif isinstance(e, ast.List):
        n=len(e.elts)
        init=[ translate_expression(ei) for ei in e.elts ]
        return "std::array<double,{}>{{ {} }}".format(n, " , ".join(init))
    else:
        raise RuntimeError("Unsupported expression {}".format(type(e)))

def translate_statement(f:ast.stmt, dst:io.TextIOBase, vars:set, indent="") -> None:
    unq=random.randint(0,2**32)
    if isinstance(f, ast.Assign):
        assert len(f.targets)==1, "Can't do tuple assignment."
        # Auto-declare direct scalars the first time they are assigned
        # TODO : This is buggy, as if a variable is first references in a local scope, it won't
        # be available outside
        decl=""
        if isinstance(f.targets[0],ast.Name):
            if f.targets[0].id not in vars:
                decl="auto "
                vars.add(f.targets[0].id)
        dst.write("{}{}{} = {};\n".format(indent, decl, translate_expression(f.targets[0]), translate_expression(f.value)))
    elif isinstance(f, ast.Pass):
        pass
    elif isinstance(f, ast.AugAssign):
        op=translate_operator(f.op)
        dst.write("{}{} {}= {};\n".format(indent, translate_expression(f.target), op, translate_expression(f.value)))
    elif isinstance(f, ast.If):
        dst.write("{}{{\n".format(indent))
        dst.write("{}  bool cond_{}=({});\n".format(indent, unq, translate_expression(f.test)))
        dst.write("{}  if(cond_{}){{\n".format(indent, unq))
        for s in f.body:
            translate_statement(s, dst, vars, indent+"    ")
        dst.write("{}  }}else{{\n".format(indent))
        for s in f.orelse:
            translate_statement(s, dst, vars, indent+"    ")
        dst.write("{}  }}\n".format(indent))
        dst.write("{}}}\n".format(indent))
    elif isinstance(f, ast.For):
        var=extract_loop_var(f.target)
        count=extract_loop_range(f.iter)
        dst.write("{}for(unsigned {var}=0; {var}<{count}; {var}++){{\n".format(indent,var=var, count=count))
        for s in f.body:
            translate_statement(s, dst, vars, indent+"  ")
        assert len(f.orelse)==0 # Who uses this construct???
        dst.write("{}}}\n".format(indent))
    elif isinstance(f, ast.Expr):
        dst.write("{}{};\n".format(indent, translate_expression(f.value)))
    else:
        raise RuntimeError("unsupported statement {}".format(type(f)))
    

def get_positional_arguments(f:ast.FunctionDef) -> Sequence[ast.arg]:
    assert f.args.vararg==None
    assert len(f.args.kwonlyargs)==0
    assert len(f.args.kw_defaults)==0
    assert f.args.kwarg==None
    assert len(f.args.defaults)==0
    return f.args.args

_to_c_type = {
    numpy.double : "double",
    numpy.uint32 : "uint32_t",
}    
    

def translate_function(f:ast.FunctionDef, dst:io.TextIOBase):
    # HACK : Moving to template types, to allow parameter types to float
    
    assert f
    args=get_positional_arguments(f)
    
    args_types=["class T{}".format(i) for i in range(len(args))]
    
    dst.write("template<{}>\n".format(",".join(args_types)))
    dst.write("void kernel_{}(\n".format(f.name))
    for (index,arg) in enumerate(args):
        #assert arg.annotation!=None, "Expecting type annotation"
        #type=arg.annotation
        #assert isinstance(type,ast.Name), "Expecting type annotation to be a name"
        #datatype=None        
        dst.write("  T{} {}".format(index,arg.arg))
        if index+1 < len(args):
            dst.write(",")
        dst.write("\n")
    dst.write("){\n")
    
    vars=set()
    for statement in f.body:
        translate_statement(statement, dst, vars, indent="  ")
    
    dst.write("}\n\n")

class TranslatorContext:
    def __init__(self, module:types.ModuleType) -> None:
        self.module=module
        if hasattr(module, "__file__"):
            self.source_file=module.__file__
            logging.debug("using source_file = %s"%(self.source_file))
            with open(self.source_file,"rt") as src:
                self.source_code=src.read()
            self.ast=ast.parse(self.source_code, filename=self.source_file)
        else:
            self.source_file="<unknown-file"
            
    
        assert isinstance(self.ast, ast.Module)
    
        self.functions={} # type:Dict[str,ast.FunctionDef]
        for n in self.ast.body:
            if isinstance(n,ast.FunctionDef):
                self.functions[n.name]=n
        
def kernel_to_c(module:types.ModuleType, name:str) -> str:
    ctxt=TranslatorContext(module)
    if name not in ctxt.functions:
        for n in ctxt.functions:
            sys.stderr.write("{}\n".format(n))
    n=ctxt.functions[name]
    tmp=io.StringIO()
    translate_function(n,tmp)
    return tmp.getvalue()
    
def scalar_to_c(f:ast.FunctionDef, name:str) -> str:
    tmp=io.StringIO()
    translate_function(f,tmp)
    return tmp.getvalue()
    

if __name__=="__main__":
    logging.basicConfig(level=4)
    
    ctxt=TranslatorContext(mini_op2.airfoil)
    
    airfoil_kernels=set(["save_soln", "adt_calc", "res_calc", "bres_calc", "update"])
    
    for k in airfoil_kernels:
        n=ctxt.functions[k]
        translate_function(n,sys.stdout)
        
