from graph.core import *

import sys
import io

class CppTypeMap(object):
    
    def emit(self,pattern, *args):
        self._defsBuffer.write(pattern.format(*args)+"\n")
    
    def visit(self, t:TypedDataSpec, thisName:str, parent:str):
        if t is None:
            # Note: this is a point of ambiguity in spec. Traditionally it is an empty struct
            assert thisName, "Only expected for a named thing like message, state, or properties"
            self.emit("typedef _empty_struct_t {};", thisName)
            return
        
        if t in self.cTypeNames:
            return
        
        if isinstance(t, Typedef):
            name=t.id
            self.visit(t.type, None, name)
            self.emit("typedef {} {};", self.cTypeNames[t.type], name)
        elif isinstance(t, ScalarTypedDataSpec):
            if isinstance(t.type,Typedef):
                name=self.cTypeNames[t.type]
            else:
                name=t.type
        elif isinstance(t, TupleTypedDataSpec):
            name=t.cTypeName or thisName or parent+"_"+t.name+"_t" # As per PIP0007
            for e in t.elements_by_index:
                self.visit(e, None, name) # Emit defs for sub-types
            self.emit("#pragma pack(push,1)")
            self.emit("typedef struct{{")
            for e in t.elements_by_index:
                self.emit("  {} {};", self.cTypeNames[e], e.name)
            self.emit("}} {};", name)
            self.emit("#pragma pack(pop)\n")
        elif isinstance(t, ArrayTypedDataSpec):
            name=t.cTypeName or thisName or parent+"_"+t.name+"_t" # As per PIP0007
            self.visit(t.type, None, name)
            self.emit("typedef {} {}[{}];", self.cTypeNames[t.type], name, t.length)
        else:
            raise RuntimeError("TYpe {} isn't supported.".format(type(t)))
    
        self.cTypeNames[t]=name
    
    def __init__(self, gt:GraphType):
        # Maps 
        self.cTypeNames={} # Map[TypedDataSpec,str]
        self._defsBuffer=io.StringIO()
        
        emit=self.emit
        
        emit("#include <cstdint>")
        emit("")
        emit("struct _empty_struct_t {};")
        
        for td in gt.typedefs_by_index:
            self.visit(td,None,None)
            
        self.visit(gt.properties, "{}_properties_t".format(gt.id), None)
        
        for mt in gt.message_types.values():
            emit("// Message type {}", mt.id)
            self.visit(mt.message, "{}_{}_message_t".format(gt.id,mt.id), None)
            emit("")
            
        
        for dt in gt.device_types.values():
            emit("// Device type {}", dt.id)
            self.visit(dt.properties, "{}_{}_properties_t".format(gt.id,dt.id), None)
            emit("")
            self.visit(dt.state, "{}_{}_state_t".format(gt.id,dt.id), None)
            emit("")
            
            for ip in dt.inputs_by_index:
                emit("//   Input {} of {}", mt.id, dt.id)
                self.visit(ip.properties, "{}_{}_{}_properties_t".format(gt.id,dt.id,ip.name), None)
                emit("")
                self.visit(ip.state, "{}_{}_{}_state_t".format(gt.id,dt.id,ip.name), None)
                emit("")
            
            emit("")
        
        self.defs=self._defsBuffer.getvalue()
        self._defsBuffer.close()
        self._defsBuffer=None
            
    def get_c_type_ref(self, td:TypedDataSpec):
        """Maps a given typed data spec to a C type that represents it."""
        return self.cTypeNames[td]
