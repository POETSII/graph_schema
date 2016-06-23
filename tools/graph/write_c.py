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
    dst.write("{}struct {} : {} {{\n".format(indent,parent,name))
    if proto:
        render_typed_data_as_decl(proto,dst,indent+"  ")
    else:
        dst.write("{}//empty\n".format(indent))
    dst.write("{}}};\n".format(indent))


def render_typed_data_as_init(proto,dst,indent=""):
    if proto is None:
        pass
    elif isinstance(proto,ScalarData):
        dst.write("{}\n".format(_typed_data_to_typename(indent, type(proto)), proto.name))
    elif isinstance(proto,TupleData):
        dst.write("{{ ")
        for elt in proto.elements_by_index:
            render_typed_data_as_init(elt,dst,indent+"  ")
        dst.write(" }}");
    else:
        raise RuntimeError("Unknown data type.")

def render_edge_type_as_c(et,dst,indent=""):

    graph=et.parent
    
    if et.properties:
        dst.write("{}struct {}_properties_t{{\n".format(indent, et.id))
        render_typed_data_as_decl(et.properties, dst, indent+"  ")
        dst.write("{}}};\n".format(indent))

    if et.state:
        dst.write("{}struct {}_state_t{{\n".format(indent, et.id))
        render_typed_data_as_decl(et.properties,dst, indent+"  ")
        dst.write("{}}};\n".format(indent))
    
def render_device_type_as_c(dt,dst,indent=""):

    graph=dt.parent
    
    render_typed_data_as_struct(dt.properties, "{}_properties_t".format(dt.id), "device_properties_t", dst, indent)
    render_typed_data_as_struct(dt.state, "{}_state_t".format(dt.id), "device_state_t", dst, indent)

    for pi in dt.inputs.values():
        et=pi.edge_type
        
        dst.write("{}void {}_{}_on_receive(\n".format(indent, dt.id, pi.name))
        dst.write("{}  const {}_graph_t *graphProperties,\n".format(indent,graph.id))
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
        dst.write("{}  const {}_graph_t *graphProperties,\n".format(indent,graph.id))
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
    for dt in graph.device_types:
        instances=[di for di in graph.device_types.values() if di.device_type==dt]

        dst.write("{}const int {}_count = {};\n".format(indent, di.device_type.name))
        dst.write("{}const {}_properties_t {}_properties[]={{\n".format(indent,dt.name,dt.name))
        

def render_graph_as_c(graph,dst,indent=""):

    
    if graph.properties:
        dst.write("{}struct {}_properties_t{{\n".format(indent, graph.id))
        render_typed_data_as_decl(graph.properties,dst, indent+"  ")
        dst.write("{}}};\n".format(indent))

    for dt in graph.edge_types.values():
        render_edge_type_as_c(dt,dst,indent+"  ")
    
    for dt in graph.device_types.values():
        render_device_type_as_c(dt,dst,indent+"  ")
