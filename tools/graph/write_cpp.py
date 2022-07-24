from graph.core import *

import sys

import inspect

def lineno():
    return inspect.currentframe().f_back.f_lineno

registrationStatements=[]

def render_typed_data_as_decl(proto,dst,indent="",arrayIndex=""):
    if proto is None:
        pass
    elif isinstance(proto,ScalarTypedDataSpec):
        if not isinstance(proto.type,Typedef):
            dst.write("{}{} {};\n".format(indent, proto.type, proto.name))
        else:
            dst.write("{}{} {};\n".format(indent, proto.type.id, proto.name))
    elif isinstance(proto,TupleTypedDataSpec):
        dst.write("{}struct {{\n".format(indent))
        for elt in proto.elements_by_index:
            render_typed_data_as_decl(elt,dst,indent+"  ")
        dst.write("{}}} {};\n".format(indent,proto.name));
    elif isinstance(proto,ArrayTypedDataSpec):
        if isinstance(proto.type,ScalarTypedDataSpec):
            if arrayIndex=="":
                dst.write("{}{} {}[{}];\n".format(indent, proto.type.type, proto.name, proto.length))
            else:
                dst.write("{}{} {}[{}];\n".format(indent, proto.type.type, arrayIndex,proto.length))
        elif isinstance(proto.type, Typedef):
            if arrayIndex=="":
                dst.write("{}{} {}[{}];\n".format(indent, proto.type.id, proto.name, proto.length))
            else:
                dst.write("{}{} {}[{}];\n".format(indent, proto.type.id, arrayIndex, proto.length))
        elif isinstance(proto.type,TupleTypedDataSpec):
            etype=proto.type
            dst.write("{}struct {{\n".format(indent))
            for elt in etype.elements_by_index:
                render_typed_data_as_decl(elt,dst,indent+"  ")
            if arrayIndex=="":
                dst.write("{}}} {}[{}];\n".format(indent,proto.name,proto.length))
            else:
                dst.write("{}}} {}[{}];\n".format(indent,arrayIndex,proto.length))
        elif isinstance(proto.type,ArrayTypedDataSpec):
            if arrayIndex=="":
                render_typed_data_as_decl(proto.type,dst,indent,"{}[{}]".format(proto.name,proto.length))
            else:
                render_typed_data_as_decl(proto.type,dst,indent,"{}[{}]".format(arrayIndex,proto.length))
        else:
            raise RuntimeError("Unrecognised type of array.")
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))

def render_typed_data_init(proto,dst,prefix,indent="    ",arrayIndex="",indexNum=0, isTypedef=False, parentDefault=None):
    if proto is None:
        pass
    elif isinstance(proto,ScalarTypedDataSpec):
        if parentDefault:
            value=parentDefault
        else:
            value=proto.default
        if isinstance(proto.type,Typedef):
            if isTypedef or not arrayIndex=="":
                render_typed_data_init(proto.type.type, dst, prefix, indent, arrayIndex,indexNum+1,True,value)
            else:
                render_typed_data_init(proto.type.type, dst, prefix+proto.name, indent, arrayIndex,indexNum+1,True,value)
        elif proto.default or parentDefault:
            if proto.type=="bool":
                value = 1 if value else 0
            if isTypedef or not arrayIndex=="":
                dst.write('{}{} = {};\n'.format(indent,prefix,value))
            else:
                dst.write('{}{}{} = {};\n'.format(indent,prefix,proto.name,value))
        else:
            if isTypedef or not arrayIndex=="":
                dst.write('{}{} = 0;\n'.format(indent,prefix))
            else:
                dst.write('{}{}{} = 0;\n'.format(indent,prefix,proto.name))
    elif isinstance(proto,TupleTypedDataSpec):
        if proto.default:
            value=proto.default
        else:
            value=parentDefault
        for elt in proto.elements_by_index:
            if value == {}:
                d = None
            elif value is None:
                d = None
            else:
                d = value[elt.name]
            # if value is not None:
            #     d = value[elt.name]
            # elif value is not {}:
            #     d = value[elt.name]
            # else:
            #     d = None
            if isTypedef or not arrayIndex=="":
                render_typed_data_init(elt,dst,prefix+"."+elt.name,indent+"  ", arrayIndex, indexNum+1,True,d)
            else:
                render_typed_data_init(elt,dst,prefix+proto.name+".",indent+"  ",arrayIndex, indexNum+1,False,d)
    elif isinstance(proto,ArrayTypedDataSpec):
        # TODO: Find a way of combining defaults at different levels of multiple dimensional arrays
        value = proto.default
        if parentDefault is not None:
            value = parentDefault
        if isinstance(proto.type, ScalarTypedDataSpec):
            if value is not None:
                for i in range(0, proto.length):
                        if isTypedef or not arrayIndex=="":
                            dst.write('{}{}{}[{}] = {};\n'.format(indent,prefix,arrayIndex,i,value[i]))
                        else:
                            dst.write('{}{}{}[{}] = {};\n'.format(indent,prefix,proto.name,i,value[i]))
                return

            else:
                dst.write(f"{indent}for (int i{indexNum} = 0; i{indexNum} < {proto.length}; i{indexNum}++) {{ //Line {lineno()}\n")
                dst.write(f"{indent}   // prefix={prefix}, indexNum={indexNum}, arrayIndex={arrayIndex}\n")
                if proto.type.default is not None:
                    d = proto.type.default
                else:
                    d = 0

                if isTypedef or not arrayIndex=="":
                    dst.write(f'{indent}{prefix}{arrayIndex}[i{indexNum}] = {d};\n')
                else:
                    dst.write(f'{indent}{prefix}{proto.name}[i{indexNum}] = {d};\n')

        elif isinstance(proto.type,Typedef):
            if value is not None:
                for i in range(0, proto.length):
                    if isTypedef or not arrayIndex=="":
                        render_typed_data_init(proto.type.type, dst, prefix+arrayIndex+"["+str(i)+"]",indent+"  ",arrayIndex+"["+str(i)+"]",indexNum+1,True,value[i])
                    else:
                        render_typed_data_init(proto.type.type, dst, prefix+proto.name+arrayIndex+"["+str(i)+"]",indent+"  ","["+str(i)+"]",indexNum+1,True,value[i])
                return
            else:
                dst.write(f"{indent}for (int i{indexNum} = 0; i{indexNum} < {proto.length}; i{indexNum}++) {{  //Line {linenum()}\n")
                if isTypedef or not arrayIndex=="":
                    render_typed_data_init(proto.type.type, dst, prefix+arrayIndex+"[i"+str(indexNum)+"]",indent+"  ",arrayIndex+"[i"+str(indexNum)+"]",indexNum+1,True)
                else:
                    render_typed_data_init(proto.type.type, dst, prefix+proto.name+arrayIndex+"[i"+str(indexNum)+"]",indent+"  ","[i"+str(indexNum)+"]",indexNum+1,True)
        elif isinstance(proto.type,TupleTypedDataSpec):
            if value is not None:
                tup = proto.type
                for i in range(0, proto.length):
                    for elt in tup.elements_by_index:
                        if isTypedef or not arrayIndex=="":
                            render_typed_data_init(elt, dst, prefix+arrayIndex+"["+str(i)+"]."+elt.name,indent+"  ",arrayIndex+"["+str(i)+"]."+elt.name,indexNum+1,True,value[i][elt.name])
                        else:
                            render_typed_data_init(elt, dst, prefix+proto.name+"["+str(i)+"]."+elt.name,indent+"  ","["+str(i)+"]."+elt.name,indexNum+1,True,value[i][elt.name])
                return
            dst.write(f"{indent}for (int i{indexNum} = 0; i{indexNum} < {proto.length}; i{indexNum}++) {{ //Line {lineno()}\n")
            tup = proto.type
            for elt in tup.elements_by_index:
                dst.write(f"{indent}   // name={elt.name}, arrayIndex={arrayIndex}\n")
                if isTypedef or not arrayIndex=="":
                    dst.write(f"{indent}    // Line {lineno()}\n")
                    render_typed_data_init(elt, dst, prefix+arrayIndex+"[i"+str(indexNum)+"]."+elt.name,indent+"  ",arrayIndex+"[i"+str(indexNum)+"].",indexNum+1)
                else:
                    dst.write(f"{indent}    // Line {lineno()}\n")
                    render_typed_data_init(elt, dst, prefix=prefix+proto.name+"[i"+str(indexNum)+"].",
                        indent=indent+"  ",
                        arrayIndex="", #[i"+str(indexNum)+"]."+elt.name,
                        indexNum=indexNum+1)

        elif isinstance(proto.type,ArrayTypedDataSpec):
            if value is not None:
                for i in range(0, proto.length):
                    if isTypedef or not arrayIndex=="":
                        render_typed_data_init(proto.type, dst, prefix+arrayIndex+"["+str(i)+"]",indent+"  ",arrayIndex+"["+str(i)+"]",indexNum+1,True,value[i])
                    else:
                        render_typed_data_init(proto.type, dst, prefix+proto.name+arrayIndex+"["+str(i)+"]",indent+"  ","["+str(i)+"]",indexNum+1,True,value[i])
                return
            else:
                dst.write(f"{indent}for (int i{indexNum} = 0; i{indexNum} < {proto.length}; i{indexNum}++) {{ //Line {lineno()}\n")
                if isTypedef or not arrayIndex=="":
                    render_typed_data_init(proto.type,dst,prefix,indent+"  ",arrayIndex+"[i"+str(indexNum)+"]",indexNum+1)
                else:
                    render_typed_data_init(proto.type,dst,prefix+proto.name,indent+"  ","[i"+str(indexNum)+"]",indexNum+1)
        else:
            raise RuntimeError("Unrecognised type of array.")
        dst.write("{}}}\n\n".format(indent))
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))

def render_typed_data_add_hash(proto,dst,prefix,indent="    ",indexNum=0,arrayIndex="",isTypedef=False):
    if proto is None:
        pass
    elif isinstance(proto,ScalarTypedDataSpec):
        if not isinstance(proto.type, Typedef):
            if isTypedef or not arrayIndex=="":
                dst.write('{}hash.add({}{});\n'.format(indent,prefix,arrayIndex))
            else:
                dst.write('{}hash.add({}{});\n'.format(indent,prefix,proto.name))
        else:
            render_typed_data_add_hash(proto.type.type,dst,prefix+proto.name,indent,indexNum,arrayIndex,True)
    elif isinstance(proto,TupleTypedDataSpec):
        for elt in proto.elements_by_index:
            if isTypedef or not arrayIndex=="":
                render_typed_data_add_hash(elt,dst,prefix+".",indent,indexNum+1,arrayIndex)
            else:
                render_typed_data_add_hash(elt,dst,prefix+proto.name+".",indent,indexNum+1,arrayIndex)
    elif isinstance(proto,ArrayTypedDataSpec):
        dst.write('{}for (int i{} = 0; i{} < {}; i{}++) {{'.format(indent,indexNum, indexNum, proto.length, indexNum))
        if isinstance(proto.type,ScalarTypedDataSpec):
            if isTypedef or not arrayIndex=="":
                dst.write('{}  hash.add({}{}[i{}]);\n'.format(indent,prefix,arrayIndex,indexNum))
            else:
                dst.write('{}  hash.add({}{}{}[i{}]);\n'.format(indent,prefix,proto.name,arrayIndex,indexNum))
        elif isinstance(proto.type, Typedef):
            if isTypedef or not arrayIndex=="":
                render_typed_data_add_hash(proto.type.type,dst,prefix+arrayIndex+"[i"+str(indexNum)+"]",indent+"  ",indexNum+1,"",True)
            else:
                render_typed_data_add_hash(proto.type.type,dst,prefix+proto.name+arrayIndex+"[i"+str(indexNum)+"]",indent+"  ",indexNum+1,"",True)
        elif isinstance(proto.type, TupleTypedDataSpec):
            for e in proto.type.elements_by_index:
                if isTypedef or not arrayIndex=="":
                    render_typed_data_add_hash(e, dst, prefix, indent+"  ",indexNum+1, arrayIndex+"[i"+str(indexNum)+"]."+e.name)
                else:
                    render_typed_data_add_hash(e, dst, prefix+proto.name,indent+"  ",indexNum+1,"[i"+str(indexNum)+"]."+e.name)
        elif isinstance(proto.type,ArrayTypedDataSpec):
            if isTypedef or not arrayIndex=="":
                render_typed_data_add_hash(proto.type, dst, prefix, indent+"  ",indexNum+1, arrayIndex+"[i"+str(indexNum)+"]")
            else:
                render_typed_data_add_hash(proto.type, dst, prefix+proto.name,indent+"  ",indexNum+1, "[i"+str(indexNum)+"]")
        else:
            raise RuntimeError("Unrecognised type of array.")
        dst.write('{}}}'.format(indent))
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))

def render_typedef_data_load(proto,dst,prefix,indent,parentName,subNum,indexNum=0,arrayIndex=""):
    if isinstance(proto,Typedef):
        render_typedef_data_load(proto.type,dst,prefix,indent,parentName,subNum,indexNum,arrayIndex)
    elif isinstance(proto,TupleTypedDataSpec):
        render_typed_data_load_v4_tuple(proto,dst,prefix+parentName+".",indent+"    ","n"+str(subNum),int(subNum)+1,arrayIndex)
    elif isinstance(proto, ScalarTypedDataSpec):

        if proto.type=="int32_t" or proto.type=="int16_t" or proto.type=="int8_t" or proto.type=="char" :
            dst.write('{}  {}{}=n{}{}.GetInt();\n'.format(indent, prefix, parentName, subNum, arrayIndex))

        elif proto.type=="int64_t" :
            dst.write('{}  {}{}=n{}{}.GetInt64();\n'.format(indent, prefix, parentName, subNum, arrayIndex))

        elif proto.type=="uint32_t" or proto.type=="uint16_t" or proto.type=="uint8_t":
            dst.write('{}  {}{}=n{}{}.GetUint();\n'.format(indent, prefix, parentName, subNum, arrayIndex))

        elif proto.type=="uint64_t":
            dst.write('{}  {}{}=n{}{}.GetUint64();\n'.format(indent, prefix, parentName, subNum, arrayIndex))

        elif proto.type=="float" or proto.type=="double":
            dst.write('{}  {}{}=n{}{}.GetDouble();\n'.format(indent, prefix, parentName, subNum, arrayIndex))

        elif isinstance(proto.type, Typedef):
            render_typedef_data_load(proto.type,dst,prefix,indent,proto.name,subNum,indexNum,arrayIndex)

        else:
            raise RuntimeError("Unknown scalar data type: " + proto.type)

    elif isinstance(proto, ArrayTypedDataSpec):
        elt=proto.type
        dst.write('{}  if (n{}{}.IsArray()) {{\n'.format(indent,subNum,arrayIndex))
        dst.write('{}    auto l{} = n{}{}.Size();'.format(indent,indexNum,subNum,arrayIndex))
        dst.write('{}    assert(l{} <= {});'.format(indent,indexNum,proto.length))
        dst.write('{}    for (uint32_t i{} = 0; i{} < l{}; i{}++) {{'.format(indent,indexNum,indexNum,indexNum,indexNum))
        if isinstance(elt, TupleTypedDataSpec):
            render_typed_data_load_v4_tuple(elt,dst,prefix+parentName+"[i"+str(indexNum)+"].",indent+"    ","n"+str(subNum),subNum+1,arrayIndex+"[i"+str(indexNum)+"]")
        else:
            render_typedef_data_load(elt,dst,prefix,indent+"    ",parentName+"[i"+str(indexNum)+"]",subNum,indexNum+1,arrayIndex+"[i"+str(indexNum)+"]")
        dst.write('{}    }}'.format(indent))

        dst.write('{}  }} else if (n{}{}.IsObject()) {{'.format(indent,subNum,arrayIndex))
        dst.write('{}    auto l{} = n{}{}.MemberCount();'.format(indent,indexNum,subNum,arrayIndex))
        dst.write('{}    assert(l{} <= {});'.format(indent,indexNum,proto.length))
        dst.write('{}    for (uint32_t i{} = 0; i{} < {}; i{}++) {{'.format(indent,indexNum,indexNum,proto.length,indexNum))
        dst.write('{}      const auto s = std::to_string(i{});'.format(indent,indexNum))
        dst.write('{}      if (n{}{}.HasMember(s.c_str())) {{'.format(indent,subNum,arrayIndex))
        dst.write('{}       const rapidjson::Value &n{}=n{}{}[s.c_str()];'.format(indent,subNum+1,subNum,arrayIndex))
        if isinstance(elt, TupleTypedDataSpec):
            render_typed_data_load_v4_tuple(elt,dst,prefix+parentName+"[i"+str(indexNum)+"].",indent+"        ","n"+str(subNum+1),subNum+2)
        else:
            render_typedef_data_load(elt,dst,prefix,indent+"        ",parentName+"[i"+str(indexNum)+"]",subNum+1,indexNum+1)
        dst.write('{}      }}'.format(indent))
        dst.write('{}    }}'.format(indent))
        dst.write('{}  }}'.format(indent))
        # else:
        #     raise RuntimeError("Unrecognised type of array.")

def render_typed_data_load_v4_tuple(proto,dst,prefix,indent,parentName,subNum,arrayIndex=""):
    assert isinstance(proto,TupleTypedDataSpec)
    for elt in proto.elements_by_index:
        dst.write('{}if({}{}.HasMember("{}")){{\n'.format(indent,parentName,arrayIndex,elt.name))
        dst.write('{}  const rapidjson::Value &n{}={}{}["{}"];\n'.format(indent,subNum,parentName,arrayIndex,elt.name))
        if isinstance(elt,ScalarTypedDataSpec):

            if elt.type=="int32_t" or elt.type=="int16_t" or elt.type=="int8_t" or elt.type=="char" :
                dst.write('{}  {}{}=n{}.GetInt();\n'.format(indent, prefix, elt.name, subNum))

            elif elt.type=="int64_t":
                dst.write('{}  {}{}=n{}.GetInt64();\n'.format(indent, prefix, elt.name, subNum))

            elif elt.type=="uint32_t" or elt.type=="uint16_t" or elt.type=="uint8_t":
                dst.write('{}  {}{}=n{}.GetUint();\n'.format(indent, prefix, elt.name, subNum))

            elif elt.type=="uint64_t" :
                dst.write('{}  {}{}=n{}.GetUint64();\n'.format(indent, prefix, elt.name, subNum))

            elif elt.type=="float" or elt.type=="double":
                dst.write('{}  {}{}=n{}.GetDouble();\n'.format(indent, prefix, elt.name, subNum))

            elif isinstance(elt.type, Typedef):
                render_typedef_data_load(elt.type,dst,prefix,indent,elt.name,subNum)
            else:
                raise RuntimeError("Unknown scalar data type : "+elt.type)
        elif isinstance(elt,TupleTypedDataSpec):
            render_typed_data_load_v4_tuple(elt,dst,prefix+elt.name+".",indent+"    ","n"+str(subNum),subNum+1)
        elif isinstance(elt,ArrayTypedDataSpec):
            render_typedef_data_load(elt,dst,prefix,indent,elt.name,subNum)
        else:
            raise RuntimeError("Unknown data type.")
        dst.write('{}  }}\n'.format(indent))


def render_typed_data_load_v4(proto,dst,elt,prefix,indent):
    if proto is None:
        pass
    if not isinstance(proto,TupleTypedDataSpec):
        raise RuntimeError("This must be passed a tuple.")
    dst.write('{}{{'.format(indent))
    dst.write('{}  std::string text="{{"+{}->get_child_text()->get_content()+"}}";\n'.format(indent,elt))
    dst.write('{}  rapidjson::Document document;\n'.format(indent))
    dst.write('{}  document.Parse(text.c_str());\n'.format(indent))
    dst.write('{}  assert(document.IsObject());\n'.format(indent))
    render_typed_data_load_v4_tuple(proto,dst,prefix,indent,"document",0)
    dst.write('{}}}'.format(indent))

def render_typed_data_save_v4(proto,dst,elt,prefix,indent):
    if proto is None:
        pass
    if not isinstance(proto,TupleTypedDataSpec):
        raise RuntimeError("This must be passed a tuple.")
    dst.write('{}{{'.format(indent))
    dst.write('{}  rapidjson::Document document;\n'.format(indent))
    dst.write('{}  auto& alloc = d.GetAllocator();'.format(indent))
    render_typed_data_save_v4_tuple(proto,dst,prefix,indent)
    dst.write('{}}}'.format(indent))

def render_typed_data_save(proto, dst, prefix, indent, indexNum=0, arrayIndex="", isTypedef=False, parentName=""):
    if isinstance(proto,ScalarTypedDataSpec):

        if proto.type=="uint64_t" or proto.type=="uint32_t" or proto.type=="uint16_t" or proto.type=="uint8_t":
            if isTypedef or not arrayIndex=="":
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<(uint64_t){}{}; sep=true; }}\n'.format(indent, prefix, arrayIndex, proto.default, parentName, prefix, arrayIndex))
            else:
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<(uint64_t){}{}; sep=true; }}\n'.format(indent, prefix, proto.name, proto.default, proto.name, prefix, proto.name))
        elif proto.type=="int64_t" or proto.type=="int32_t" or proto.type=="int16_t" or proto.type=="int8_t":
            if isTypedef or not arrayIndex=="":
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<(int64_t){}{}; sep=true; }}\n'.format(indent, prefix, arrayIndex, proto.default, parentName, prefix, arrayIndex))
            else:
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<(int64_t){}{}; sep=true; }}\n'.format(indent, prefix, proto.name, proto.default, proto.name, prefix, proto.name))
        elif proto.type=="float" or proto.type=="double":
            if isTypedef or not arrayIndex=="":
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<float_to_string({}{}); sep=true; }}\n'.format(indent, prefix, arrayIndex, proto.default, parentName, prefix, arrayIndex))
            else:
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<float_to_string({}{}); sep=true; }}\n'.format(indent, prefix, proto.name, proto.default, proto.name, prefix, proto.name))
        elif isinstance(proto.type, Typedef):
            if isTypedef or not arrayIndex=="":
                render_typed_data_save(proto.type.type, dst, prefix, indent, indexNum+1, arrayIndex, True, proto.name)
            else:
                render_typed_data_save(proto.type.type, dst, prefix+proto.name, indent, indexNum+1, arrayIndex, True, proto.name)
        else:
            if isTypedef or not arrayIndex=="":
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<{}{}; sep=true; }}\n'.format(indent, prefix, arrayIndex, proto.default, parentName, prefix, arrayIndex))
            else:
                dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<{}{}; sep=true; }}\n'.format(indent, prefix, proto.name, proto.default, proto.name, prefix, proto.name))
    elif isinstance(proto,ArrayTypedDataSpec):
        if isinstance(proto.type,ScalarTypedDataSpec):
            dst.write('{}if(sep){{ dst<<","; }}; '.format(indent))
            if arrayIndex=="":
                dst.write('{}dst<<"\\"{}\\":[";'.format(indent,proto.name))
            else:
                dst.write('{}dst<<" [";'.format(indent))
            dst.write("{}for(int ii=0; ii<{}; ii++){{ ".format(indent,proto.length))
            dst.write('{}  if(ii>0){{ dst<<","; }}\n'.format(indent))
            if proto.type.type=="float" or proto.type.type=="double":
                if isTypedef or not arrayIndex=="":
                    dst.write('{}  dst<<float_to_string({}{}[ii]);\n'.format(indent, prefix, arrayIndex))
                else:
                    dst.write('{}  dst<<float_to_string({}{}{}[ii]);\n'.format(indent, prefix, proto.name, arrayIndex))
            elif proto.type.type=="uint64_t":
                if isTypedef or not arrayIndex=="":
                    dst.write('{}  dst<<(uint64_t)({}{}[ii]);\n'.format(indent, prefix, arrayIndex))
                else:
                    dst.write('{}  dst<<(uint64_t)({}{}{}[ii]);\n'.format(indent, prefix, proto.name, arrayIndex))
            else:
                if isTypedef or not arrayIndex=="":
                    dst.write('{}  dst<<(int64_t)({}{}[ii]);\n'.format(indent, prefix, arrayIndex))
                else:
                    dst.write('{}  dst<<(int64_t)({}{}{}[ii]);\n'.format(indent, prefix, proto.name, arrayIndex))
            dst.write("{}}}".format(indent)); # End of for loop
            dst.write('{}dst<<"]"; sep=true;\n'.format(indent))
        elif isinstance(proto.type,Typedef):
            e = proto.type
            dst.write('{}if(sep){{ dst<<","; }}; dst<<"\\"{}\\":[";'.format(indent,proto.name))
            for i in range(proto.length):
                if isTypedef or not arrayIndex=="":
                    render_typed_data_save(e.type, dst, prefix+arrayIndex+"["+str(i)+"]", indent+"  ", indexNum+1, "",True, parentName)
                else:
                    render_typed_data_save(e.type, dst, prefix+proto.name+arrayIndex+"["+str(i)+"]", indent+"  ", indexNum+1, "",True, proto.name);
            dst.write('{}dst<<"]"; sep=true;\n'.format(indent))
        elif isinstance(proto.type,TupleTypedDataSpec):
            e = proto.type
            dst.write('{}if(sep){{ dst<<","; }}; dst<<"\\"{}\\":[";'.format(indent,proto.name))
            for i in range(proto.length):
                dst.write('{}if(sep){{ dst<<","; }} sep=true;\n'.format(indent))
                dst.write('{}{{ bool sep=false;\n'.format(indent))
                if not isTypedef or not arrayINdex=="":
                    dst.write('{}dst<<"\\"{}\\":{{";\n'.format(indent,proto.name))
                for elt in e.elements_by_index:
                    if isTypedef or not arrayIndex=="":
                        render_typed_data_save(elt, dst, prefix, indent+"  ", indexNum+1, arrayIndex+"["+str(i)+"]."+elt.name)
                    else:
                        render_typed_data_save(elt, dst, prefix+proto.name, indent+"  ", indexNum+1, arrayIndex+"["+str(i)+"]."+elt.name)
                dst.write('{}dst<<"}}";\n'.format(indent))
                dst.write('{}}}\n'.format(indent))
            dst.write('{}dst<<"]"; sep=true;\n'.format(indent))
        elif isinstance(proto.type,ArrayTypedDataSpec):
            if arrayIndex=="":
                dst.write('{}if(sep){{ dst<<","; }}; dst<<"\\"{}\\":[";'.format(indent,proto.name))
            else:
                dst.write('{}dst<<" [";'.format(indent))
            dst.write("{}for(int i{}=0; i{}<{}; i{}++){{ ".format(indent,indexNum,indexNum,proto.length,indexNum))
            if isTypedef or not arrayIndex=="":
                render_typed_data_save(proto.type, dst, prefix, indent+"  ", indexNum+1, arrayIndex+"[i"+str(indexNum)+"]")
            else:
                render_typed_data_save(proto.type, dst, prefix+proto.name, indent+"  ",indexNum+1,"[i"+str(indexNum)+"]")
            dst.write('{}}}'.format(indent))
            dst.write('{}dst<<"]"; sep=true;\n'.format(indent))

        else:
            raise RuntimeError("Unrecognised type of array.")
    elif isinstance(proto,TupleTypedDataSpec):
        dst.write('{}if(sep){{ dst<<","; }} sep=true;\n'.format(indent))
        dst.write('{}{{ bool sep=false;\n'.format(indent))
        if not isTypedef:
            dst.write('{}dst<<"\\"{}\\":{{";\n'.format(indent,proto.name))
        for elt in proto.elements_by_index:
            if isTypedef or not arrayIndex=="":
                render_typed_data_save(elt, dst, prefix+".", indent+"  ",indexNum+1, arrayIndex)
            else:
                render_typed_data_save(elt, dst, prefix+proto.name+".", indent+"  ",indexNum+1, arrayIndex)
        dst.write('{}dst<<"}}";\n'.format(indent))
        dst.write('{}}}\n'.format(indent))
    else:
        assert False, "Unknown data-type: "+str(proto.type)

def render_typedef_as_elements(proto, dst, prefix, parentName):
    e = proto.type
    if isinstance(e, Typedef):
        render_typedef_as_elements(e, dst, prefix, e.name)
    elif isinstance(e, ScalarTypedDataSpec):
        dst.write('{}makeScalar("{}","{}","{}")\n'.format(prefix,parentName,e.type, proto.default or 0))
    elif isinstance(e, ArrayTypedDataSpec):
        dst.write('{}makeArray("{}",{},\n'.format(prefix,parentName,e.length))
        render_typed_data_as_elements(e.type, dst, prefix+"  ")
        dst.write("{})\n".format(prefix))
    elif isinstance(e, TupleTypedDataSpec):
        tup = proto.type
        dst.write('{}makeTuple("{}",{{\n'.format(prefix,parentName))
        first=True
        for elt in tup.elements_by_index:
            if first:
                first=False
            else:
                dst.write("{},\n".format(prefix))
            render_typed_data_as_elements(elt, dst, prefix+"  ")
        dst.write('{}}})\n'.format(prefix))

def render_typed_data_as_elements(proto, dst, prefix):
    if proto is None:
        dst.write('{}makeTuple("_",{{}})\n'.format(prefix))
    elif isinstance(proto,ScalarTypedDataSpec):
        if not isinstance(proto.type, Typedef):
            dst.write('{}makeScalar("{}","{}","{}")\n'.format(prefix,proto.name,proto.type, proto.default or 0))
        else:
            render_typedef_as_elements(proto.type, dst, prefix, proto.name)
    elif isinstance(proto,TupleTypedDataSpec):
        dst.write('{}makeTuple("{}",{{\n'.format(prefix,proto.name))
        first=True
        for elt in proto.elements_by_index:
            if first:
                first=False
            else:
                dst.write("{},\n".format(prefix))
            render_typed_data_as_elements(elt, dst, prefix+"  ")
        dst.write('{}}})\n'.format(prefix))
    elif isinstance(proto,ArrayTypedDataSpec):
        dst.write('{}makeArray("{}",{},\n'.format(prefix,proto.name,proto.length))
        render_typed_data_as_elements(proto.type, dst, prefix+"  ")
        dst.write("{})\n".format(prefix))
    elif isinstance(proto,Typedef):
        render_typedef_as_elements(proto, dst, prefix, proto.name)
    else:
        assert False, "Unknown data-type, {}".format(proto)


def render_typed_data_as_spec(proto,name,elt_name,dst,asHeader=False):
    dst.write("#pragma pack(push,1)\n")
    dst.write("struct {} : typed_data_t{{\n".format(name))
    if proto:
        assert isinstance(proto, TupleTypedDataSpec)
        for elt in proto.elements_by_index:
            render_typed_data_as_decl(elt,dst,"  ")
    else:
        dst.write("  //empty\n")
    dst.write("};\n")
    dst.write("#pragma pack(pop)\n")
    if asHeader:
        return

    dst.write("class {}_Spec : public TypedDataSpec {{\n".format(name))
    dst.write("private:\n")
    dst.write("  std::shared_ptr<TypedDataSpecElementTuple> m_tupleElt;\n")
    dst.write("  std::vector<char> m_default;\n")
    dst.write("public:\n")
    dst.write("  {}_Spec(){{\n".format(name))
    dst.write("    m_tupleElt=\n".format(name))
    render_typed_data_as_elements(proto, dst,"      ")
    dst.write("    ;\n")
    dst.write("    m_default.resize(payloadSize());\n")
    dst.write("    m_tupleElt->createBinaryDefault(m_default.empty() ? nullptr : &m_default[0], m_default.size());\n");
    dst.write("    ")
    dst.write("  }\n")
    dst.write("  std::shared_ptr<TypedDataSpecElementTuple> getTupleElement() const override { return m_tupleElt; }\n")
    dst.write("  size_t payloadSize() const override {{ return sizeof({})-sizeof(typed_data_t); }}\n".format(name))
    dst.write("  size_t totalSize() const override {{ return sizeof({}); }}\n".format(name))
    dst.write("  TypedDataPtr create() const override {\n")
    if proto:
        dst.write("    {} *res=({}*)malloc(sizeof({}));\n".format(name,name,name))
        dst.write("    res->_ref_count=0;\n")
        dst.write("    res->_total_size_bytes=sizeof({});\n".format(name))
        #for elt in proto.elements_by_index:
        #    render_typed_data_init(elt, dst, "    res->");
        if( proto.size_in_bytes() > 0):
            dst.write("    if(!m_default.empty()){\n")
            dst.write("        memcpy( ((char*)res)+sizeof(typed_data_t), &m_default[0], m_default.size() );")
            dst.write("    }\n")
        dst.write("    return TypedDataPtr(res);\n")
    else:
        dst.write("    return TypedDataPtr();\n")
    dst.write("  }\n")
    dst.write("""
    virtual bool is_default(const TypedDataPtr &v) const override
    {
      if(!v) return true;
      assert(v.payloadSize()==m_default.size());
      return 0==memcmp(v.payloadPtr(), &m_default[0], m_default.size());
    }
    """)
    dst.write("  TypedDataPtr load(xmlpp::Element *elt) const override {\n")
    if proto is None:
        dst.write("    return TypedDataPtr();\n")
    else:
        dst.write("    xmlpp::Node::PrefixNsMap ns;\n")

        dst.write('    ns["g"]="TODO/POETS/virtual-graph-schema-v1";\n')
        dst.write("    {} *res=({}*)malloc(sizeof({}));\n".format(name,name,name))
        dst.write("    res->_ref_count=0;\n")
        dst.write("    res->_total_size_bytes=sizeof({});\n".format(name))
        for elt in proto.elements_by_index:
            render_typed_data_init(elt,dst,"    res->")
        dst.write("    if(elt){\n")
        render_typed_data_load_v4(proto, dst, "elt", "res->", "      ")
        dst.write("    }\n")
        dst.write("    return TypedDataPtr(res);\n")
    dst.write("  }\n")
    dst.write("  void save(xmlpp::Element *parent, const TypedDataPtr &data) const override {\n")
    dst.write('    throw std::runtime_error("Not implemented.");\n')
    dst.write("  }\n")
    dst.write("  std::string toJSON(const TypedDataPtr &data) const override {\n")
    if proto is None:
        dst.write('    return std::string();\n')
    else:
        dst.write("    const {0} *src=(const {0} *)data.get();\n".format(name));
        dst.write("    if(!src){ return std::string(); }\n")
        dst.write("    std::stringstream dst;\n")
        dst.write("    bool sep=false;\n")
        dst.write('    dst<<"{";\n')
        for elt in proto.elements_by_index:
            render_typed_data_save(elt, dst, "src->", "    ")
        dst.write('    dst<<"}";\n')
        dst.write("    return dst.str();\n")
    dst.write('  }\n')
    dst.write('  void addDataHash(const TypedDataPtr &data, POETSHash &hash) const override\n')
    dst.write('  {\n')
    dst.write("    const {0} *src=(const {0} *)data.get();\n".format(name));
    dst.write("    if(!src) return;\n")
    if proto is not None:
        for elt in proto.elements_by_index:
            render_typed_data_add_hash(elt, dst, "src->")
    dst.write('  }\n')
    dst.write("""

    std::string toXmlV4ValueSpec(const TypedDataPtr &data, int formatMinorVersion=0) const override
    {
        std::stringstream acc;
        m_tupleElt->binaryToXmlV4Value((const char *)data.payloadPtr(), data.payloadSize(), acc, formatMinorVersion);
        return acc.str();
    }

    TypedDataPtr loadXmlV4ValueSpec(const std::string &value, int formatMinorVersion=0) const override
    {
        std::stringstream src(value);
        TypedDataPtr res=create();
        m_tupleElt->xmlV4ValueToBinary(src, (char *)res.payloadPtr(), res.payloadSize(), true, formatMinorVersion);
        return res;
    }

    """)

    dst.write("};\n")
    dst.write("TypedDataSpecPtr {}_Spec_get(){{\n".format(name))
    dst.write("  static TypedDataSpecPtr singleton(new {}_Spec);\n".format(name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def emit_device_global_constants(dt:DeviceType,subs,indent):
    res="""
{indent}const unsigned INPUT_COUNT_{deviceTypeId} = {inputCount};
{indent}const unsigned OUTPUT_COUNT_{deviceTypeId} = {outputCount};
{indent}const unsigned RTC_INDEX_{deviceTypeId}=31;
{indent}const unsigned RTC_FLAG_{deviceTypeId}=0x80000000ul;
""".format(**subs)

    currPinIndex=0
    for ip in dt.inputs_by_index:
        res=res+"""
{indent}const unsigned INPUT_INDEX_{deviceTypeId}_{currPinName}={currPinIndex};
{indent}const unsigned INPUT_FLAG_{deviceTypeId}_{currPinName}=1ul<<{currPinIndex};
{indent}const unsigned INPUT_INDEX_{currPinName}={currPinIndex};
{indent}const unsigned INPUT_FLAG_{currPinName}=1ul<<{currPinIndex};
""".format(currPinName=ip.name,currPinIndex=currPinIndex,**subs)
        currPinIndex+=1

    currPinIndex=0
    for ip in dt.outputs_by_index:
        res=res+"""
{indent}const unsigned OUTPUT_INDEX_{deviceTypeId}_{currPinName}={currPinIndex};
{indent}const unsigned OUTPUT_FLAG_{deviceTypeId}_{currPinName}=1ul<<{currPinIndex};
{indent}const unsigned RTS_INDEX_{deviceTypeId}_{currPinName}={currPinIndex};
{indent}const unsigned RTS_FLAG_{deviceTypeId}_{currPinName}=1ul<<{currPinIndex};
""".format(currPinName=ip.name,currPinIndex=currPinIndex,**subs)
        if ip.is_supervisor_implicit_pin:
            assert currPinIndex==len(dt.outputs_by_index)-1, "Implicit output should always be last"
            res=res+"""
{indent}const unsigned RTS_SUPER_IMPLICIT_SEND_FLAG=RTS_FLAG_{deviceTypeId}_{currPinName};
""" .format(currPinName=ip.name,currPinIndex=currPinIndex,**subs)         
        currPinIndex+=1

    return res

def emit_device_local_constants(dt,subs,indent=""):
    res="""
{indent}typedef {graphPropertiesStructName} GRAPH_PROPERTIES_T;
{indent}typedef {devicePropertiesStructName} DEVICE_PROPERTIES_T;
{indent}typedef {deviceStateStructName} DEVICE_STATE_T;
""".format(**subs)

    currPinIndex=0
    for ip in dt.outputs_by_index:
        res=res+"""
{indent}const unsigned OUTPUT_INDEX_{currPinName}={currPinIndex};
{indent}const unsigned OUTPUT_FLAG_{currPinName}=1ul<<{currPinIndex};
{indent}const unsigned RTS_INDEX_{currPinName}={currPinIndex};
{indent}const unsigned RTS_FLAG_{currPinName}=1ul<<{currPinIndex};
""".format(currPinName=ip.name,currPinIndex=currPinIndex,**subs)
        currPinIndex+=1

    return res

def emit_input_pin_local_constants(dt,subs,indent=""):
    return """
{indent}typedef {pinPropertiesStructName} PORT_PROPERTIES_T;
{indent}typedef {pinStateStructName} PORT_STATE_T;
{indent}typedef {messageStructName} MESSAGE_T;
""".format(**subs)

def emit_output_pin_local_constants(dt,subs,indent=""):
    return """
{indent}typedef {messageStructName} MESSAGE_T;
""".format(**subs)


_add_orchestrator_hack_macros= \
"""
// Add "quality of life" macros defined by orchestrator
#define GRAPHPROPERTIES(a)  graphProperties->a
#define DEVICEPROPERTIES(a) deviceProperties->a
#define DEVICESTATE(a)      deviceState->a
#define EDGEPROPERTIES(a)   edgeProperties->a
#define EDGESTATE(a)        edgeState->a
#define MSG(a)              message->a
#define PKT(a)              message->a
#define RTS(a)              *readyToSend |= RTS_FLAG_##a
#define RTSSUP()            *readyToSend |= RTS_SUPER_IMPLICIT_SEND_FLAG
#define RTSBCAST()          __rtsBcast = true
#define RTSREPLY()          __rtsReply = true
"""


def render_input_pin_as_cpp(ip:InputPin,dst):
    dt=ip.parent
    gt=dt.parent
    mt=ip.message_type
    graph=dt.parent
    for index in range(len(dt.inputs_by_index)):
        if ip==dt.inputs_by_index[index]:
            break

    subs={
        "graphPropertiesStructName"     : "{}_properties_t".format(dt.parent.id),
        "deviceTypeId"                  : dt.id,
        "devicePropertiesStructName"    : "{}_properties_t".format(dt.id),
        "deviceStateStructName"         : "{}_state_t".format(dt.id),
        "messageTypeId"                 : mt.id,
        "messageStructName"             : "{}_message_t".format(mt.id),
        "pinName"                      : ip.name,
        "pinIndex"                     : index,
        "pinPropertiesStructName"       : "{}_{}_properties_t".format(dt.id,ip.name),
        "pinStateStructName"            : "{}_{}_state_t".format(dt.id,ip.name),
        "handlerCode"                   : ip.receive_handler or "",
        "inputCount"                    : len(dt.inputs),
        "outputCount"                   : len(dt.outputs),
        "indent"                        : "  ",
        "isSupervisor"                  : "true" if ip.is_supervisor_implicit_pin else "false"
    }

    if dt.is_external:
        subs["handlerCode"]='throw std::runtime_error("This device type is an external, so onReceive should not be called.");'


    subs["pinLocalConstants"]=emit_input_pin_local_constants(dt,subs, "    ")


    if ip.source_line and ip.source_file:
        subs["preProcLinePragma"]= '#line {} "{}"\n'.format(ip.source_line-1,ip.source_file)
    else:
        subs["preProcLinePragma"]="// No line/file information for handler"

    dst.write(
"""
//MessageTypePtr {messageTypeId}_Spec_get();

static const char *{deviceTypeId}_{pinName}_handler_code=R"CDATA({handlerCode})CDATA";

class {deviceTypeId}_{pinName}_Spec
    : public InputPinImpl {{
public:
  {deviceTypeId}_{pinName}_Spec()
    : InputPinImpl(
        {deviceTypeId}_Spec_get,  //
        "{pinName}",
        {pinIndex},
        {messageTypeId}_Spec_get(),
        {pinPropertiesStructName}_Spec_get(),
        {pinStateStructName}_Spec_get(),
        {deviceTypeId}_{pinName}_handler_code,
        {isSupervisor}
    ) {{}} \n

    virtual void onReceive(
        OrchestratorServices *orchestrator,
        const typed_data_t *gGraphProperties,
        const typed_data_t *gDeviceProperties,
        typed_data_t *gDeviceState,
        const typed_data_t *gEdgeProperties,
        typed_data_t *gEdgeState,
        const typed_data_t *gMessage
    ) const override {{

    {pinLocalConstants}

    auto graphProperties=cast_typed_properties<{graphPropertiesStructName}>(gGraphProperties);
    auto deviceProperties=cast_typed_properties<{devicePropertiesStructName}>(gDeviceProperties);
    auto deviceState=cast_typed_data<{deviceStateStructName}>(gDeviceState);
    auto edgeProperties=cast_typed_properties<{pinPropertiesStructName}>(gEdgeProperties);
    auto edgeState=cast_typed_data<{pinPropertiesStructName}>(gEdgeState);
    auto message=cast_typed_properties<{messageStructName}>(gMessage);
    HandlerLogImpl handler_log(orchestrator);
    auto handler_exit=[&](int code) -> void {{ orchestrator->application_exit(code); }};

    // Begin custom handler
    {preProcLinePragma}
    {handlerCode}
    __POETS_REVERT_PREPROC_DETOUR__
    // End custom handler

  }}
}};

InputPinPtr {deviceTypeId}_{pinName}_Spec_get(){{
  static InputPinPtr singleton(new {deviceTypeId}_{pinName}_Spec);
  return singleton;
}}
""".format(**subs))

    #    end of render_input_pin_as_cpp
    return None

def render_output_pin_as_cpp(op:OutputPin,dst):
    dt=op.parent
    graph=dt.parent
    for index in range(len(dt.outputs_by_index)):
        if op==dt.outputs_by_index[index]:
            break

    mt=op.message_type


    subs={
        "graphPropertiesStructName"     : "{}_properties_t".format(dt.parent.id),
        "deviceTypeId"                  : dt.id,
        "devicePropertiesStructName"    : "{}_properties_t".format(dt.id),
        "deviceStateStructName"         : "{}_state_t".format(dt.id),
        "messageTypeId"                 : mt.id,
        "messageStructName"             : "{}_message_t".format(mt.id),
        "pinName"                      : op.name,
        "pinIndex"                     : index,
        "pinPropertiesStructName"       : "{}_{}_properties_t".format(dt.id,op.name),
        "pinStateStructName"            : "{}_{}_state_t".format(dt.id,op.name),
        "handlerCode"                   : op.send_handler,
        "inputCount"                    : len(dt.inputs),
        "outputCount"                   : len(dt.outputs),
        "indent"                        : "  ",
        "isSupervisor"                  : "true" if op.is_supervisor_implicit_pin else "false"
    }

    subs["pinLocalConstants"]=emit_output_pin_local_constants(dt,subs, "    ")

    if dt.is_external:
        subs["handlerCode"]='throw std::runtime_error("This device is an external, so onSend can''t be called.");'

    if op.source_line and op.source_file:
        subs["preProcLinePragma"]= '#line {} "{}"\n'.format(op.source_line-1,op.source_file)
    else:
        subs["preProcLinePragma"]="// No line/file information for handler"


    #dst.write('MessageTypePtr {}_Spec_get();\n\n'.format(op.message_type.id))

    dst.write('static const char *{}_{}_handler_code=R"CDATA({})CDATA";\n'.format(dt.id, op.name, op.send_handler))

    dst.write("class {}_{}_Spec : public OutputPinImpl {{\n".format(dt.id,op.name))
    dst.write('  public: {}_{}_Spec() : OutputPinImpl({}_Spec_get, "{}", {}, {}_Spec_get(), {}_{}_handler_code, {}, {}) {{}} \n'.format(
            dt.id,op.name, dt.id, op.name, index, op.message_type.id,  dt.id, op.name,
            1 if op.is_indexed else 0, 1 if op.is_supervisor_implicit_pin else 0
    ))
    dst.write("""    virtual void onSend(
                      OrchestratorServices *orchestrator,
                      const typed_data_t *gGraphProperties,
		      const typed_data_t *gDeviceProperties,
		      typed_data_t *gDeviceState,
		      typed_data_t *gMessage,
		      bool *doSend,
              unsigned *sendIndex
		      ) const override {""")
    dst.write('    {pinLocalConstants}\n'.format(**subs));

    if op.is_indexed:
        dst.write("    assert(sendIndex!=0); // This is an indexed output pin\n");
    else:
        dst.write("    assert(sendIndex==0); // This is not an indexed output pin\n");

    dst.write('    auto graphProperties=cast_typed_properties<{}_properties_t>(gGraphProperties);\n'.format( graph.id ))
    dst.write('    auto deviceProperties=cast_typed_properties<{}_properties_t>(gDeviceProperties);\n'.format( dt.id ))
    dst.write('    auto deviceState=cast_typed_data<{}_state_t>(gDeviceState);\n'.format( dt.id ))
    dst.write('    auto message=cast_typed_data<{}_message_t>(gMessage);\n'.format(op.message_type.id))
    dst.write('    HandlerLogImpl handler_log(orchestrator);\n')
    dst.write('    auto handler_exit=[&](int code) -> void { orchestrator->application_exit(code); };\n')

    dst.write('    // Begin custom handler\n')
    if op.source_line and op.source_file:
        dst.write('#line {} "{}"\n'.format(op.source_line,op.source_file))
    if op.send_handler:
        for line in op.send_handler.splitlines():
            dst.write('    {}\n'.format(line))
    dst.write("__POETS_REVERT_PREPROC_DETOUR__")
    dst.write('    // End custom handler\n')

    dst.write('  }\n')
    dst.write("};\n")
    dst.write("OutputPinPtr {}_{}_Spec_get(){{\n".format(dt.id,op.name))
    dst.write("  static OutputPinPtr singleton(new {}_{}_Spec);\n".format(dt.id,op.name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def render_message_type_as_cpp_fwd(et,dst,asHeader):
    render_typed_data_as_spec(et.message, "{}_message_t".format(et.id), "pp:Message", dst, asHeader)
    dst.write(f"using {et.parent.id}_{et.id}_message_t = {et.id}_message_t;\n")

def render_message_type_as_cpp(et,dst):
    dst.write("class {}_Spec : public MessageTypeImpl {{\n".format(et.id))
    dst.write("public:\n")
    dst.write('  {}_Spec() : MessageTypeImpl("{}", {}_message_t_Spec_get()) {{}}\n'.format(et.id, et.id, et.id))
    dst.write('};\n')
    dst.write("MessageTypePtr {}_Spec_get(){{\n".format(et.id))
    dst.write("  static MessageTypePtr singleton(new {}_Spec);\n".format(et.id))
    dst.write("  return singleton;\n")
    dst.write("}\n")
    registrationStatements.append('registry->registerMessageType({}_Spec_get());'.format(et.id,et.id))

def render_device_type_as_cpp_fwd(dt,dst, asHeader):
    render_typed_data_as_spec(dt.properties, "{}_properties_t".format(dt.id), "pp:Properties", dst, asHeader)
    dst.write(f"using {dt.parent.id}_{dt.id}_properties_t = {dt.id}_properties_t;")
    render_typed_data_as_spec(dt.state, "{}_state_t".format(dt.id), "pp:State", dst, asHeader)
    dst.write(f"using {dt.parent.id}_{dt.id}_state_t = {dt.id}_state_t;")
    for ip in dt.inputs.values():
        render_typed_data_as_spec(ip.properties, "{}_{}_properties_t".format(dt.id,ip.name), "pp:Properties", dst)
        render_typed_data_as_spec(ip.state, "{}_{}_state_t".format(dt.id,ip.name), "pp:State", dst)

def render_device_type_as_cpp(dt:DeviceType,dst):
    subs={
        "indent"                        : "    ",
        "graphPropertiesStructName"     : "{}_properties_t".format(dt.parent.id),
        "deviceTypeId"                  : dt.id,
        "devicePropertiesStructName"    : "{}_properties_t".format(dt.id),
        "deviceStateStructName"         : "{}_state_t".format(dt.id),
        "handlerCode"                   : dt.ready_to_send_handler or "",
        "initCode"                      : dt.init_handler,
        "inputCount"                    : len(dt.inputs),
        "outputCount"                   : len(dt.outputs),
        "onHardwareIdleCode"            : dt.on_hardware_idle_handler,
        "isExternal"                    : int(dt.is_external)
    }
    if dt.init_source_line and dt.init_source_file:
        subs["preInitProcLinePragma"]= '#line {} "{}"\n'.format(dt.init_source_line-1,dt.init_source_file)
    else:
        subs["preInitProcLinePragma"]="// No line/file information for init handler"

    if dt.ready_to_send_source_line and dt.ready_to_send_source_file:
        subs["preProcLinePragma"]= '#line {} "{}"\n'.format(dt.ready_to_send_source_line-1,dt.ready_to_send_source_file)
    else:
        subs["preProcLinePragma"]="// No line/file information for readyToSend handler"

    subs["deviceGlobalConstants"]=emit_device_global_constants(dt,subs,"    ")
    subs["deviceLocalConstants"]=emit_device_local_constants(dt,subs, "    ")

    dst.write("namespace ns_{}{{\n".format(dt.id));

    dst.write(subs["deviceGlobalConstants"])
    dst.write(subs["deviceLocalConstants"])

    if dt.shared_code is not None:
        for i in dt.shared_code:
            dst.write(i)

    dst.write("DeviceTypePtr {}_Spec_get();\n".format(dt.id))

    for ip in dt.inputs.values():
        render_input_pin_as_cpp(ip,dst)

    for op in dt.outputs.values():
        render_output_pin_as_cpp(op,dst)

    dst.write("class {}_Spec : public DeviceTypeImpl {{\n".format(dt.id))
    dst.write("public:\n")
    dst.write("  {}_Spec()\n".format(dt.id))
    dst.write('  : DeviceTypeImpl("{}", {}_properties_t_Spec_get(), {}_state_t_Spec_get(),\n'.format(dt.id, dt.id, dt.id))
    dst.write('      std::vector<InputPinPtr>({')
    first=True
    for i in dt.inputs_by_index:
        if first:
            first=False
        else:
            dst.write(',')
        dst.write('{}_{}_Spec_get()'.format(dt.id,i.name))
    dst.write('}),\n')
    dst.write('      std::vector<OutputPinPtr>({')
    first=True
    for o in dt.outputs_by_index:
        if first:
            first=False
        else:
            dst.write(',')
        dst.write('{}_{}_Spec_get()'.format(dt.id,o.name))
    dst.write(f'}}), {int(dt.is_external)})\n')
    dst.write("  {}\n")
    
    dst.write(f"""
      
      int getSupervisorImplicitInput() const override {{ return {dt.supervisor_implicit_input_index};  }}
      int getSupervisorImplicitOutput() const override {{ return {dt.supervisor_implicit_output_index}; }}
      
""")

    dst.write(
"""
  virtual uint32_t calcReadyToSend(
    OrchestratorServices *orchestrator,
    const typed_data_t *gGraphProperties,
    const typed_data_t *gDeviceProperties,
    const typed_data_t *gDeviceState
  ) const override {{
      if({isExternal}){{
          throw std::runtime_error("Attempt to call calcReadyToSend on external type {deviceTypeId}");
      }}

    auto graphProperties=cast_typed_properties<{graphPropertiesStructName}>(gGraphProperties);
    auto deviceProperties=cast_typed_properties<{devicePropertiesStructName}>(gDeviceProperties);
    auto deviceState=cast_typed_properties<{deviceStateStructName}>(gDeviceState);
    HandlerLogImpl handler_log(orchestrator);
    // Note: no handler_exit in rts, as it should be side effect free

    uint32_t fReadyToSend=0;
    uint32_t *readyToSend=&fReadyToSend;

    // Begin custom handler
    {preProcLinePragma}
    {handlerCode}
    __POETS_REVERT_PREPROC_DETOUR__
    // End custom handler

    return fReadyToSend;
  }}
""".format(**subs))

    if (dt.init_handler):
        dst.write(
    """
      virtual void init(
        OrchestratorServices *orchestrator,
        const typed_data_t *gGraphProperties,
        const typed_data_t *gDeviceProperties,
        typed_data_t *gDeviceState
      ) const override {{
        if({isExternal}){{
          throw std::runtime_error("Attempt to call init on external type {deviceTypeId}");
        }}


        auto graphProperties=cast_typed_properties<{graphPropertiesStructName}>(gGraphProperties);
        auto deviceProperties=cast_typed_properties<{devicePropertiesStructName}>(gDeviceProperties);
        auto deviceState=cast_typed_data<{deviceStateStructName}>(gDeviceState);
        HandlerLogImpl handler_log(orchestrator);
        auto handler_exit=[&](int code) -> void {{ orchestrator->application_exit(code); }};
        auto handler_checkpoint=[&](bool preEvent, int level, const char *fmt, ...) -> void {{
            va_list a;
            va_start(a,fmt);
            orchestrator->vcheckpoint(preEvent,level,fmt,a);
            va_end(a);
        }};

        // Begin custom handler
        {preInitProcLinePragma}
        {initCode}
        __POETS_REVERT_PREPROC_DETOUR__
        // End custom handler
      }}
    """.format(**subs))
    else:
        dst.write(
    """
      virtual void init(
        OrchestratorServices *orchestrator,
        const typed_data_t *gGraphProperties,
        const typed_data_t *gDeviceProperties,
        typed_data_t *gDeviceState
      ) const override {{
        if({isExternal}){{
          throw std::runtime_error("Attempt to call init on external type {deviceTypeId}");
        }}

        auto graphProperties=cast_typed_properties<{graphPropertiesStructName}>(gGraphProperties);
        auto deviceProperties=cast_typed_properties<{devicePropertiesStructName}>(gDeviceProperties);
        auto deviceState=cast_typed_data<{deviceStateStructName}>(gDeviceState);
        HandlerLogImpl handler_log(orchestrator);
        auto handler_exit=[&](int code) -> void {{ orchestrator->application_exit(code); }};
        auto handler_checkpoint=[&](bool preEvent, int level, const char *fmt, ...) -> void {{
            va_list a;
            va_start(a,fmt);
            orchestrator->vcheckpoint(preEvent,level,fmt,a);
            va_end(a);
        }};
        // NO INIT HANDLER PROVIDED
        return;
    }}
    """.format(**subs))

    if (dt.on_hardware_idle_handler):
        dst.write(
    """
      virtual void onHardwareIdle(
        OrchestratorServices *orchestrator,
        const typed_data_t *gGraphProperties,
        const typed_data_t *gDeviceProperties,
        typed_data_t *gDeviceState
      ) const override {{
        if({isExternal}){{
          throw std::runtime_error("Attempt to call onHardwareIdle on external type {deviceTypeId}");
        }}


        auto graphProperties=cast_typed_properties<{graphPropertiesStructName}>(gGraphProperties);
        auto deviceProperties=cast_typed_properties<{devicePropertiesStructName}>(gDeviceProperties);
        auto deviceState=cast_typed_data<{deviceStateStructName}>(gDeviceState);
        HandlerLogImpl handler_log(orchestrator);
        auto handler_exit=[&](int code) -> void {{ orchestrator->application_exit(code); }};
        auto handler_checkpoint=[&](bool preEvent, int level, const char *fmt, ...) -> void {{
            va_list a;
            va_start(a,fmt);
            orchestrator->vcheckpoint(preEvent,level,fmt,a);
            va_end(a);
        }};

        // Begin custom handler
        {preInitProcLinePragma}
        {onHardwareIdleCode}
        __POETS_REVERT_PREPROC_DETOUR__
        // End custom handler
      }}
    """.format(**subs))
    else:
        dst.write("""
      virtual void onHardwareIdle(
        OrchestratorServices *orchestrator,
        const typed_data_t *gGraphProperties,
        const typed_data_t *gDeviceProperties,
        typed_data_t *gDeviceState
      ) const override {{
        if({isExternal}){{
          throw std::runtime_error("Attempt to call onHardwareIdle on external type {deviceTypeId}");
        }}

          // No hardware idle handler
      }}
    """.format(**subs))

    dst.write("};\n")
    dst.write("DeviceTypePtr {}_Spec_get(){{\n".format(dt.id))
    dst.write("  static DeviceTypePtr singleton(new {}_Spec);\n".format(dt.id))
    dst.write("  return singleton;\n")
    dst.write("}\n")

    dst.write("}}; //namespace ns_{}\n\n".format(dt.id));

    registrationStatements.append('registry->registerDeviceType(ns_{}::{}_Spec_get());'.format(dt.id,dt.id,dt.id))


def raw_cpp_str(x):
    if x is None:
        return '""'
    else:
        return f'R"XYZ({x})XYZ"'

def render_supervisor_type_as_cpp(st:SupervisorType,dst):
    R=raw_cpp_str
    S=lambda x: x if x is not None else ""

    assert st.id!=""

    inputs_str="{\n"
    for (i,input) in enumerate(st.inputs_by_index):
        dst.write(f"// Supervisor type {st.id} handler code for input pin {input[0]} {input[1]}\n")
        dst.write(f"static const char *{st.id}_in_handler_{input[0]} = \n")
        dst.write(R(input[3]))
        dst.write(";\n")
        
        if(i!=0):
            inputs_str+=",\n"
        inputs_str+=f"""{{ {i}, "{input[1]}", {input[2].id}_Spec_get(), {st.id}_in_handler_{input[0]} }}"""

    inputs_str+="}\n"
    
    assert len(st.inputs_by_index)==1, "Don't know how to render a supervisor type that doesn't have exactly one SupervisorInPin"

    messageTypeId=st.inputs_by_index[0][2].id

    dst.write("namespace ns_{}{{\n".format(st.id));

    dst.write(f"""
static const char *{st.id}_properties =
{R(st.properties_code)}
;
static const char *{st.id}_state =
{R(st.state_code)}
;
static const char *{st.id}_shared_code =
{R(st.shared_code)}
;
static const char *{st.id}_init_handler =
{R(st.init_handler)}
;
static const char *{st.id}_on_stop_handler =
{R(st.on_stop_handler)}
;
static const char *{st.id}_on_supervisor_idle_handler =
{R(st.on_supervisor_idle_handler)}
;

""")

    # Write out the supervisor instance class. This contains the handlers
    dst.write(
f"""

struct Super
{{
    OrchestratorServices *curr_services;
    
    void post(const std::string &msg)
    {{
        curr_services->log(0, msg.c_str());
    }}
    
    void stop_application()
    {{
        curr_services->application_exit(0);
    }}
    
    std::string get_output_directory(std::string suffix="")
    {{
        char *tmp=getenv("TMP");
        if(tmp==0){{
            post("Couldn't create supervisor directory.");
            return {{}};
        }}
        std::string res=std::string(tmp)+"/supervisor_dir_XXXXXX";
        
        if(0==mkdtemp(&res[0])){{
            post("Couldn't create supervisor directory.");
            return "";
        }}
        return res;
    }}
}};

class {st.id}_Instance
    : public SupervisorInstance
    , public Super
{{
private:
    struct properties_t
    {{
{st.properties_code}
    }} m_properties;

    struct state_t
    {{
{st.state_code}
    }} m_state;
    
    const properties_t * const supervisorProperties;
    state_t * const supervisorState;
    
    void beginHandler(OrchestratorServices *services)
    {{
        Super::curr_services=services;  
    }}
    
    void endHandler()
    {{
        Super::curr_services=0;
    }}
    
public:

    {st.id}_Instance()
        : supervisorProperties(&m_properties)
        , supervisorState(&m_state)
    {{
    }}

    ~{st.id}_Instance()
    {{}}

  // Only valid if the supervisor type is cloneable
  virtual std::shared_ptr<SupervisorInstance> clone()
  {{
        // TODO: Need more thought here
        /*if const(std::is_copy_constructible<{st.id}_Instance>()){{
            return std::make_shared<{st.id}_Instance>(*this);
        }}else{{*/
            throw std::runtime_error("The {st.id} supervisor does not appear to be cloneable.");
        //}}
  }}

  virtual void onInit(
          OrchestratorServices *orchestrator,
          const typed_data_t *gGraphProperties
          ) 
    {{
        auto graphProperties=cast_typed_properties<{st.parent.id}_properties_t>(gGraphProperties);
        
        beginHandler(orchestrator);
{S(st.init_handler)}
        endHandler();
    }}

  virtual void onSupervisorIdle(
          OrchestratorServices *orchestrator,
          const typed_data_t *gGraphProperties
          )
    {{
        auto graphProperties=cast_typed_properties<{st.parent.id}_properties_t>(gGraphProperties);    
    
        beginHandler(orchestrator);
{S(st.on_supervisor_idle_handler)}
        endHandler();
    }}

  virtual void onStop(
          OrchestratorServices *orchestrator,
          const typed_data_t *gGraphProperties
          )
{{
    auto graphProperties=cast_typed_properties<{st.parent.id}_properties_t>(gGraphProperties);
    
    beginHandler(orchestrator);
{S(st.on_supervisor_idle_handler)}
    endHandler();
}}

  virtual void onRecv(
          OrchestratorServices *orchestrator,
          const typed_data_t *gGraphProperties,
          unsigned pin_index, // Currently always 0
          const typed_data_t *gMessage,
          typed_data_t *gReply,
          typed_data_t *gBroadcast,
          bool &rtsReply,   // Set to true to cause reply to be sent to originator
          bool &rtsBcast    // Set to true to cause broadcast to be sent to all devices
          )
{{
    auto graphProperties=cast_typed_properties<{st.parent.id}_properties_t>(gGraphProperties);
    auto message=cast_typed_properties<{messageTypeId}_message_t>(gMessage);
    auto reply=cast_typed_data<{messageTypeId}_message_t>(gReply);
    auto bcast=cast_typed_data<{messageTypeId}_message_t>(gBroadcast);
    
    beginHandler(orchestrator);
    auto &__rtsReply = rtsReply;
    auto &__rtsBcast = rtsBcast;
    
{S(st.inputs_by_index[0][3])}
    
    endHandler();  
}}
}};

"""
)
    

    # Write out the supervisor type class, which is just meta information plus a factory for instances
    dst.write(
f"""
class {st.id}_Spec : public SupervisorTypeImpl {{
public:
    {st.id}_Spec()
        : SupervisorTypeImpl("{st.id}"
            , {st.id}_properties
            , {st.id}_state
            , {inputs_str}
            , {st.id}_shared_code
            , {st.id}_init_handler
            , {st.id}_on_supervisor_idle_handler
            , {st.id}_on_stop_handler
        )
    {{}}

    bool isCloneable() const override
    {{ return false; }}

    virtual std::shared_ptr<SupervisorInstance> create() const
    {{
        return std::make_shared<{st.id}_Instance>();
    }}
}};
""")

    dst.write(f"""
SupervisorTypePtr {st.id}_Spec_get(){{
    static SupervisorTypePtr singleton(new {st.id}_Spec);
    return singleton;
}};
""")

    dst.write("}}; //namespace ns_{}\n\n".format(st.id));

    # Not registering. It's unclear if this is needed.
    # TODO : should devices and message types still be registered?
    # registrationStatements.append(f'registry->registerSupervisorType(ns_{st.id}::{st.id}_Spec_get());')


def render_typedef_decl(td,dst,arrayIndex="",name=""):
    if isinstance(td, TupleTypedDataSpec):
        if name=="":
            dst.write("typedef struct {} {{".format(td.name))
        else:
            dst.write("typedef struct {} {{".format(name))
        for elt in td.elements_by_index:
            render_typed_data_as_decl(elt,dst,"    ")
        if name=="":
            dst.write("}} {}{};\n\n".format(td.name,arrayIndex))
        else:
            dst.write("}} {}{};\n\n".format(name,arrayIndex))
    elif isinstance(td, ScalarTypedDataSpec):
        if isinstance(td.type, Typedef):
            if name=="":
                dst.write("typedef {} {}{};\n\n".format(td.type.type,td.name,arrayIndex))
            else:
                dst.write("typedef {} {}{};\n\n".format(td.type.type,name,arrayIndex))
        else:
            if name=="":
                dst.write("typedef {} {}{};\n\n".format(td.type, td.name,arrayIndex))
            else:
                dst.write("typedef {} {}{};\n\n".format(td.type, name,arrayIndex))
    elif isinstance(td, ArrayTypedDataSpec):
        if name=="":
            render_typedef_decl(td.type,dst,arrayIndex+"["+str(td.length)+"]",td.name)
        else:
            render_typedef_decl(td.type,dst,arrayIndex+"["+str(td.length)+"]",name)
    elif isinstance(td, Typedef):
        if name=="":
            render_typedef_decl(td.type,dst,arrayIndex,td.name)
        else:
            render_typedef_decl(td.type,dst,arrayIndex,name=name)
    else:
        raise RuntimeError("Unrecognised type \"{}\" in TypeDef declarations".format(td))

def render_graph_as_cpp(graph,dst, destPath, asHeader=False):
    gt=graph

    if asHeader:
        dst.write('#ifndef {}_header\n'.format(gt.id))
        dst.write('#define {}_header\n'.format(gt.id))

    dst.write('#include "graph.hpp"\n')
    dst.write('#include "rapidjson/document.h"\n')

    if gt.typedefs_by_index:
        dst.write("\n// USER DEFINED TYPES")
        for i in gt.typedefs_by_index:
            render_typedef_decl(i,dst)
        #     if isinstance(i.type, TupleTypedDataSpec):
        #         dst.write("typedef struct {} {{".format(i.name))
        #         for elt in i.type.elements_by_index:
        #             render_typed_data_as_decl(elt,dst,"    ")
        #         dst.write("}} {};\n\n".format(i.name))
        #     elif isinstance(i.type, ScalarTypedDataSpec):
        #         if isinstance(i.type.type, Typedef):
        #             dst.write("typedef {} {};\n\n".format(i.type,i.name))
        #         else:
        #             dst.write("typedef {} {};\n\n".format(i.type.type, i.name))
        #     elif isinstance(i.type, ArrayTypedDataSpec):
        #         if isinstance(i.type.type, Typedef):
        #             dst.write("typedef {} {}[{}];\n\n".format(i.type.type, i.name, i.type.length))
        #         elif isinstance(i.type,ArrayTypedDataSpec):
        #         else:
        #             dst.write("typedef {} {}[{}];\n\n".format(i.type.type.type, i.name, i.type.length))

    render_typed_data_as_spec(gt.properties, "{}_properties_t".format(gt.id),"pp:Properties",dst,asHeader)

    dst.write("#undef assert")
    dst.write("#define assert handler_assert\n")

    dst.write(_add_orchestrator_hack_macros)
    dst.write("\n")

    dst.write("/////////////////////////////////\n")
    dst.write("// FWD\n")
    for et in gt.message_types.values():
        render_message_type_as_cpp_fwd(et,dst, asHeader)

    for dt in gt.device_types.values():

        render_device_type_as_cpp_fwd(dt, dst, asHeader)


    if not asHeader:
        if gt.shared_code:
            for code in gt.shared_code:
                dst.write(code)

    dst.write("/////////////////////////////////\n")

    dst.write("""
    #ifdef POETS_HAVE_IN_PROC_EXTERNAL_MAIN
    extern "C" void poets_in_proc_external_main(InProcessBinaryUpstreamConnection &conn, int argc, const char **argv);
    #endif
    """)

    dst.write("// DEF\n")
    for et in gt.message_types.values():
        render_message_type_as_cpp(et,dst)

    for dt in gt.device_types.values():
        render_device_type_as_cpp(dt,dst)

    for st in gt.supervisor_types.values():
        render_supervisor_type_as_cpp(st,dst)

    dst.write("class {}_Spec : public GraphTypeImpl {{\n".format(gt.id))
    dst.write('  public: {}_Spec() : GraphTypeImpl("{}", {}_properties_t_Spec_get()) {{\n'.format(gt.id,gt.id,gt.id))
    for et in gt.message_types.values():
        dst.write('    addMessageType({}_Spec_get());\n'.format(et.id))
    for dt in gt.device_types.values():
        dst.write('    addDeviceType(ns_{}::{}_Spec_get());\n'.format(dt.id,dt.id))
    for st in gt.supervisor_types.values():
        dst.write('    addSupervisorType(ns_{}::{}_Spec_get());\n'.format(st.id,st.id))
    dst.write("""
    // This define will be specified when compiling the provider
    #ifdef POETS_HAVE_IN_PROC_EXTERNAL_MAIN
    addInProcExternalMain(poets_in_proc_external_main);
    #endif
    """)
    dst.write("  };\n")
    dst.write("};\n");
    dst.write("GraphTypePtr {}_Spec_get(){{\n".format(gt.id))
    dst.write("  static GraphTypePtr singleton(new {}_Spec);\n".format(gt.id))
    dst.write("  return singleton;\n")
    dst.write("}\n")

    registrationStatements.append('registry->registerGraphType({}_Spec_get());'.format(gt.id))

    dst.write('extern "C" void registerGraphTypes(Registry *registry){\n')
    for r in registrationStatements:
        dst.write("  {}\n".format(r))
    dst.write("}\n");

    if asHeader:
        dst.write("#endif\n")
