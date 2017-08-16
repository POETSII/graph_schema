import enum
import numpy
import functools
import typing

from typing import Union, Sequence, List, Tuple, Optional, Dict, Any
from abc import ABC, abstractmethod

import h5py

class AccessMode(enum.IntEnum):
    READ = 1
    WRITE = 2
    RW = 3
    INC = 4

class DataType(object):
    def __init__(self, dtype:Union[str,numpy.dtype]='double', shape:Union[int,Tuple[int,...]]=1) -> None:
        if isinstance(type,str):
            self.dtype=numpy.dtype(type)
        else:
            self.dtype=type
        if isinstance(shape,int):
            self.shape=() # type:Tuple[int,...]
        else:
            self.shape=shape # TODO : Canonicalise this?
        self.size=functools.reduce(lambda x,y: x*y, self.shape, 1)
        assert self.size>0, "Data type cannot be empty"
    
    @property
    def is_scalar(self) -> bool:
        return self.size==1
    
    def create_default_value(self) -> numpy.ndarray:
        return numpy.zeros(self.shape,dtype=self.dtype)
        
    def import_value(self, value:Any) -> numpy.ndarray:
        res=numpy.array(value,dtype=self.dtype)
        if res.shape!=self.shape:
            raise RuntimeError("Expected shape of {}, got shape of {}".format(self.shape,res.shape))
    
    def __eq__(self, other:object) -> bool:
        if isinstance(other,DataType):
            return self.dtype==other.dtype and self.shape==other.shape
        else:
            return False

class Global(ABC):
    def __init__(self, id:str, type:DataType) -> None:
        self.id=id
        self.data_type=type

    @abstractmethod
    def __call__(self, access_mode:AccessMode) -> GlobalArgument:
        raise NotImplementedError()
        

class ConstGlobal(Global):
    def __init__(self, id:str, type:DataType) -> None:
        super().__init__(id, type)

    def __call__(self, access_mode:AccessMode) -> GlobalArgument:
        if access_mode!=AccessMode.READ:
            raise RuntimeError("Attempt to access constant global {} using mode {}".format(self.id,access_mode))
        return GlobalArgument(access_mode, self)
        
class MutableGlobal(Global):
    def __init__(self, id:str, type:DataType) -> None:
        super().__init__(id, type)

    def __call__(self, access_mode:AccessMode) -> GlobalArgument:
        return GlobalArgument(access_mode, self)


class Set(object):
    def __init__(self, id:str) -> None:
        self.id=id
        self.dats={} # type:Dict[str,Dat]
        
    def add_dat(self, dat:'Dat') -> None:
        assert dat.id not in self.dats
        self.dats[dat.id]=dat
            
        
class Dat(object):
    def __init__(self, set:Set, id:str, type:DataType) -> None:
        self.id=id
        self.set=set
        self.data_type=type
        set.add_dat(self)
        
    def __call__(self, access_mode, map:Optional[Map]=None, index:int=0) -> DatArgument:
        if map==None:
            return DirectDatArgument(access_mode, self)
        else:
            return IndirectDatArgument(access_mode, self, map, index)
    
class Map(object):
    def __init__(self, id:str, iter_set:Set, to_set:Set, arity:int) -> None:
        assert arity>0
        self.id=id
        self.iter_set=iter_set
        self.to_set=to_set
        self.arity=arity
        
class SystemSpecification(object):
    def __init__(self):
        self._ids=set()
        self.globals={}
        self.sets={}
        self.dats={}
        self.maps={}
        
    def add_global(self, global_:Global):
       assert global_.id not in self._ids
       self._ids.add(global_.id)
       self.globals[global_.id]=global_

    def add_set(self, set:Set):
        assert set.id not in self._ids
        self._ids.add(set.id)
        self.sets[id]=set
        
    def add_dat(self, dat:Dat):
        assert dat.set.id in self.sets
        assert dat.id not in self._ids
        self._ids.add(dat.id)
        self.dats[id]=dat
        
    def add_map(self, map:Map):
        assert map.iter_set.id in self.sets
        assert map.to_set.id in self.sets
        assert map.id not in self._ids
        self._ids.add(map.id)
        self.maps[id]=map

class KernelParameter(object):
    def __init__(self, name:str, access_mode:AccessMode, type:DataType) -> None:
        self.name=name
        self.data_type=type
        self.access_mode=access_mode

class Kernel(object):
    def __init__(self, id:str, parameters:List[KernelParameter] ) -> None:
        self.id=id
        self.parameters=parameters
        
class Argument(object):
    def __init__(self, access_mode:AccessMode, type:DataType) -> None:
        self._access_mode=access_mode
        self._type=type
        
    @property
    def access_mode(self) -> AccessMode:
        return self.access_mode
    
    @property
    def data_type(self) -> DataType:
        return self.data_type

class GlobalArgument(Argument):
    def __init__(self, access_mode:AccessMode, global_:Global) -> None:
        super().__init__(access_mode, global_.data_type)
        self.global_=global_
        
class DatArgument(Argument):
    def __init__(self, access_mode:AccessMode, dat:Dat, iter_set:Set) -> None:
        super().__init__(access_mode, dat.data_type)
        self.dat=dat
        self.iter_set=iter_set

class DirectDatArgument(DatArgument):
    def __init__(self, access_mode:AccessMode, dat:Dat) -> None:
        super().__init__(access_mode, dat, dat.set)

class IndirectDatArgument(DatArgument):
    def __init__(self, access_mode:AccessMode, dat:Dat, map:Map, index:int=0) -> None :        
        assert dat.set==map.to_set
        assert 0 <= index < map.arity
        super().__init__(access_mode, dat, map.iter_set)
        self.map=map
        self.to_set=map.to_set # == dat.set
        self.index=index
   
        
class Statement(object):
    def __init__(self):
        pass
        
class Seq(Statement):
    def __init__(self,statements:Sequence[Statement]) -> None:
        self.statements=list(statements)
        
class Par(Statement):
    def __init__(self,statements:Sequence[Statement]) -> None:
        self.statements=list(statements)

class ParFor(Statement):
    def __init__(self,
        kernel:Kernel,
        iter_set:Set,
        arguments:Sequence[Argument]
    ) -> None :
        assert len(kernel.parameters)==len(arguments)
        for (i,a) in enumerate(arguments):
            if isinstance(a,DatArgument):
                assert a.iter_set==iter_set
            assert a.data_type==kernel.parameters[i].data_type, "Argument type not strictly equal to parameter type"
            assert a.access_mode==kernel.parameters[i].access_mode, "Argument access not strictly equal to parameter access"
        self.kernel=kernel
        self.iter_set=iter_set
        self.arguments=list(arguments)
   

class SystemInstance(object):
    def __init__(self, spec:SystemSpecification, src:Dict[str,Any]) -> None:
        """Given a system instance, load the state from hdf5 (well, a dictionary)
        
        Size of sets is given by scalar int with the same name, and must exist.
        
        Globals must match the data_type, and will be zero initialised if they don't exist.
        
        Maps are given by integer arrays of shape (len(set),arity), and must exist.
        
        Dats are optionally given by arrays of (len(set),*). If they don't exist, they will be zero initialised.
        """
        self.spec=spec
        self.globals={} # type:Dict[str,Global]
        self.sets={}    # type:Dict[str,int]
        
        for g in spec.globals.values():
            if g.id in src:
                value=g.data_type.import_value(src[g.id])
            else:
                value=g.data_type.create_default_value()
            self.globals[g.id]=value
        
        for s in spec.sets.values():
            if not s.id in src:
                raise RuntimeError("No value in dictionary called '{}' for set size.".format(s.id))
            raw=src[s.id]
            if isinstance(raw,int):
                val=raw
            else:
                val=int(numpy.array(raw))
            assert val>=0
            self.sets[s.id]=val
        
