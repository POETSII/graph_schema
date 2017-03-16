from graph.core import *

from graph.make_properties import *

def add_props(a,b):
    r=dict(a)
    for (k,v) in b.items():
        r[k]=v
    return r
    
# c_globals : constants and typedefs that are in scope throughout the graph
# c_locals : constants and typedefs that are only in scope for a particular device or port

def calc_graph_type_c_globals(gt,gtProps=None):
    gtProps=gtProps or make_graph_type_properties(gt)
    res="""
    const char *GRAPH_TYPE_ID="{GRAPH_TYPE_ID}";
    typedef {GRAPH_TYPE_PROPERTIES_T} GRAPH_TYPE_PROPERTIES_T;
    const int GRAPH_TYPE_MESSAGE_TYPE_COUNT = {GRAPH_TYPE_MESSAGE_TYPE_COUNT};
    const int GRAPH_TYPE_DEVICE_TYPE_COUNT = {GRAPH_TYPE_DEVICE_TYPE_COUNT};
    """.format(**gtProps)
    for dt in gt.device_types.values():
        dtProps=make_device_type_properties(dt)
        for op in dt.outputs_by_index:
            opProps=add_props(dtProps,make_output_port_properties(op))
            res=res+"""
            const unsigned RTS_INDEX_{DEVICE_TYPE_ID}_{OUTPUT_PORT_NAME} = {OUTPUT_PORT_INDEX};\n
            const unsigned RTS_FLAG_{DEVICE_TYPE_ID}_{OUTPUT_PORT_NAME} = 1<<{OUTPUT_PORT_INDEX};\n      
            const unsigned OUTPUT_INDEX_{DEVICE_TYPE_ID}_{OUTPUT_PORT_NAME} = {OUTPUT_PORT_INDEX};\n
            const unsigned OUTPUT_FLAG_{DEVICE_TYPE_ID}_{OUTPUT_PORT_NAME} = 1<<{OUTPUT_PORT_INDEX};\n      
            """.format(**opProps)
    return res

def calc_device_type_c_locals(dt,dtProps):
    dt=dt or make_device_type_properties(dt)
    res="""
    const char *DEVICE_TYPE_ID="{DEVICE_TYPE_ID}";
    typedef {DEVICE_TYPE_PROPERTIES_T} DEVICE_TYPE_PROPERTIES_T;
    typedef {DEVICE_TYPE_STATE_T} DEVICE_TYPE_STATE_T;
    """.format(**dtProps)
    for op in dt.outputs_by_index:
        opProps=make_output_port_properties(op)
        res=res+"""
        const unsigned RTS_INDEX_{OUTPUT_PORT_NAME} = {OUTPUT_PORT_INDEX};\n
        const unsigned RTS_FLAG_{OUTPUT_PORT_NAME} = 1<<{OUTPUT_PORT_INDEX};\n      
        const unsigned OUTPUT_INDEX_{OUTPUT_PORT_NAME} = {OUTPUT_PORT_INDEX};\n
        const unsigned OUTPUT_FLAG_{OUTPUT_PORT_NAME} = 1<<{OUTPUT_PORT_INDEX};\n      
        """.format(**opProps)
    return res
