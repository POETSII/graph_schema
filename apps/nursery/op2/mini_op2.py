import enum
import numpy
import functools
import typing
from typing import Union, Sequence, List, Tuple, Optional, Dict

class AccessMode(enum.IntEnum):
    READ = 1
    WRITE = 2
    RW = 3
    INC = 4

class DataType(object):
    def __init__(self, type:Union[str,numpy.dtype]='double', shape:Union[int,Tuple[int,...]]=1) -> None:
        if isinstance(type,str):
            self.type=numpy.dtype(type)
        else:
            self.type=type
        if isinstance(shape,int):
            self.shape=(shape,) # type:Tuple[int,...]
        else:
            self.shape=shape # TODO : Canonicalise this?
        assert 0 < functools.reduce(lambda x,y: x*y, self.shape, 1), "Data type cannot be empty"
    
    def __eq__(self, other:object) -> bool:
        if isinstance(other,DataType):
            return self.type==other.type and self.shape==other.shape
        else:
            return False

class Global(object):
    def __init__(self, id:str, type:DataType) -> None:
        self.id=id
        self.data_type=type

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
        
class System(object):
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
   
