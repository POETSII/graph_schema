from graph.core import *

_typed_data_to_typename={
    Int32Data: "int32_t",
    Float32Data: "float",
    BoolData:"bool"
    }

registrationStatements=[]

def render_typed_data_as_decl(proto,dst,indent=""):
    if proto is None:
        pass
    elif isinstance(proto,ScalarData):
        dst.write("{}{} {};\n".format(indent, _typed_data_to_typename[type(proto)], proto.name))
    elif isinstance(proto,TupleData):
        dst.write("{}struct {{\n".format(indent))
        for elt in proto.elements_by_index:
            render_typed_data_as_decl(elt,dst,indent+"  ")
        dst.write("{}}} {};\n".format(indent,proto.name));
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))

def render_typed_data_init(proto,dst,prefix):
    if proto is None:
        pass
    elif isinstance(proto,ScalarData):
        if proto.value:
            value=proto.value
            dst.write("// {}\n".format(proto.value))
            if isinstance(proto,BoolData):
                value = 1 if value else 0
            dst.write('{}{} = {};\n'.format(prefix,proto.name,value))
        else:
            dst.write('{}{} = 0;\n'.format(prefix,proto.name))
    elif isinstance(proto,TupleData):
        for elt in proto.elements_by_index:
            render_typed_data_init(elt,dst,prefix+proto.name+".")
    else:
        raise RuntimeError("Unknown data type {}.".format(type(proto)))

def render_typed_data_load(proto,dst,elt,prefix, indent):
    if proto is None:
        pass
    elif isinstance(proto,ScalarData):
        dst.write('{}load_typed_data_attribute({}{}, {}, "{}");\n'.format(indent,prefix,proto.name,elt,proto.name))
    elif isinstance(proto,TupleData):
        dst.write('{}{{'.format(indent))
        dst.write('{}  xmlpp::Element *{}=load_typed_data_tuple({}, "{}");\n'.format(indent,proto.name, elt,proto.name))
        dst.write('{}  if({}){{'.format(indent,proto.name))
        for elt in proto.elements_by_index:
            render_typed_data_load(elt,dst,proto.name,prefix+proto.name+".",indent+"    ")
        dst.write('{}  }}\n'.format(indent))
        dst.write('{}}}\n'.format(indent))
    else:
        raise RuntimeError("Unknown data type {}".format(type(proto)))
                           

def render_typed_data_as_spec(proto,name,elt_name,dst):
    dst.write("struct {} : typed_data_t{{\n".format(name))
    if proto:
        assert isinstance(proto, TupleData)
        for elt in proto.elements_by_index:
            render_typed_data_as_decl(elt,dst,"  ")
    else:
        dst.write("  //empty\n")
    dst.write("};\n")
    dst.write("class {}_Spec : public TypedDataSpec {{\n".format(name))
    dst.write("  public: TypedDataPtr create() const override {\n")
    if proto:
        dst.write("    auto res=std::make_shared<{}>();\n".format(name))
        for elt in proto.elements_by_index:
            render_typed_data_init(elt, dst, "    res->");
        dst.write("    return res;\n")
    else:
        dst.write("    return TypedDataPtr();\n")
    dst.write("  }\n")
    dst.write("  TypedDataPtr load(xmlpp::Element *elt) const override {\n")
    if proto is None:
        dst.write("    return TypedDataPtr();\n")
    else:
        dst.write("    xmlpp::Node::PrefixNsMap ns;\n")
        dst.write('    ns["g"]="TODO/POETS/virtual-graph-schema-v0";\n')
        dst.write("    std::shared_ptr<{}> res(new {});\n".format(name,name))
        for elt in proto.elements_by_index:
            render_typed_data_init(elt,dst,"    res->")
        dst.write('    fprintf(stderr, "   Loading {}_Spec, elt=%p\\n",elt);'.format(name))
        dst.write("    if(elt){\n")
        for elt in proto.elements_by_index:
            render_typed_data_load(elt, dst, "elt", "res->",  "      ")
        dst.write("    }\n")
        dst.write("    return res;\n")
    dst.write("  }\n")
    dst.write("  void save(xmlpp::Element *parent, const TypedDataPtr &data) const override {\n")
    dst.write('    throw std::runtime_error("Not implemented.");\n')
    dst.write("  }\n")
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
        
    dst.write("class {}_{}_Spec : public InputPortImpl {{\n".format(dt.id,ip.name))
    dst.write('  public: {}_{}_Spec() : InputPortImpl({}_Spec_get, "{}", {}, {}_Spec_get()) {{}} \n'.format(dt.id,ip.name, dt.id, ip.name, index, ip.edge_type.id))
    dst.write("""  virtual void onReceive(const typed_data_t *gGraphProperties,
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

    dst.write('    // Begin custom handler\n')
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
        
    dst.write("class {}_{}_Spec : public OutputPortImpl {{\n".format(dt.id,op.name))
    dst.write('  public: {}_{}_Spec() : OutputPortImpl({}_Spec_get, "{}", {}, {}_Spec_get()) {{}} \n'.format(dt.id,op.name, dt.id, op.name, index, op.edge_type.id))
    dst.write("""    virtual void onSend(const typed_data_t *gGraphProperties,
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

    dst.write('    // Begin custom handler\n')
    for line in op.send_handler.splitlines():
        dst.write('    {}\n'.format(line))
    dst.write('    // End custom handler\n')
    
    dst.write('  }\n')
    dst.write("};\n")
    dst.write("OutputPortPtr {}_{}_Spec_get(){{\n".format(dt.id,op.name))
    dst.write("  static OutputPortPtr singleton(new {}_{}_Spec);\n".format(dt.id,op.name))
    dst.write("  return singleton;\n")
    dst.write("}\n")

def render_edge_type_as_cpp(et,dst):
    render_typed_data_as_spec(et.properties, "{}_properties_t".format(et.id), "pp:Properties", dst)
    render_typed_data_as_spec(et.state, "{}_state_t".format(et.id), "pp:State", dst)
    render_typed_data_as_spec(et.message, "{}_message_t".format(et.id), "pp:Message", dst)

    dst.write("class {}_Spec : public EdgeTypeImpl {{\n".format(et.id))
    dst.write("public:\n")
    dst.write('  {}_Spec() : EdgeTypeImpl("{}", {}_properties_t_Spec_get(), {}_state_t_Spec_get(), {}_message_t_Spec_get()) {{}}\n'.format(et.id, et.id, et.id, et.id, et.id))
    dst.write('};\n')
    dst.write("EdgeTypePtr {}_Spec_get(){{\n".format(et.id))
    dst.write("  static EdgeTypePtr singleton(new {}_Spec);\n".format(et.id))
    dst.write("  return singleton;\n")
    dst.write("}\n")
    registrationStatements.append('registry->registerEdgeType({}_Spec_get());'.format(et.id,et.id))
    
def render_device_type_as_cpp(dt,dst):
    render_typed_data_as_spec(dt.properties, "{}_properties_t".format(dt.id), "pp:Properties", dst)
    render_typed_data_as_spec(dt.state, "{}_state_t".format(dt.id), "pp:State", dst)

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
    dst.write('#include "graph_impl.hpp"\n')

    gt=graph.graph_type

 
    render_typed_data_as_spec(graph.graph_type.properties, "{}_properties_t".format(graph.graph_type.id),"pp:Properties",dst)

    for et in graph.graph_type.edge_types.values():
        render_edge_type_as_cpp(et,dst)

    for dt in graph.graph_type.device_types.values():
        render_device_type_as_cpp(dt,dst)

    dst.write("class {}_Spec : public GraphTypeImpl {{\n".format(graph.graph_type.id))
    dst.write('  public: {}_Spec() : GraphTypeImpl("{}", {}_properties_t_Spec_get()) {{\n'.format(gt.id,gt.id,gt.id))
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
    

    dst.write("extern void registerGraphTypes(Registry *registry){\n")
    for r in registrationStatements:
        dst.write("  {}\n".format(r))
    dst.write("}\n");
