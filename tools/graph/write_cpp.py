from graph.core import *

import sys

registrationStatements=[]

def typed_data_packed_size(proto):
    if proto in None:
        return 0
    elif isinstance(proto,ScalarTypedDataSpec):
        if proto.type=="uint32_t": return 4
        elif proto.type=="int32_t": return 4
        elif proto.type=="bool": return 1
        elif proto.type=="float": return 4
        else raise RuntimeError("Uknknown scalar data type {}".format(proto.type))
    elif isinstance(proto,TupleTypedDataSpec):
        acc=0;
        for elt in proto.elements_by_index:
            acc += typed_data_packed_size(proto)
        return acc
    elif isinstance(proto,ArrayTypedDataSpec):
        return typed_data_packed_size(proto.type)*proto.length;
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))

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
        if proto.value:
            value=proto.value
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
            if proto.type.value is not None:
                dst.write('{}{}[{}] = {};\n'.format(prefix,proto.name,i,proto.type.value))
            else:
                dst.write('{}{}[{}] = 0;\n'.format(prefix,proto.name,i))

    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))


def render_typed_data_load_v4_tuple(proto,dst,prefix,indent):
    assert isinstance(proto,TupleTypedDataSpec)
    for elt in proto.elements_by_index:
        dst.write('{}if(document.HasMember("{}")){{\n'.format(indent,elt.name))
        dst.write('{}  const rapidjson::Value &n=document["{}"];\n'.format(indent,elt.name))
        if isinstance(elt,ScalarTypedDataSpec):
            if elt.type=="int32_t":
                dst.write('{}  {}{}=n.GetInt();\n'.format(indent, prefix, elt.name))
            elif elt.type=="uint32_t":
                dst.write('{}  {}{}=n.GetUint();\n'.format(indent, prefix, elt.name))
            elif elt.type=="bool":
                dst.write('{}  {}{}=n.GetBool();\n'.format(indent, prefix, elt.name))
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
                if elt.type.type=="int32_t":
                    dst.write('{}  assert(n[{}].IsInt());\n'.format(indent,i))
                    dst.write('{}  {}{}[{}]=n[{}].GetInt();\n'.format(indent, prefix, elt.name,i,i))
                elif elt.type.type=="uint32_t":
                    dst.write('{}  assert(n[{}].IsUint());\n'.format(indent,i))
                    dst.write('{}  {}{}[{}]=n[{}].GetUint();\n'.format(indent, prefix,elt.name,i,i))
                elif elt.type.type=="bool":
                    dst.write('{}  assert(n[{}].IsBool());\n'.format(indent,i))
                    dst.write('{}  {}{}[{}]=n[{}].GetBool();\n'.format(indent, prefix, elt.name,i,i))
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
        dst.write('{}if({}{} != {}){{ if(sep){{ dst<<","; }}; dst<<"\\"{}\\":"<<{}{}; sep=true; }}\n'.format(indent, prefix, proto.name, proto.value, proto.name, prefix, proto.name))
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

def render_typed_data_as_spec(proto,name,elt_name,dst):
    dst.write("struct {} : typed_data_t{{\n".format(name))
    if proto:
        assert isinstance(proto, TupleTypedDataSpec)
        for elt in proto.elements_by_index:
            render_typed_data_as_decl(elt,dst,"  ")
    else:
        dst.write("  //empty\n")
    dst.write("};\n")
    dst.write("class {}_Spec : public TypedDataSpec {{\n".format(name))
    dst.write("  public: TypedDataPtr create() const override {\n")
    if proto:
        dst.write("    {} *res=({}*)malloc(sizeof({}));\n".format(name,name,name))
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
        dst.write('    ns["g"]="TODO/POETS/virtual-graph-schema-v0";\n')
        dst.write("    {} *res=({}*)malloc(sizeof({}));\n".format(name,name,name))
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
    dst.write("};\n")
    dst.write("TypedDataSpecPtr {}_Spec_get(){{\n".format(name))
    dst.write("  static TypedDataSpecPtr singleton(new {}_Spec);\n".format(name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def render_input_port_as_cpp(ip,dst):
    dt=ip.parent
    et=ip.edge_type
    graph=dt.parent
    for index in range(len(dt.inputs_by_index)):
        if ip==dt.inputs_by_index[index]:
            break

    dst.write('EdgeTypePtr {}_Spec_get();\n\n'.format(ip.edge_type.id))

    dst.write('static const char *{}_{}_handler_code=R"CDATA({})CDATA";\n'.format(dt.id, ip.name, ip.receive_handler))

    dst.write("class {}_{}_Spec : public InputPortImpl {{\n".format(dt.id,ip.name))
    dst.write('  public: {}_{}_Spec() : InputPortImpl({}_Spec_get, "{}", {}, {}_Spec_get(), {}_{}_handler_code) {{}} \n'.format(dt.id,ip.name, dt.id, ip.name, index, ip.edge_type.id, dt.id, ip.name))
    dst.write("""  virtual void onReceive(
                         OrchestratorServices *orchestrator,
                         const typed_data_t *gGraphProperties,
			 const typed_data_t *gDeviceProperties,
			 typed_data_t *gDeviceState,
			 const typed_data_t *gEdgeProperties,
			 typed_data_t *gEdgeState,
                         const typed_data_t *gMessage,
			 bool *requestSend
		      ) const override {\n""")
    dst.write('    auto graphProperties=cast_typed_properties<{}_properties_t>(gGraphProperties);\n'.format( graph.id ))
    dst.write('    auto deviceProperties=cast_typed_properties<{}_properties_t>(gDeviceProperties);\n'.format( dt.id ))
    dst.write('    auto deviceState=cast_typed_data<{}_state_t>(gDeviceState);\n'.format( dt.id ))
    dst.write('    auto edgeProperties=cast_typed_properties<{}_properties_t>(gEdgeProperties);\n'.format( et.id ))
    dst.write('    auto edgeState=cast_typed_data<{}_state_t>(gEdgeState);\n'.format( et.id ))
    dst.write('    auto message=cast_typed_properties<{}_message_t>(gMessage);\n'.format(ip.edge_type.id))
    dst.write('    HandlerLogImpl handler_log(orchestrator);\n')
    for i in range(0,len(dt.outputs_by_index)):
        dst.write('    bool &requestSend_{}=requestSend[{}];\n'.format(dt.outputs_by_index[i].name, i))

    dst.write('    // Begin custom handler\n')
    if ip.source_line and ip.source_file: 
        dst.write('#line {} "{}"\n'.format(ip.source_line,ip.source_file))
    for line in ip.receive_handler.splitlines():
        dst.write('    {}\n'.format(line))
    dst.write('    // End custom handler\n')

    dst.write('  }\n')
    dst.write("};\n")
    dst.write("InputPortPtr {}_{}_Spec_get(){{\n".format(dt.id,ip.name))
    dst.write("  static InputPortPtr singleton(new {}_{}_Spec);\n".format(dt.id,ip.name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def render_output_port_as_cpp(op,dst):
    dt=op.parent
    graph=dt.parent
    for index in range(len(dt.outputs_by_index)):
        if op==dt.outputs_by_index[index]:
            break

    dst.write('EdgeTypePtr {}_Spec_get();\n\n'.format(op.edge_type.id))

    dst.write('static const char *{}_{}_handler_code=R"CDATA({})CDATA";\n'.format(dt.id, op.name, op.send_handler))

    dst.write("class {}_{}_Spec : public OutputPortImpl {{\n".format(dt.id,op.name))
    dst.write('  public: {}_{}_Spec() : OutputPortImpl({}_Spec_get, "{}", {}, {}_Spec_get(), {}_{}_handler_code) {{}} \n'.format(dt.id,op.name, dt.id, op.name, index, op.edge_type.id, dt.id, op.name))
    dst.write("""    virtual void onSend(
                      OrchestratorServices *orchestrator,
                      const typed_data_t *gGraphProperties,
		      const typed_data_t *gDeviceProperties,
		      typed_data_t *gDeviceState,
		      typed_data_t *gMessage,
		      bool *requestSend,
		      bool *cancelSend
		      ) const override {""")
    dst.write('    auto graphProperties=cast_typed_properties<{}_properties_t>(gGraphProperties);\n'.format( graph.id ))
    dst.write('    auto deviceProperties=cast_typed_properties<{}_properties_t>(gDeviceProperties);\n'.format( dt.id ))
    dst.write('    auto deviceState=cast_typed_data<{}_state_t>(gDeviceState);\n'.format( dt.id ))
    dst.write('    auto message=cast_typed_data<{}_message_t>(gMessage);\n'.format(op.edge_type.id))
    for i in range(0,len(dt.outputs_by_index)):
        dst.write('    bool &requestSend_{}=requestSend[{}];\n'.format(dt.outputs_by_index[i].name, i))
    dst.write('    HandlerLogImpl handler_log(orchestrator);\n')

    dst.write('    // Begin custom handler\n')
    if op.source_line and op.source_file: 
        dst.write('#line {} "{}"\n'.format(op.source_line,op.source_file))
    for line in op.send_handler.splitlines():
        dst.write('    {}\n'.format(line))
    dst.write('    // End custom handler\n')

    dst.write('  }\n')
    dst.write("};\n")
    dst.write("OutputPortPtr {}_{}_Spec_get(){{\n".format(dt.id,op.name))
    dst.write("  static OutputPortPtr singleton(new {}_{}_Spec);\n".format(dt.id,op.name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def render_edge_type_as_cpp_fwd(et,dst):
    render_typed_data_as_spec(et.properties, "{}_properties_t".format(et.id), "pp:Properties", dst)
    render_typed_data_as_spec(et.state, "{}_state_t".format(et.id), "pp:State", dst)
    render_typed_data_as_spec(et.message, "{}_message_t".format(et.id), "pp:Message", dst)

def render_edge_type_as_cpp(et,dst):
    dst.write("class {}_Spec : public EdgeTypeImpl {{\n".format(et.id))
    dst.write("public:\n")
    dst.write('  {}_Spec() : EdgeTypeImpl("{}", {}_properties_t_Spec_get(), {}_state_t_Spec_get(), {}_message_t_Spec_get()) {{}}\n'.format(et.id, et.id, et.id, et.id, et.id))
    dst.write('};\n')
    dst.write("EdgeTypePtr {}_Spec_get(){{\n".format(et.id))
    dst.write("  static EdgeTypePtr singleton(new {}_Spec);\n".format(et.id))
    dst.write("  return singleton;\n")
    dst.write("}\n")
    registrationStatements.append('registry->registerEdgeType({}_Spec_get());'.format(et.id,et.id))

def render_device_type_as_cpp_fwd(dt,dst):
    render_typed_data_as_spec(dt.properties, "{}_properties_t".format(dt.id), "pp:Properties", dst)
    render_typed_data_as_spec(dt.state, "{}_state_t".format(dt.id), "pp:State", dst)

def render_device_type_as_cpp(dt,dst):
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
    dst.write("};\n")
    dst.write("DeviceTypePtr {}_Spec_get(){{\n".format(dt.id))
    dst.write("  static DeviceTypePtr singleton(new {}_Spec);\n".format(dt.id))
    dst.write("  return singleton;\n")
    dst.write("}\n")
    registrationStatements.append('registry->registerDeviceType({}_Spec_get());'.format(dt.id,dt.id))

def render_graph_as_cpp(graph,dst):
    dst.write('#include "graph.hpp"\n')
    dst.write('#include "rapidjson/document.h"\n')

    gt=graph


    render_typed_data_as_spec(gt.properties, "{}_properties_t".format(gt.id),"pp:Properties",dst)

    dst.write("/////////////////////////////////\n")
    dst.write("// FWD\n")
    for et in gt.edge_types.values():
        render_edge_type_as_cpp_fwd(et,dst)

    for dt in gt.device_types.values():
        render_device_type_as_cpp_fwd(dt, dst)

    if gt.shared_code:
        for code in gt.shared_code:
            dst.write(code)

    dst.write("/////////////////////////////////\n")
    dst.write("// DEF\n")
    for et in gt.edge_types.values():
        render_edge_type_as_cpp(et,dst)

    for dt in gt.device_types.values():
        render_device_type_as_cpp(dt,dst)

    dst.write("class {}_Spec : public GraphTypeImpl {{\n".format(gt.id))
    dst.write('  public: {}_Spec() : GraphTypeImpl("{}", {}, {}_properties_t_Spec_get()) {{\n'.format(gt.id,gt.id,gt.native_dimension,gt.id))
    for et in gt.edge_types.values():
        dst.write('    addEdgeType({}_Spec_get());\n'.format(et.id))
    for et in gt.device_types.values():
        dst.write('    addDeviceType({}_Spec_get());\n'.format(et.id))
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
