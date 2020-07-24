import typing
from typing import Any, Dict, List

import io
import numpy

from mini_op2.framework.core import *

from contextlib import contextmanager


from graph.core import *
from graph.save_xml import *

from lxml import etree


_scalar_type_map={
    numpy.dtype("float64"):"double",
    numpy.dtype("uint32"):"uint32_t",
    numpy.dtype("uint16"):"uint16_t",
    numpy.dtype("uint8"):"uint8_t",
    numpy.double:"double",
    numpy.float:"float",
    int:"int32_t",
    numpy.int32:"int32_t",
    numpy.uint32:"uint32_t",
    numpy.uint16:"uint16_t",
    numpy.uint8:"uint8_t"
}

def import_data_type(name:str,ot:DataType) -> TypedDataSpec:
    if ot.shape==():
        return ScalarTypedDataSpec(name,_scalar_type_map[ot.dtype])
    elif len(ot.shape)==1:
        return ArrayTypedDataSpec(name,ot.shape[0],ScalarTypedDataSpec("_", _scalar_type_map[ot.dtype]))
    #elif len(ot.shape)==2 and ot.shape[1]==1:
    #    return ArrayTypedDataSpec(name,ot.shape[0],ScalarTypedDataSpec("_", _scalar_type_map[ot.dtype]))
    elif len(ot.shape)==2 and ot.shape[1]>=1:
        # Actually a 2d array...
        return ArrayTypedDataSpec(name, ot.shape[0],
            ArrayTypedDataSpec("_", ot.shape[1],
                ScalarTypedDataSpec("_", _scalar_type_map[ot.dtype])
            )
        )
    else:
        raise RuntimeError("data type not supported yet.")
        
def import_data_type_tuple(name:str, members:Sequence[DataType]):
    return TupleTypedDataSpec(name, members)

class raw(object):
    def __init__(self,value):
        self.value=value

    def __add__(self,o):
        value=self.value
        if isinstance(o,raw):
            value+=o.valuse
        elif isinstance(o,str):
            value+=o
        else:
            raise RuntimeError("Can't add these together.")
        return raw(value)
    
    def __iadd__(self,o):
        if isinstance(o,raw):
            self.value+=o.value
        elif isinstance(o,str):
            self.value+=o
        else:
            raise RuntimeError("Can't add these together.")
        return self

class DeviceTypeBuilder(object):
    def __init__(self, id:str):
        self.id=id
        self.rts=""
        self.properties={} # type:Dict[str,TypedDataSpec]
        self.state={} # type:Dict[str,TypedDataSpec]
        self.inputs={} # type:Dict[str,Tuple[str,str,str]]
        self.outputs={} # type:Dict[str,Tuple[str,str,str]]
        self.shared_code=[]

    def add_property(self, name:str, type:DataType):
        assert name not in self.properties
        self.properties[name]=import_data_type(name,type)
        
    def add_state(self, name:str, type:DataType):
        assert name not in self.state
        self.state[name]=import_data_type(name,type)
    
    def add_shared_code(self, code:str):
        self.shared_code.append(code)
        
    def merge_state(self, name:str, type:DataType):
        type=import_data_type(name,type)
        if name not in self.state:
            self.state[name]=type
        else:
            assert type==self.state[name]
        
    def add_input_pin(self, name:str, msgType:str, properties:Optional[Any], state:None, body:str):
        assert name not in self.inputs
        assert state is None
        if properties:
            properties=[ import_data_type(n,t) for (n,t) in properties.items() ]
        self.inputs[name]=(name, msgType, properties, body)
    
    def extend_input_pin_handler(self, name:str, code:str):
        assert name in self.inputs
        (name,msgType,properties, body)=self.inputs[name]
        self.inputs[name]=(name,msgType,properties, body+code)
        
    def add_output_pin(self, name:str, msgType:str, body:str):
        assert name not in self.outputs
        self.outputs[name]=(name, msgType, body)
    
    def add_rts_clause(self, body:str):
        self.rts += body
    
    def build(self, graph:GraphType) -> DeviceType:
        device_properties=import_data_type_tuple("_", self.properties.values())
        assert isinstance(device_properties,TypedDataSpec)
        device_state=import_data_type_tuple("_", self.state.values())
        assert isinstance(device_state,TypedDataSpec)
        d=DeviceType(graph, self.id, device_properties, device_state)
        d.ready_to_send_handler=self.rts
        d.shared_code=self.shared_code
        for (name,msgType,properties,handler) in self.inputs.values():
            message_type=graph.message_types[msgType]
            input_properties=None
            if properties:
                input_properties=import_data_type_tuple("_", properties)
            input_state=None
            d.add_input(name,message_type,input_properties,input_state,None,handler)
        for (name,msgType,handler) in self.outputs.values():
            message_type=graph.message_types[msgType]
            output_properties=None
            output_state=None
            d.add_output(name,message_type,None,handler)
        return d

class GraphTypeBuilder:
    _subst={} # type:Dict[str,str]

    @contextmanager
    def subst(self, **kwargs):
        old_vals={}
        for (k,v) in kwargs.items():
            if k in self._subst:
                old_vals[k]=self._subst[k]
            self._subst[k]=v
        
        yield
        
        for k in kwargs:
            if k in old_vals:
                self._subst[k]=old_vals[k]
            else:
                del self._subst[k]
            
    def s(self, x:Union[str,raw], **kwargs) -> str:
        if isinstance(x,raw):
            return x.value
        else:
            if len(kwargs)>0:
                with self.subst(**kwargs):
                    return self.s(x)
            else:
                try:
                    return x.format(**self._subst)
                except:
                    logging.error("Exception while formatting '%s' with dictionary %s", x, self._subst)
                    raise
    
    def __init__(self, id:str) -> None:
        self.id=id
        self.properties={} # type:Dict[str,TypedDataSpec]
        self.device_types={} # type:Dict[str,DeviceTypeBuilder]
        self.message_types={} # type:Dict[str,Tuple[str,TypedDataSpec]]
        self.shared_code=[] # type:List[str]
    
    def create_device_type(self, devType:str) -> str :
        devType=self.s(devType)
        assert str not in self.device_types
        self.device_types[devType]=DeviceTypeBuilder(devType)
        return devType
        
    def create_message_type(self, msgType:str, members:Dict[str,DataType]) -> str :
        msgType=self.s(msgType)
        assert msgType not in self.message_types
        members=[ import_data_type(self.s(n),t) for (n,t) in members.items() ]
        self.message_types[msgType]=(msgType, members)
        return msgType
        
    def merge_message_type(self, msgType:str, members:Dict[str,DataType]) -> str :
        msgType=self.s(msgType)
        members=[ import_data_type(self.s(n),t) for (n,t) in members.items() ]
        if msgType not in self.message_types:
            self.message_types[msgType]=(msgType, members)
        else:
            assert (msgType, members)==self.message_types[msgType], "name={}, original type = {}, new type = {}".format(msgType, str(self.message_types[msgType][1]),str(members))
        return msgType
        
    def add_device_property(self, devType:str, name:str, type:DataType) -> None :
        devType=self.device_types[ self.s(devType) ]
        devType.add_property(self.s(name), type)
        
    def add_graph_property(self, name:str, type:DataType) -> None :
        name=self.s(name)
        assert name not in self.properties
        self.properties[name]=import_data_type(name,type)

        
    def add_device_state(self, devType:str, name:str, type:DataType) -> None :
        devType=self.device_types[ self.s(devType) ]
        devType.add_state(self.s(name), type)
        
    def merge_device_state(self, devType, name:str, type:DataType) -> bool :
        """Adds the device state if it doesn't already exists. Checks that the type is the same."""
        devType=self.device_types[ self.s(devType) ]
        devType.merge_state(self.s(name), type)
        
    def add_input_pin(self, devType:str, name:str, msgType:str, properties:None, state:None, body:str) -> None:
        devType=self.s(devType)
        msgType=self.s(msgType)
        name=self.s(name)
        body=self.s(body)
        assert devType in self.device_types
        assert msgType in self.message_types
        self.device_types[devType].add_input_pin(name, msgType, properties, state, body)
        
    def extend_input_pin_handler(self, devType:str, pinName:str, code:str) -> None:
        devType=self.s(devType)
        pinName=self.s(pinName)
        code=self.s(code)
        self.device_types[devType].extend_input_pin_handler(pinName, code)

    def add_output_pin(self, devType:str, name:str, msgType:str, body:str) -> None:
        devType=self.s(devType)
        msgType=self.s(msgType)
        name=self.s(name)
        body=self.s(body)
        assert devType in self.device_types
        assert msgType in self.message_types
        self.device_types[devType].add_output_pin(name, msgType, body)

    def add_rts_clause(self, devType:str, body:str) -> None:
        devType=self.s(devType)
        body=self.s(body)
        assert devType in self.device_types
        self.device_types[devType].add_rts_clause(body)

    def add_shared_code_raw(self, code:str) -> None:
        self.shared_code.append(code)

    def add_shared_code(self, code:str) -> None:
        self.shared_code.append(self.s(code))

    def add_device_shared_code(self, devType:str, code:str) -> None:
        devType=self.s(devType)
        code=self.s(code)
        self.device_types[devType].add_shared_code(code)
        
    def add_device_shared_code_raw(self, devType:str, code:str) -> None:
        self.device_types[devType].add_shared_code(code)

    def build(self) -> GraphType:
        graph_properties=import_data_type_tuple("_", self.properties.values())
        
        graph=GraphType(self.id, graph_properties)
        for s in self.shared_code:
            graph.shared_code.append(s)
        
        for (name,type) in self.message_types.values():
            graph.add_message_type(MessageType(graph, name, import_data_type_tuple("_", type)))
            
        for db in self.device_types.values():
            graph.add_device_type(db.build(graph))
        
        return graph

    def build_and_render(self) -> str:
        graph=self.build()
        
        nsmap = { None : "https://poets-project.org/schemas/virtual-graph-schema-v2", "p":"https://poets-project.org/schemas/virtual-graph-schema-v2" }
        def toNS(t):
            tt=t.replace("p:","{"+nsmap["p"]+"}")
            return tt
        
        root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)
        xml=save_graph_type(root, graph)
        
        b=etree.tostring(root, encoding="utf-8", pretty_print=True, xml_declaration=True)
        return b.decode("utf-8")
        
        
        
