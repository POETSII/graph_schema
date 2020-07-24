from typing import Union, Sequence, List, Tuple, Optional, Dict, Any, Iterator
from abc import ABC, abstractmethod

import numpy
import sys

from mini_op2.framework.core import *
from mini_op2.framework.system import SystemInstance, SystemSpecification

from mini_op2.framework.user_code_parser import scan_code, VarUses

class Statement(ABC):
    def _eval_statements(self, instance:SystemInstance, stats:Sequence["Statement"]):
        for s in stats:
            s.execute(instance)
            
    def _import_statements(self, spec:SystemSpecification, stats:Sequence["Statement"]) -> Sequence["Statement"]:
        global _scalar_statement_unq
        res=[]
        for s in stats:
            if isinstance(s,str):
                uses=scan_code(spec,s)
                unq=_scalar_statement_unq
                _scalar_statement_unq+=1
                args=[ GlobalArgument(mode,spec.globals[var]) for (var,mode) in uses.get_args() ]
                name="controller_{}".format(unq)
                (func,ast)=uses.create_func_and_ast(name)
                stat=UserCode( func, ast, *args)
                stat.id=name
                res.append(stat)
            else:
                res.append(s)
        return res
        
    def _on_bind_spec_local(self, spec:SystemSpecification) -> None:
        pass
    
    def __init__(self):
        # Used to assign globally unique ids later one
        self.id=None # type: Optional[str]
        
    def on_bind_spec(self, spec:SystemSpecification) -> "Statement":
        self._on_bind_spec_local(spec)
        for s in self.children():
            s.on_bind_spec(spec)
        return self

    def children(self) -> Iterator["Statement"]:
        yield from []
        
    def all_statements(self) -> Iterator["Statement"]:
        yield self
        for s in self.children():
            yield from s.all_statements()

    @abstractmethod
    def execute(self, instance:SystemInstance) -> None:
        raise NotImplementedError()
    
class UsesStatement(Statement):
    """This is a stupid name. It is code that was turned from a string into a function for scalar code."""
    def __init__(self, uses:VarUses) -> None:
        self.uses=uses
    
    def children(self) -> Iterator[Statement]:
        pass
        
    def execute(self, instance:SystemInstance) -> None:
        self.uses.execute(instance)

_scalar_statement_unq=0

class CompositeStatement(Statement):
    
    def __init__(self):
        pass
    
        
class Seq(CompositeStatement):
    def __init__(self, *statements:Statement ) -> None:
        super().__init__()
        self.statements=list(statements) # type:Sequence[Statement]
        
    def _on_bind_spec_local(self, spec:SystemSpecification) -> None:
        self.statements=self._import_statements(spec, self.statements)
        
    def children(self) -> Iterator[Statement]:
        yield from self.statements
        
    def execute(self, instance:SystemInstance) -> None:
        self._eval_statements(instance, self.statements)
    
        
class Par(CompositeStatement):
    def __init__(self, *statements:Statement) -> None:
        super().__init__()
        self.statements=list(statements)
    
    def _on_bind_spec_local(self, spec:SystemSpecification) -> None:
        self.statements=self._import_statements(spec, self.statements)
        
    def children(self) -> Iterator[Statement]:
        yield from self.statements
    
        
    def execute(self, instance:SystemInstance) -> None:
        self._eval_statements(instance, self.statements)
        
class RepeatForCount(CompositeStatement):
    def __init__(self, count:int, variable:MutableGlobal, *statements:Statement) -> None:
        super().__init__()
        self.count=count
        self.variable=variable
        self.statements=list(statements)
        
    def _on_bind_spec_local(self, spec:SystemSpecification) -> None:
        self.statements=self._import_statements(spec, self.statements)
    
    def children(self) -> Iterator[Statement]:
        yield from self.statements

    
    def execute(self, instance:SystemInstance) -> None:
        for i in range(self.count):
            instance.globals[self.variable][0]=i # Update global
            self._eval_statements(instance, self.statements)
                

class Execute(Statement):
    def __init__(self):
        super().__init__()
        
    def _get_current(self, instance:SystemInstance, iter_index:Union[int,None], arg:Argument):
        """Get the current value of the argument.
        
        iter_index : Set index in a parallel context, None in a scalar context
        """
        if isinstance(arg,GlobalArgument):
            current=instance.globals[arg.global_]
        elif isinstance(arg,DirectDatArgument):
            assert iter_index is not None
            current=instance.dats[arg.dat][iter_index]
        elif isinstance(arg,IndirectDatArgument):
            assert iter_index is not None
            map=instance.maps[arg.map]
            if arg.index>=0:
                indirect_index=map[iter_index][arg.index]
                current=instance.dats[arg.dat][indirect_index]
            else:
                assert arg.index == -arg.map.arity
                dat=instance.dats[arg.dat]
                current=[ dat[map[iter_index][i]] for i in range(arg.map.arity) ]
                #print("indirect {} = {}".format(arg.dat.id,current))
        else:
            raise RuntimeError("Unknown arg type.")
        return current
    
    def _get_all_current(self, instance:SystemInstance, iter_index:Union[int,None], args:List[Argument]):
        return [self._get_current(instance, iter_index, arg) for arg in args ]
        
    def _arg_pre(self, instance:SystemInstance, arg:Argument, current:numpy.ndarray) -> numpy.ndarray:
        """For a given argument and value, prepare the input to the kernel.
        """
        if arg.access_mode==AccessMode.INC:
            if isinstance(arg,IndirectDatArgument) and arg.index < 0:
                return [arg.data_type.create_default_value() for x in current]
            else:
                return arg.data_type.create_default_value() # Create zeros
        elif arg.access_mode==AccessMode.WRITE:
            # Scramble current value
            if isinstance(arg,IndirectDatArgument) and arg.index < 0:
                for i in range(-arg.index):
                    numpy.copyto(current[i], arg.data_type.create_random_value()) # type:ignore
            else:
                numpy.copyto(current, arg.data_type.create_random_value()) # type:ignore
            return current
        elif arg.access_mode==AccessMode.READ:
            if isinstance(arg,IndirectDatArgument) and  arg.index < 0:
                return [current[i].copy() for i in range(len(current))]
            else:
                return current.copy()
        elif arg.access_mode==AccessMode.RW:
            return current
        else:
            raise RuntimeError("Unknown access mode.")
    
    def _all_args_pre(self, instance:SystemInstance, args:List[Argument], current:Sequence[numpy.ndarray]):
        res=[]
        for (i,(arg,val)) in enumerate(zip(args,current)):
            try:
                res.append(self._arg_pre(instance, arg, val))
            except Exception as e:
                raise RuntimeError("Couldn't prepare argument index {} with arg={} and val={}".format(i,arg,val)) from e
        return res
        
    def _arg_post(self, instance:SystemInstance, arg:Argument, current:numpy.ndarray, new:numpy.ndarray) -> None:
        """For a given argument and previous value, apply the result from the kernel."""
        if arg.access_mode==AccessMode.INC:
            if isinstance(arg,IndirectDatArgument) and arg.index < 0:
                for i in range(len(current)):
                    arg.data_type.inc_value(current[i], new[i])
            else:
                arg.data_type.inc_value(current, new)
        elif arg.access_mode==AccessMode.WRITE:
            pass # Should have been modified in place, otherwise left random
        elif arg.access_mode==AccessMode.READ:
            if isinstance(arg,IndirectDatArgument) and arg.index < 0:
                for i in range(len(current)):
                    assert (current[i]==new[i]).all()
            else:
                assert (current==new).all()
        elif arg.access_mode==AccessMode.RW:
            pass # Will have modified in place if it wanted to
        else:
            raise RuntimeError("Unknown access mode.")

    def _all_args_post(self, instance:SystemInstance, args:List[Argument], current:Sequence[numpy.ndarray], newVals:Sequence[numpy.ndarray]) -> None:
        for (arg,val,new) in zip(args,current,newVals):
            self._arg_post(instance, arg, val, new)

class While(Execute):
    def _init_func(self, spec:SystemSpecification):
        global _scalar_statement_unq
        code="_cond_[0] = "+self.expression
        uses=scan_code(spec,code)
        unq=_scalar_statement_unq
        _scalar_statement_unq+=1
        args=[ GlobalArgument(mode,spec.globals[var]) for (var,mode) in uses.get_args() ]
        name="controller_expr_{}".format(unq)
        (func,ast)=uses.create_func_and_ast(name)
        self.id=name
        self.expr_ast=ast
        self.expr_func=func
        self.arguments=args
    
    def __init__(self, expression:str, *statements:Statement) -> None:
        super().__init__()
        self.expression=expression
        self.statements=list(statements)
        self.id=None
        self.expr_ast=None
        self.expr_func=None
        self.arguments=None
        
    def children(self) -> Iterator[Statement]:
        yield from self.statements
        
        
    def _on_bind_spec_local(self, spec:SystemSpecification) -> None:
        self._init_func(spec)
        self.statements=self._import_statements(spec, self.statements)
    
    
    def execute(self, instance:SystemInstance) -> None:
        if self.expr_func is None:
            self._init_func(instance.spec)
        while True:
            current=self._get_all_current(instance, None, self.arguments)
            vals=self._all_args_pre(instance, self.arguments, current)
            self.expr_func(*vals)
            self._all_args_post(instance, self.arguments, current, vals)
            logging.info("globals= %s", ",".join([g.id for g in instance.globals]))
            val=instance.globals[instance.spec.globals["_cond_"]]
            if not val:
                break
            self._eval_statements(instance, self.statements)

class ParFor(Execute):
    def __init__(self,
        kernel:Callable[...,None],
        iter_set:Set,
        *arguments:Argument
    ) -> None :
        super().__init__()
        
        for (i,a) in enumerate(arguments):
            if isinstance(a,DatArgument):
                assert a.iter_set==iter_set
        if hasattr(kernel,"__name__"):
            self.name=kernel.__name__
        else:
            self.name="<unknown>"
        self.kernel=kernel
        self.iter_set=iter_set
        self.arguments=list(arguments)
   
    def execute(self, instance:'SystemInstance') -> None:
        iter_size=instance.sets[self.iter_set]
        try:
            for ci in range(iter_size):
                current=self._get_all_current(instance, ci, self.arguments)
                vals=self._all_args_pre(instance, self.arguments, current)
                self.kernel(*vals)
                self._all_args_post(instance, self.arguments, current, vals)
        except Exception as     e:
            raise RuntimeError("While executing kernel {} over set {}".format(self.name, self.iter_set.id)) from e

class UserCode(Execute):
    def __init__(self,
        code:Callable[...,None],
        ast:any,
        *arguments:Argument
    ) -> None :
        super().__init__()
        
        assert ast
        for (i,a) in enumerate(arguments):
            assert isinstance(a,GlobalArgument)
        self.code=code
        self.ast=ast
        self.arguments=list(arguments)
        
    def execute(self, instance:'SystemInstance') -> None:    
        current=self._get_all_current(instance, None, self.arguments)
        vals=self._all_args_pre(instance, self.arguments, current)
        self.code(*vals)
        self._all_args_post(instance, self.arguments, current, vals)

class Debug(Statement):
    def __init__(self,
        callback:Callable[[SystemInstance],None]
    ) -> None :
        super().__init__()
        self.callback=callback
        
    def execute(self, instance:'SystemInstance') -> None:    
        self.callback(instance)

class CheckState(Statement):
    def __init__(self,
        src:Any,
        pattern:str
    ) -> None:
        super().__init__()
        self.src=src
        self.pattern=pattern
    
    def execute(self, instance:'SystemInstance') -> None:
        globals={ g.id:instance.globals[g][0] for g in instance.spec.globals.values() }
        pattern=self.pattern.format(**globals)
        logging.debug("Checking for reference pattern {}".format(pattern))
        instance.check_snapshot_if_present(instance.spec, self.src, pattern)
