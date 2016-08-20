from graph.core import *

_typed_data_to_typename={
    Int32Data: "int32_t",
    Float32Data: "float",
    BoolData:"bool"
    }

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
            dst.write('{}{} = {};\n'.format(prefix,proto.name,proto.value))
    elif isinstance(proto,TupleData):
        for elt in proto.elements_by_index:
            render_typed_data_init(elt,dst,prefix+proto.name+".")
    else:
        raise RuntimeError("Unknown data type {}."format(type(proto))

def render_typed_data_load(proto,dst,elt,prefix, indent):
    if proto is None:
        pass
    elif isinstance(proto,ScalarData):
        dst.write('{}load_typed_data_attribute({}{}, {}, "{}");\n'.format(indent,prefix,proto.name,elt,proto.name))
    elif isinstance(proto,TupleData):
        dst.write('{}{{'.format(indent)):
        dst.write('{}  xmlpp::Element *{}=load_type_data_tuple({}, "{}");\n'.format(indent,proto.name, elt,proto.name))
        dst.write('{}  if({}){{'.format(indent,proto.name))
        for elt in proto.elements_by_index:
            render_typed_data_load(elt,dst,proto.name,prefix+proto.name+".",indent+"    ")
        dst.write('{}  }}'\n.format(indent))
        dst.write('{}}}\n'.format(indent))
    else:
        raise RuntimeError("Unknown data type {}."format(type(proto))
                           

def render_typed_data_as_spec(proto,name,elt_name,dst):
    dst.write("struct {} : typed_data_t{{\n".format(name))
    if proto:
        render_typed_data_as_decl(proto,dst,"  ")
    else:
        dst.write("  //empty\n")
    dst.write("};\n")
    dst.write("class {}_Spec : public TypedDataSpec {{\n".format(name))
    dst.write("  TypedDataPtr create() const override {\n")
    dst.write("    return TypedDataPtr(new {}".format(name))
    dst.write("  }\n")
    dst.write("  TypedDataPtr load(xmlpp::Element *parent) const override {\n")
    if proto is None
        dst.write("    return TypedDataPtr();\n")
    else:
        dst.write("    {} *res=malloc(sizeof({}));\n").format(name,name))
        render_typed_data_init(proto,dst,"    res->")
        dst.write('    xmlpp::Element *elt=find_single(parent, "./{}");\n'.format(elt_name))
        render_typed_data_load(proto, dst, "elt", "res->",  "    ")
        dst.write("    return TypedDataPtr(res);\n")
    dst.write("  }\n")
    dst.write("  void save(xmlpp::Element *parent, TypedDataPtr data) const override {\n")
    dst.write('    throw std::runtime_error("Not implemented.");\n')
    dst.write("  }\n")
    dst.write("};\n")

def render_graph_as_cpp(graph,dst):
    dst.write('#include "graph.hpp"\n')

    render_typed_data_as_struct(graph.properties, "{}_properties_t".format(graph.id))
