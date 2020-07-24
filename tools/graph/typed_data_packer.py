from graph.core import *

from typing import *
import struct
import functools

class DataPacker:
    def __init__(self):
        self.size=None

    def unpack(self, buffer : Union[bytes,bytearray]):
        return self.unpack_from(buffer, 0)

    def unpack_from(self, buffer : Union[bytes,bytearray], offset:int):
        raise NotImplementedError()

    def pack(self, buffer: bytearray, value:any):
        self.pack_info(buffer,0,value)

    def pack_into(self, buffer : bytearray, offset:int, value:any):
        raise NotImplementedError()

class TypedDataPacker(DataPacker):
    def __init__(self, spec : TypedDataSpec):
        super().__init__()
        self.spec=spec
        
    def _make_default(self):
        default=self.spec.create_default()
        db=bytearray(self.size)
        self.pack_into(db, 0, default )
        self.default_bytes=bytes(db)
        got_default=self.unpack_from(self.default_bytes, 0)
        assert default==got_default, "Default={}, got={}".format(default,got_default)

    def unpack_from(self, buffer : Union[bytes,bytearray], offset:int):
        raise NotImplementedError()

    def pack_into(self, buffer : bytearray, offset:int, value:any):
        raise NotImplementedError()

_scalar_props={
    "int8_t":(1,"<b",int),
    "uint8_t":(1,"<B",int),
    "int16_t":(2,"<h",int),
    "uint16_t":(2,"<H",int),
    "int32_t":(4,"<i",int),
    "uint32_t":(4,"<I",int),
    "int64_t":(8,"<q",int),
    "uint64_t":(8,"<Q",int),
    "float":(4,"<f",float),
    "double":(8,"<d",float)
}

class ScalarTypedDataPacker(TypedDataPacker):
    def __init__(self, spec : ScalarTypedDataSpec):
        super().__init__(spec)
        (self.size,format_string,self.python_type)=_scalar_props[spec.type]
        self.formatter=struct.Struct(format_string)
        assert(self.formatter.size==self.size)
        self._make_default()

    def unpack_from(self, buffer, offset ):
        return self.formatter.unpack_from(buffer, offset)[0]
    
    def pack_into(self, buffer, offset, value ):
        self.formatter.pack_into(buffer, offset, value)

class TupleTypedDataPacker(TypedDataPacker):
    def __init__(self, spec : TupleTypedDataSpec):
        super().__init__(spec)
        offset=0
        self.elements=[]
        for ss in spec.elements_by_index:
            sp=make_typed_data_packer(ss)
            self.elements.append( (ss.name,sp,offset) )
            offset+=sp.size
        self.elements_by_name={ n:(u,o) for (n,u,o) in self.elements }
        self.size = offset
        self._make_default()
        
    def unpack_from(self, buffer, offset ):
        return {n:u.unpack_from(buffer,offset+loff) for (n,u,loff) in self.elements}
            
    def pack_into(self, buffer, offset, value ):
        if value is None:
            return
        for (n,v) in value.items():
            (u,loff)=self.elements_by_name[n]
            u.pack_into(buffer, offset+loff, v)

class ArrayTypedDataPacker(TypedDataPacker):
    def __init__(self, spec:ArrayTypedDataSpec):
        super().__init__(spec)
        self.length=spec.length
        assert isinstance(spec.type, TypedDataSpec)
        self.elt_packer=make_typed_data_packer(spec.type)
        self.size=self.length*self.elt_packer.size
        self._make_default()

    def unpack_from(self, buffer, offset):
        ep=self.elt_packer
        es=ep.size
        return [ ep.unpack_from(buffer, offset+i*es) for i in range(self.length) ]

    def pack_into(self, buffer, offset, value):
        if value is None:
            return
        assert len(value)==self.length
        ep=self.elt_packer
        es=ep.size
        for (i,v) in enumerate(value):
            ep.pack_into(buffer, offset+i*es, v)

def make_typed_data_packer(spec : TypedDataSpec) -> TypedDataPacker :
    if isinstance(spec, ScalarTypedDataSpec):
        return ScalarTypedDataPacker(spec)
    elif isinstance(spec, TupleTypedDataSpec):
        return TupleTypedDataPacker(spec)
    elif isinstance(spec, ArrayTypedDataSpec):
        return ArrayTypedDataPacker(spec)
    elif isinstance(spec, Typedef):
        return make_typed_data_packer(spec.type)
    else:
        raise NotImplementedError("Type not supported for data packing. I bet this is a union... "+spec)

class UnicastMessagePacker(DataPacker):
    def __init__(self, payload:TypedDataSpec):
        super().__init__()
        # TODO : This needs to match the PIP
        self.header=struct.Struct("<IIHH")
        self.payload=payload
        self.size=self.header.size + self.payload.size
    
    def pack_into(self, buffer:bytearray, offset:int, value:any ):
        header=(
            int(value["dstDevA"]),
            int(value["srcDevA"]),
            int(value["dstPortI"]),
            int(value["srcPortI"])
        )
        self.header.pack_into(buffer, offset, header)
        self.payload.pack_into(buffer, offset+self.header.size, value["payload"])
    
    def unpack_from(self, buffer, offset):
        value={}
        ( value["dstDevA"], value["srcDevA"], value["dstPortI"], value["srcPortI"] ) = self.header.unpack_from(offset)
        value["payload"]=self.payload.unpack_from(buffer, offset+self.header.size)
        return value
