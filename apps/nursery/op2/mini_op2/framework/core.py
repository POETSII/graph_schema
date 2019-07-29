import enum
import numpy
import functools
import typing
import logging

from typing import Union, Sequence, List, Tuple, Optional, Dict, Any, Callable
from abc import ABC, abstractmethod

import h5py # type:ignore

class AccessMode(enum.IntEnum):
    READ = 1
    WRITE = 2
    RW = 3
    INC = 4

class DataType(object):
    def __init__(self, dtype:Union[str,numpy.dtype]='double', shape:Tuple[int,...]=(1,)) -> None:
        if isinstance(dtype,str):
            self.dtype=numpy.dtype(dtype)
        else:
            self.dtype=dtype
        if isinstance(shape,int):
            self.shape=(shape,) # type:Tuple[int,...]
        else:
            self.shape=shape # TODO : Canonicalise this?
        self.size=functools.reduce(lambda x,y: x*y, self.shape, 1)
        assert self.size>0, "Data type cannot be empty"
    
    @property
    def is_scalar(self) -> bool:
        return self.size==1
    
    def create_default_value(self) -> numpy.ndarray:
        return numpy.zeros(self.shape,dtype=self.dtype)
        
    def create_random_value(self) -> numpy.ndarray:
        rr=numpy.random.random(self.shape) # type:ignore
        return numpy.array(rr,dtype=self.dtype)
        
    def inc_value(self, dst:numpy.ndarray, src:numpy.ndarray) -> None:
        dst += src
        
    def import_value(self, value:Any) -> numpy.ndarray:
        res=numpy.array(value,dtype=self.dtype)
        assert self.size == res.size
        # Load scalars
        #if res.shape==(1,) and self.shape==():
        #    res=res.reshape(self.shape)
        # Vectors of scalars
        #if len(res.shape)==2 and res.shape[1]==1 and len(self.shape)==1:
        #    res=res.reshape(self.shape)
        if res.shape!=self.shape:
            raise RuntimeError("Expected shape of {}, got shape of {}".format(self.shape,res.shape))
        return res
    
    def __eq__(self, other:object) -> bool:
        if isinstance(other,DataType):
            return self.dtype==other.dtype and self.shape==other.shape
        else:
            return False
                
scalar_double = DataType("double", (1,))
scalar_uint32 = DataType("uint32", (1,))

class Global(ABC):
    def __init__(self, id:str, type:DataType) -> None:
        self.id=id
        self.data_type=type

    @abstractmethod
    def __call__(self, access_mode:AccessMode) -> 'GlobalArgument':
        raise NotImplementedError()
        

class ConstGlobal(Global):
    def __init__(self, id:str, type:DataType=DataType()) -> None:
        super().__init__(id, type)

    def __call__(self, access_mode:AccessMode) -> 'GlobalArgument':
        if access_mode!=AccessMode.READ:
            raise RuntimeError("Attempt to access constant global {} using mode {}".format(self.id,access_mode))
        return GlobalArgument(access_mode, self)
        
class MutableGlobal(Global):
    def __init__(self, id:str, type:DataType=DataType()) -> None:
        super().__init__(id, type)

    def __call__(self, access_mode:AccessMode) -> 'GlobalArgument':
        return GlobalArgument(access_mode, self)


class Set(object):
    def __init__(self, id:str) -> None:
        self.id=id
        self.dats={} # type:Dict[str,Dat]
        
    def __call__(self, access_mode:AccessMode) -> 'Argument':
        if access_mode!=AccessMode.LENGTH:
            raise RuntimeError("Sets only support length access.")
        return LengthArgument(self)

        
    def add_dat(self, dat:'Dat') -> None:
        assert dat.id not in self.dats
        self.dats[dat.id]=dat
            
        
class Dat(object):
    def __init__(self, set:Set, id:str, type:DataType) -> None:
        self.id=id
        self.set=set
        self.data_type=type
        set.add_dat(self)
        
    def __call__(self, access_mode, map:Optional['Map']=None, index:int=0) -> 'DatArgument':
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
        


class Parameter(object):
    def __init__(self, name:str, access_mode:AccessMode, type:DataType) -> None:
        self.name=name
        self.data_type=type
        self.access_mode=access_mode
        
        
class ScalarLogic(object):
    def __init__(self, id:str, parameters:List[Parameter] ) -> None:
        self.id=id
        self.parameters=parameters

        
class Argument(object):
    def __init__(self, access_mode:AccessMode, type:DataType) -> None:
        self._access_mode=access_mode
        self._type=type
        
    @property
    def access_mode(self) -> AccessMode:
        return self._access_mode
    
    @property
    def data_type(self) -> DataType:
        return self._type

class GlobalArgument(Argument):
    def __init__(self, access_mode:AccessMode, global_:Global) -> None:
        super().__init__(access_mode, global_.data_type)
        self.global_=global_
        
    def __repr__(self) -> str:
        return "{}({})".format(self.access_mode.name, self.global_.id)
        
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
        assert (0 <= index < map.arity) or (index == -map.arity)
        super().__init__(access_mode, dat, map.iter_set)
        self.map=map
        self.to_set=map.to_set # == dat.set
        self.index=index
        
    def __repr__(self) -> str:
        return "{}({}[{}][{}])".format(self.access_mode.name, self.to_set.id, self.map.id, self.index)

