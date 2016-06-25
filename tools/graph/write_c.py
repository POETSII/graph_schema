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

def render_typed_data_as_struct(proto,name,parent,dst,indent=""):
    if parent:
        dst.write("{}struct {} : {} {{\n".format(indent,name,parent))
    else:
        dst.write("{}struct {}{{\n".format(indent,name))
    if proto:
        render_typed_data_as_decl(proto,dst,indent+"  ")
    else:
        dst.write("{}//empty\n".format(indent))
    dst.write("{}}};\n".format(indent))


def render_typed_data_as_init(proto,dst,indent=""):
    if proto is None:
        pass
    elif isinstance(proto,ScalarData):
        if proto.value is None:
            dst.write("0")
        else:
            dst.write("{}\n".format(proto.value))
    elif isinstance(proto,TupleData):
        dst.write("{{ ")
        for elt in proto.elements_by_index:
            render_typed_data_as_init(elt,dst,indent+"  ")
        dst.write(" }}");
    else:
        raise RuntimeError("Unknown data type.")

def render_typed_data_as_struct_init(proto,dst,indent=""):
    dst.write("{}{{\n".format(indent))
    render_typed_data_as_init(proto,dst,indent+"  ")
    dst.write("{}}}\n".format(indent))


def render_edge_type_as_c(et,dst,indent=""):

    graph=et.parent

    render_typed_data_as_struct(et.properties, "{}_properties_t".format(et.id), None, dst, indent)
    render_typed_data_as_struct(et.state, "{}_state_t".format(et.id), None, dst, indent)
    render_typed_data_as_struct(et.message, "{}_message_t".format(et.id), None, dst, indent)


def render_device_type_as_c(dt,dst,indent=""):

    graph=dt.parent

    render_typed_data_as_struct(dt.properties, "{}_properties_t".format(dt.id), None, dst, indent)
    render_typed_data_as_struct(dt.state, "{}_state_t".format(dt.id), None, dst, indent)

    for pi in dt.inputs.values():
        et=pi.edge_type

        dst.write("{}void {}_{}_on_receive(\n".format(indent, dt.id, pi.name))
        dst.write("{}  const {}_properties_t *graphProperties,\n".format(indent,graph.id))
        dst.write("{}  const {}_properties_t *deviceProperties,\n".format(indent,dt.id))
        dst.write("{}  {}_state_t *deviceState,\n".format(indent,dt.id))
        dst.write("{}  {}_properties_t *edgeProperties,\n".format(indent,et.id))
        dst.write("{}  {}_properties_t *edgeState,\n".format(indent,et.id))
        dst.write("{}  {}_message_t *message,\n".format(indent,et.id))
        dst.write("{}  bool *requestSend\n".format(indent))
        dst.write("{}){{\n".format(indent))
        for s in pi.receive_handler.splitlines():
            dst.write("{}  {}\n".format(indent,s))
        dst.write("{}}}\n".format(indent));

    for pi in dt.outputs.values():
        et=pi.edge_type
        dst.write("{}void {}_{}_on_send(\n".format(indent, dt.id, pi.name))
        dst.write("{}  const {}_properties_t *graphProperties,\n".format(indent,graph.id))
        dst.write("{}  const {}_properties_t *deviceProperties,\n".format(indent,dt.id))
        dst.write("{}  {}_state_t *deviceState,\n".format(indent,dt.id))
        dst.write("{}  {}_message_t *message,\n".format(indent,et.id))
        dst.write("{}  bool *cancelSend,\n".format(indent))
        dst.write("{}  bool *requestSend\n".format(indent))
        dst.write("{}){{\n".format(indent))
        for s in pi.send_handler.splitlines():
            dst.write("{}  {}\n".format(indent,s))
        dst.write("{}}}\n".format(indent));

def render_device_instances_as_c_static_data(graph,dst,indent=""):
    """Loop over all the device types, and create arrays for the property and
    state arrays of all device instances. This creates global static arrays
    representing the entire graph.

    The data-structures created are:
    - EDGE_properties : read-only array of EDGE_properties_t (if EDGE has properties)
    - EDGE_state : mutable array of EDGE_state_t  (if EDGE has state)
    - DEVICE_properties

    """
    for et in graph.edge_types.values():
        dst.write("{}extern const {}_properties_t {}_properties[];\n".format(indent,et.id,et.id))
        dst.write("{}extern const {}_state_t {}_state[];\n".format(indent,et.id,et.id))
    for dt in graph.device_types.values():
        dst.write("{}extern const {}_properties_t {}_properties[];\n".format(indent,dt.id,dt.id))
        dst.write("{}extern const {}_state_t {}_state[];\n".format(indent,dt.id,dt.id))

    diToIndex={}
    for di in graph.device_instances.values():
        diToIndex[di]=len(diToIndex)
    eiToIndex={}
    for ei in graph.edge_instances.values():
        eiToIndex[ei]=len(eiToIndex)

    for dt in graph.device_types.values():
        instances=[di for di in graph.device_instances.values() if di.device_type==dt]

        dst.write("{}const int {}_count = {};\n".format(indent, dt.id, len(instances)))
        dst.write("{}const {}_properties_t {}_properties[]={{\n".format(indent,dt.id,dt.id))
        for i in range(len(instances)):
            render_typed_data_as_struct_init(instances[i].properties,dst,indent+"  ")
            if i+1!=len(instances):
                dst.write("{},".format(indent))
        dst.write("{}}};\n".format(indent))
        dst.write("{}const {}_state_t {}_state[]={{\n".format(indent,dt.id,dt.id))
        for i in range(len(instances)):
            render_typed_data_as_struct_init(dt.state,dst,indent+"  ")
            if i+1!=len(instances):
                dst.write("{},".format(indent))
        dst.write("{}}};\n".format(indent))



def render_graph_as_c(graph,dst,indent=""):
    dst.write('{}#include "runtime_graph.hpp"\n'.format(indent))

    render_typed_data_as_struct(graph.properties, "{}_properties_t".format(graph.id), None, dst,indent)

    for dt in graph.edge_types.values():
        render_edge_type_as_c(dt,dst,indent+"  ")

    for dt in graph.device_types.values():
        render_device_type_as_c(dt,dst,indent+"  ")

    render_device_instances_as_c_static_data(graph,dst,indent)
