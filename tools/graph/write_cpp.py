from graph.core import *

import sys

registrationStatements=[]

def render_typed_data_as_decl(proto,dst,indent=""):
    if proto is None:
        pass
    elif isinstance(proto,ScalarTypedDataSpec):
        dst.write("{}{} {};\n".format(indent, proto.type, proto.name))
    elif isinstance(proto,TupleTypedDataSpec):
        dst.write("{}struct {{\n".format(indent))
        for elt in proto.elements_by_index:
            render_typed_data_as_decl(elt,dst,indent+"  ")
        dst.write("{}}} {};\n".format(indent,proto.name));
    elif isinstance(proto,ArrayTypedDataSpec):
        assert isinstance(proto.type,ScalarTypedDataSpec), "Haven't implemented arrays of non-scalars yet."
        dst.write("{}{} {}[{}];\n".format(indent, proto.type.type, proto.name, proto.length));
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))


def render_typed_data_init(proto,dst,prefix):
    if proto is None:
        pass
    elif isinstance(proto,ScalarTypedDataSpec):

        if proto.default:
            value=proto.default
            if proto.type=="bool":
                value = 1 if value else 0
            dst.write('{}{} = {};\n'.format(prefix,proto.name,value))
        else:
            dst.write('{}{} = 0;\n'.format(prefix,proto.name))
    elif isinstance(proto,TupleTypedDataSpec):
        for elt in proto.elements_by_index:
            render_typed_data_init(elt,dst,prefix+proto.name+".")
    elif isinstance(proto,ArrayTypedDataSpec):
        assert isinstance(proto.type,ScalarTypedDataSpec), "Haven't implemented arrays of non-scalars yet."
        for i in range(0,proto.length):

            if proto.type.default is not None:
                dst.write('{}{}[{}] = {};\n'.format(prefix,proto.name,i,proto.type.default))
            else:
                dst.write('{}{}[{}] = 0;\n'.format(prefix,proto.name,i))

    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))

def render_typed_data_add_hash(proto,dst,prefix):
    if proto is None:
        pass
    elif isinstance(proto,ScalarTypedDataSpec):
        dst.write('hash.add({}{});\n'.format(prefix,proto.name))
    elif isinstance(proto,TupleTypedDataSpec):
        for elt in proto.elements_by_index:
            render_typed_data_add_hash(elt,dst,prefix+proto.name+".")
    elif isinstance(proto,ArrayTypedDataSpec):
        assert isinstance(proto.type,ScalarTypedDataSpec), "Haven't implemented arrays of non-scalars yet."
        for i in range(0,proto.length):
            dst.write('  hash.add({}{}[{}]);\n'.format(prefix,proto.name,i))
            
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))


def render_typed_data_load_v4_tuple(proto,dst,prefix,indent):
    assert isinstance(proto,TupleTypedDataSpec)
    for elt in proto.elements_by_index:
        dst.write('{}if(document.HasMember("{}")){{\n'.format(indent,elt.name))
        dst.write('{}  const rapidjson::Value &n=document["{}"];\n'.format(indent,elt.name))
        if isinstance(elt,ScalarTypedDataSpec):

            if elt.type=="int32_t" or elt.type=="int16_t" or elt.type=="int8_t" :
                dst.write('{}  {}{}=n.GetInt();\n'.format(indent, prefix, elt.name))

            elif elt.type=="uint32_t" or elt.type=="uint16_t" or elt.type=="uint8_t":
                dst.write('{}  {}{}=n.GetUint();\n'.format(indent, prefix, elt.name))

            elif elt.type=="float":
                dst.write('{}  {}{}=n.GetDouble();\n'.format(indent, prefix, elt.name))
            else:
                raise RuntimeError("Unknown scalar data type.")
        elif isinstance(elt,TupleTypedDataSpec):
            render_typed_data_load_v4_tuple(elt,dst,prefix+elt.name+".",indent+"    ")
        elif isinstance(elt,ArrayTypedDataSpec):
            assert isinstance(elt.type,ScalarTypedDataSpec), "Haven't implemented non-scalar arrays yet."
            dst.write('{}  assert(n.IsArray());\n')
            for i in range(0,elt.length):

                if elt.type.type=="int32_t" or elt.type.type=="int16_t" or elt.type.type=="int8_t":
                    dst.write('{}  assert(n[{}].IsInt());\n'.format(indent,i))
                    dst.write('{}  {}{}[{}]=n[{}].GetInt();\n'.format(indent, prefix, elt.name,i,i))

                elif elt.type.type=="uint32_t" or elt.type.type=="uint16_t" or elt.type.type=="uint8_t":
                    dst.write('{}  assert(n[{}].IsUint());\n'.format(indent,i))
                    dst.write('{}  {}{}[{}]=n[{}].GetUint();\n'.format(indent, prefix,elt.name,i,i))

                elif elt.type.type=="float":
                    dst.write('{}  assert(n[{}].IsDouble());\n'.format(indent,i))
                    dst.write('{}  {}{}[{}]=n[{}].GetDouble();\n'.format(indent, prefix,elt.name,i,i))
                else:
                    raise RuntimeError("Unknown scalar data type.")
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
    render_typed_data_load_v4_tuple(proto,dst,prefix,indent)
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

def render_typed_data_save(proto, dst, prefix, indent):
    if isinstance(proto,ScalarTypedDataSpec):

        if proto.type=="uint32_t" or proto.type=="uint16_t" or proto.type=="uint8_t":
            dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<(uint32_t){}{}; sep=true; }}\n'.format(indent, prefix, proto.name, proto.default, proto.name, prefix, proto.name))
        elif proto.type=="int32_t" or proto.type=="int16_t" or proto.type=="int8_t":
            dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<(int32_t){}{}; sep=true; }}\n'.format(indent, prefix, proto.name, proto.default, proto.name, prefix, proto.name))
        else:
            dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<{}{}; sep=true; }}\n'.format(indent, prefix, proto.name, proto.default, proto.name, prefix, proto.name))


    elif isinstance(proto,ArrayTypedDataSpec):
        assert isinstance(proto.type,ScalarTypedDataSpec), "Haven't implemented non-scalar arrays yet."
        dst.write('{}if(sep){{ dst<<","; }}; dst<<"\\"{}\\":[";'.format(indent,proto.name))
        for i in range(proto.length):
            if i>0:
                dst.write('{}  dst<<",";\n'.format(indent))
            dst.write('{}  dst<<{}{}[{}];\n'.format(indent, prefix, proto.name, i))
        dst.write('{}dst<<"]"; sep=true;\n'.format(indent))
    elif isinstance(proto,TupleTypedDataSpec):
        dst.write('{}if(sep){ dst<<","; } sep=true;\n'.format(indent))
        dst.write('{}{{ bool sep=false;\n'.format(indent))
        dst.write('{}dst<<"\\"{}\\"":{{"'.format(indent,proto.name))
        for elt in proto.elements_by_index:
            render_typed_data_save(elt, dst, prefix+"."+proto.name, indent+"  ")
        dst.write('{}dst<<"}}"'.format(indent))
        dst.write('{}}}'.format(indent))
    else:
        assert False, "Unknown data-type"


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
    dst.write("public:\n")
    dst.write("  size_t totalSize() const override {{ return sizeof({})-sizeof(typed_data_t); }}\n".format(name))
    dst.write("  size_t payloadSize() const override {{ return sizeof({}); }}\n".format(name))
    dst.write("  TypedDataPtr create() const override {\n")
    if proto:
        dst.write("    {} *res=({}*)malloc(sizeof({}));\n".format(name,name,name))
        dst.write("    res->_ref_count=0;\n")
        dst.write("    res->_total_size_bytes=sizeof({});\n".format(name))
        for elt in proto.elements_by_index:
            render_typed_data_init(elt, dst, "    res->");
        dst.write("    return TypedDataPtr(res);\n")
    else:
        dst.write("    return TypedDataPtr();\n")
    dst.write("  }\n")
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
    
    dst.write("};\n")
    dst.write("TypedDataSpecPtr {}_Spec_get(){{\n".format(name))
    dst.write("  static TypedDataSpecPtr singleton(new {}_Spec);\n".format(name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def emit_device_global_constants(dt,subs,indent):
    res="""
{indent}const unsigned INPUT_COUNT_{deviceTypeId} = {inputCount};
{indent}const unsigned OUTPUT_COUNT_{deviceTypeId} = {outputCount};
{indent}const unsigned RTC_INDEX_{deviceTypeId}=31;
{indent}const unsigned RTC_FLAG_{deviceTypeId}=0x80000000ul;
""".format(**subs)

    currPortIndex=0
    for ip in dt.inputs_by_index:
        res=res+"""
{indent}const unsigned INPUT_INDEX_{deviceTypeId}_{currPortName}={currPortIndex};
{indent}const unsigned INPUT_FLAG_{deviceTypeId}_{currPortName}=1ul<<{currPortIndex};
{indent}const unsigned INPUT_INDEX_{currPortName}={currPortIndex};
{indent}const unsigned INPUT_FLAG_{currPortName}=1ul<<{currPortIndex};
""".format(currPortName=ip.name,currPortIndex=currPortIndex,**subs)
        currPortIndex+=1

    currPortIndex=0
    for ip in dt.outputs_by_index:
        res=res+"""
{indent}const unsigned OUTPUT_INDEX_{deviceTypeId}_{currPortName}={currPortIndex};
{indent}const unsigned OUTPUT_FLAG_{deviceTypeId}_{currPortName}=1ul<<{currPortIndex};
{indent}const unsigned RTS_INDEX_{deviceTypeId}_{currPortName}={currPortIndex};
{indent}const unsigned RTS_FLAG_{deviceTypeId}_{currPortName}=1ul<<{currPortIndex};
{indent}const unsigned OUTPUT_INDEX_{currPortName}={currPortIndex};
{indent}const unsigned OUTPUT_FLAG_{currPortName}=1ul<<{currPortIndex};
{indent}const unsigned RTS_INDEX_{currPortName}={currPortIndex};
{indent}const unsigned RTS_FLAG_{currPortName}=1ul<<{currPortIndex};
""".format(currPortName=ip.name,currPortIndex=currPortIndex,**subs)
        currPortIndex+=1

    return res

def emit_device_local_constants(dt,subs,indent=""):
    return """
{indent}typedef {graphPropertiesStructName} GRAPH_PROPERTIES_T;
{indent}typedef {devicePropertiesStructName} DEVICE_PROPERTIES_T;
{indent}typedef {deviceStateStructName} DEVICE_STATE_T;
""".format(**subs)

def emit_input_port_local_constants(dt,subs,indent=""):
    return """
{indent}typedef {pinPropertiesStructName} PORT_PROPERTIES_T;
{indent}typedef {pinStateStructName} PORT_STATE_T;
{indent}typedef {messageStructName} MESSAGE_T;
""".format(**subs)

def emit_output_port_local_constants(dt,subs,indent=""):
    return """
{indent}typedef {messageStructName} MESSAGE_T;
""".format(**subs)



def render_input_port_as_cpp(ip,dst):
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
        "portName"                      : ip.name,
        "portIndex"                     : index,
        "pinPropertiesStructName"       : "{}_{}_properties_t".format(dt.id,ip.name),
        "pinStateStructName"            : "{}_{}_state_t".format(dt.id,ip.name),
        "handlerCode"                   : ip.receive_handler,
        "inputCount"                    : len(dt.inputs),
        "outputCount"                   : len(dt.outputs),
        "indent"                        : "  "
    }

    subs["deviceGlobalConstants"]=emit_device_global_constants(dt,subs,"    ")
    subs["deviceLocalConstants"]=emit_device_local_constants(dt,subs, "    ")
    subs["pinLocalConstants"]=emit_input_port_local_constants(dt,subs, "    ")


    if ip.source_line and ip.source_file:
        subs["preProcLinePragma"]= '#line {} "{}"\n'.format(ip.source_line-1,ip.source_file)
    else:
        subst["preProcLinePragma"]="// No line/file information for handler"

    dst.write(
"""
//MessageTypePtr {messageTypeId}_Spec_get();

static const char *{deviceTypeId}_{portName}_handler_code=R"CDATA({handlerCode})CDATA";

class {deviceTypeId}_{portName}_Spec
    : public InputPortImpl {{
public:
  {deviceTypeId}_{portName}_Spec()
    : InputPortImpl(
        {deviceTypeId}_Spec_get,  //
        "{portName}",
        {portIndex},
        {messageTypeId}_Spec_get(),
        {pinPropertiesStructName}_Spec_get(),
        {pinStateStructName}_Spec_get(),
        {deviceTypeId}_{portName}_handler_code
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

    {deviceGlobalConstants}
    {deviceLocalConstants}
    {pinLocalConstants}

    auto graphProperties=cast_typed_properties<{graphPropertiesStructName}>(gGraphProperties);
    auto deviceProperties=cast_typed_properties<{devicePropertiesStructName}>(gDeviceProperties);
    auto deviceState=cast_typed_data<{deviceStateStructName}>(gDeviceState);
    auto edgeProperties=cast_typed_properties<{pinPropertiesStructName}>(gEdgeProperties);
    auto edgeState=cast_typed_data<{pinPropertiesStructName}>(gEdgeState);
    auto message=cast_typed_properties<{messageStructName}>(gMessage);
    HandlerLogImpl handler_log(orchestrator);

    // Begin custom handler
    {preProcLinePragma}
    {handlerCode}
    __POETS_REVERT_PREPROC_DETOUR__
    // End custom handler

  }}
}};

InputPortPtr {deviceTypeId}_{portName}_Spec_get(){{
  static InputPortPtr singleton(new {deviceTypeId}_{portName}_Spec);
  return singleton;
}}
""".format(**subs))

    #    end of render_input_port_as_cpp
    return None

def render_output_port_as_cpp(op,dst):
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
        "portName"                      : op.name,
        "portIndex"                     : index,
        "pinPropertiesStructName"       : "{}_{}_properties_t".format(dt.id,op.name),
        "pinStateStructName"            : "{}_{}_state_t".format(dt.id,op.name),
        "handlerCode"                   : op.send_handler,
        "inputCount"                    : len(dt.inputs),
        "outputCount"                   : len(dt.outputs),
        "indent"                        : "  "
    }

    subs["deviceGlobalConstants"]=emit_device_global_constants(dt,subs,"    ")
    subs["deviceLocalConstants"]=emit_device_local_constants(dt,subs, "    ")
    subs["pinLocalConstants"]=emit_output_port_local_constants(dt,subs, "    ")


    if op.source_line and op.source_file:
        subs["preProcLinePragma"]= '#line {} "{}"\n'.format(op.source_line-1,op.source_file)
    else:
        subst["preProcLinePragma"]="// No line/file information for handler"


    #dst.write('MessageTypePtr {}_Spec_get();\n\n'.format(op.message_type.id))

    dst.write('static const char *{}_{}_handler_code=R"CDATA({})CDATA";\n'.format(dt.id, op.name, op.send_handler))

    dst.write("class {}_{}_Spec : public OutputPortImpl {{\n".format(dt.id,op.name))
    dst.write('  public: {}_{}_Spec() : OutputPortImpl({}_Spec_get, "{}", {}, {}_Spec_get(), {}_{}_handler_code) {{}} \n'.format(dt.id,op.name, dt.id, op.name, index, op.message_type.id, dt.id, op.name))
    dst.write("""    virtual void onSend(
                      OrchestratorServices *orchestrator,
                      const typed_data_t *gGraphProperties,
		      const typed_data_t *gDeviceProperties,
		      typed_data_t *gDeviceState,
		      typed_data_t *gMessage,
		      bool *doSend
		      ) const override {""")
    dst.write('    {deviceGlobalConstants}\n'.format(**subs));
    dst.write('    {deviceLocalConstants}\n'.format(**subs));
    dst.write('    {pinLocalConstants}\n'.format(**subs));

    dst.write('    auto graphProperties=cast_typed_properties<{}_properties_t>(gGraphProperties);\n'.format( graph.id ))
    dst.write('    auto deviceProperties=cast_typed_properties<{}_properties_t>(gDeviceProperties);\n'.format( dt.id ))
    dst.write('    auto deviceState=cast_typed_data<{}_state_t>(gDeviceState);\n'.format( dt.id ))
    dst.write('    auto message=cast_typed_data<{}_message_t>(gMessage);\n'.format(op.message_type.id))
    dst.write('    HandlerLogImpl handler_log(orchestrator);\n')

    dst.write('    // Begin custom handler\n')
    if op.source_line and op.source_file:
        dst.write('#line {} "{}"\n'.format(op.source_line,op.source_file))
    for line in op.send_handler.splitlines():
        dst.write('    {}\n'.format(line))
    dst.write("__POETS_REVERT_PREPROC_DETOUR__")
    dst.write('    // End custom handler\n')

    dst.write('  }\n')
    dst.write("};\n")
    dst.write("OutputPortPtr {}_{}_Spec_get(){{\n".format(dt.id,op.name))
    dst.write("  static OutputPortPtr singleton(new {}_{}_Spec);\n".format(dt.id,op.name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def render_message_type_as_cpp_fwd(et,dst,asHeader):
    render_typed_data_as_spec(et.message, "{}_message_t".format(et.id), "pp:Message", dst, asHeader)

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
    render_typed_data_as_spec(dt.state, "{}_state_t".format(dt.id), "pp:State", dst, asHeader)
    for ip in dt.inputs.values():
        render_typed_data_as_spec(ip.properties, "{}_{}_properties_t".format(dt.id,ip.name), "pp:Properties", dst)
        render_typed_data_as_spec(ip.state, "{}_{}_state_t".format(dt.id,ip.name), "pp:State", dst)

def render_device_type_as_cpp(dt,dst):
    dst.write("namespace ns_{}{{\n".format(dt.id));

    if dt.shared_code:
        for i in dt.shared_code:
            dst.write(i)

    dst.write("DeviceTypePtr {}_Spec_get();\n".format(dt.id))

    for ip in dt.inputs.values():
        render_input_port_as_cpp(ip,dst)

    for op in dt.outputs.values():
        render_output_port_as_cpp(op,dst)

    dst.write("class {}_Spec : public DeviceTypeImpl {{\n".format(dt.id))
    dst.write("public:\n")
    dst.write("  {}_Spec()\n".format(dt.id))
    dst.write('  : DeviceTypeImpl("{}", {}_properties_t_Spec_get(), {}_state_t_Spec_get(),\n'.format(dt.id, dt.id, dt.id))
    dst.write('      std::vector<InputPortPtr>({')
    first=True
    for i in dt.inputs_by_index:
        if first:
            first=False
        else:
            dst.write(',')
        dst.write('{}_{}_Spec_get()'.format(dt.id,i.name))
    dst.write('}),\n')
    dst.write('      std::vector<OutputPortPtr>({')
    first=True
    for o in dt.outputs_by_index:
        if first:
            first=False
        else:
            dst.write(',')
        dst.write('{}_{}_Spec_get()'.format(dt.id,o.name))
    dst.write('}))\n')
    dst.write("  {}\n")

    subs={
        "indent"                        : "    ",
        "graphPropertiesStructName"     : "{}_properties_t".format(dt.parent.id),
        "deviceTypeId"                  : dt.id,
        "devicePropertiesStructName"    : "{}_properties_t".format(dt.id),
        "deviceStateStructName"         : "{}_state_t".format(dt.id),
        "handlerCode"                   : dt.ready_to_send_handler,
        "inputCount"                    : len(dt.inputs),
        "outputCount"                   : len(dt.outputs)
    }
    if dt.ready_to_send_source_line and dt.ready_to_send_source_file:
        subs["preProcLinePragma"]= '#line {} "{}"\n'.format(dt.ready_to_send_source_line-1,dt.ready_to_send_source_file)
    else:
        subst["preProcLinePragma"]="// No line/file information for handler"

    subs["deviceGlobalConstants"]=emit_device_global_constants(dt,subs,"    ")
    subs["deviceLocalConstants"]=emit_device_local_constants(dt,subs, "    ")

    dst.write(
"""
  virtual uint32_t calcReadyToSend(
    OrchestratorServices *orchestrator,
    const typed_data_t *gGraphProperties,
    const typed_data_t *gDeviceProperties,
    const typed_data_t *gDeviceState
  ) const override {{
    auto graphProperties=cast_typed_properties<{graphPropertiesStructName}>(gGraphProperties);
    auto deviceProperties=cast_typed_properties<{devicePropertiesStructName}>(gDeviceProperties);
    auto deviceState=cast_typed_properties<{deviceStateStructName}>(gDeviceState);
    HandlerLogImpl handler_log(orchestrator);

    {deviceGlobalConstants}
    {deviceLocalConstants}

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

    dst.write("};\n")
    dst.write("DeviceTypePtr {}_Spec_get(){{\n".format(dt.id))
    dst.write("  static DeviceTypePtr singleton(new {}_Spec);\n".format(dt.id))
    dst.write("  return singleton;\n")
    dst.write("}\n")

    dst.write("}}; //namespace ns_{}\n\n".format(dt.id));

    registrationStatements.append('registry->registerDeviceType(ns_{}::{}_Spec_get());'.format(dt.id,dt.id,dt.id))

def render_graph_as_cpp(graph,dst, destPath, asHeader=False):
    gt=graph

    if asHeader:
        dst.write('#ifndef {}_header\n'.format(gt.id))
        dst.write('#define {}_header\n'.format(gt.id))

    dst.write('#include "graph.hpp"\n')
    dst.write('#include "rapidjson/document.h"\n')

    render_typed_data_as_spec(gt.properties, "{}_properties_t".format(gt.id),"pp:Properties",dst,asHeader)

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
    dst.write("// DEF\n")
    for et in gt.message_types.values():
        render_message_type_as_cpp(et,dst)

    for dt in gt.device_types.values():
        render_device_type_as_cpp(dt,dst)

    dst.write("class {}_Spec : public GraphTypeImpl {{\n".format(gt.id))
    dst.write('  public: {}_Spec() : GraphTypeImpl("{}", {}_properties_t_Spec_get()) {{\n'.format(gt.id,gt.id,gt.id))
    for et in gt.message_types.values():
        dst.write('    addMessageType({}_Spec_get());\n'.format(et.id))
    for dt in gt.device_types.values():
        dst.write('    addDeviceType(ns_{}::{}_Spec_get());\n'.format(dt.id,dt.id))
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
