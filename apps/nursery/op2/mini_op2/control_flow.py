from typing import Union, Sequence, List, Tuple, Optional, Dict, Any, Iterator
from abc import ABC, abstractmethod

import numpy
import sys

from mini_op2.core import *
from mini_op2.system import SystemInstance

from mini_op2.user_code_parser import scan_code, VarUses

class Statement(ABC):
    def __init__(self):
        pass
        
    def all_statements(self) -> Iterator['Statement']:
        yield self
        
    @abstractmethod
    def execute(self, instance:SystemInstance) -> None:
        raise NotImplementedError()
        
class CompositeStatement(Statement):
    def _eval_statements(self, instance:SystemInstance, stats:Sequence[Statement]):
        for s in stats:
            if isinstance(s,str):
                # TODO : Cache this
                uses=scan_code(instance.spec, s)
                uses.execute(instance)
            else:
                s.execute(instance)
    
    def __init__(self):
        pass
        
    @abstractmethod
    def children(self) -> Iterator[Statement]:
        raise NotImplementedError()
        
    def all_statements(self) -> Iterator[Statement]:
        yield self
        for s in self.children():
            yield from s.all_statements()
        
class Seq(CompositeStatement):
    def __init__(self, *statements:Statement ) -> None:
        self.statements=list(statements) # type:Sequence[Statement]
        
    def children(self) -> Iterator[Statement]:
        yield from self.statements
        
    def execute(self, instance:SystemInstance) -> None:
        self._eval_statements(self.statements)
    
        
class Par(CompositeStatement):
    def __init__(self, *statements:Statement) -> None:
        self.statements=list(statements)
        
    def children(self) -> Iterator[Statement]:
        yield from self.statements
    
        
    def execute(self, instance:SystemInstance) -> None:
        self._eval_statements(instance, self.statements)
        
class RepeatForCount(CompositeStatement):
    def __init__(self, count:int, variable:MutableGlobal, *statements:Statement) -> None:
        self.count=count
        self.variable=variable
        self.statements=list(statements)
    
    def children(self) -> Iterator[Statement]:
        yield from self.statements

    
    def execute(self, instance:SystemInstance) -> None:
        for i in range(self.count):
            instance.globals[self.variable][0]=i # Update global
            self._eval_statements(instance, self.statements)
                
                
class While(CompositeStatement):
    def __init__(self, expression:str, *statements:Statement) -> None:
        self.expression=expression
        self.statements=list(statements)
    
    def children(self) -> Iterator[Statement]:
        yield from self.statements
    
    def execute(self, instance:SystemInstance) -> None:
        # TODO : Cache this
        uses=scan_code(instance.spec, "return "+self.expression )
        while True:
            val=uses.execute(instance)
            sys.stderr.write("While cond = {}".format(val))
            if not val:
                break
            self._eval_statements(instance, self.statements)

class Execute(Statement):
    def __init__(self):
        pass
        
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
                current=[ instance.dats[arg.dat][i] for i in range(arg.map.arity) ]
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

class ParFor(Execute):
    def __init__(self,
        kernel:Callable[...,None],
        iter_set:Set,
        *arguments:Argument
    ) -> None :
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
        *arguments:Argument
    ) -> None :
        for (i,a) in enumerate(arguments):
            assert isinstance(a,GlobalArgument)
        self.code=code
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
        self.callback=callback
        
    def execute(self, instance:'SystemInstance') -> None:    
        self.callback(instance)

