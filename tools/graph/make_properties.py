from graph.core import *

def add_properties(a,b):
    r=dict(a)
    for (k,v) in b.items():
        r[k]=v
    return r
    
def make_graph_type_properties(gt):
    return {
        "GRAPH_TYPE_ID" : gt.id,
        "GRAPH_TYPE_PROPERTIES_T" : "{}_properties_t".format(gt.id),
        "GRAPH_TYPE_MESSAGE_TYPE_COUNT" : len(gt.message_types),
        "GRAPH_TYPE_DEVICE_TYPE_COUNT" : len(gt.device_types)
    }

def make_device_type_properties(dt):
    if dt.ready_to_send_source_line and dt.ready_to_send_source_file: 
        preProc = '#line {} "{}"\n'.format(dt.ready_to_send_source_line-1,dt.ready_to_send_source_file) 
    else:
        preProc = "// No line/file information for handler"

    return add_properties(make_graph_type_properties(dt.parent),{
        "DEVICE_TYPE_ID" : dt.id,
        "DEVICE_TYPE_FULL_ID" : "{}_{}".format(dt.parent.id,dt.id),
        "DEVICE_TYPE_INDEX" : sorted(list(dt.parent.device_types.keys())).index(dt.id), # index is by id
        "DEVICE_TYPE_PROPERTIES_T" : "{}_{}_properties_t".format(dt.parent.id,dt.id),
        "DEVICE_TYPE_STATE_T" : "{}_{}_state_t".format(dt.parent.id,dt.id),
        "DEVICE_TYPE_INPUT_COUNT" : len(dt.inputs),
        "DEVICE_TYPE_OUTPUT_COUNT" : len(dt.outputs),
        "DEVICE_TYPE_RTS_HANDLER" : dt.ready_to_send_handler,
        "DEVICE_TYPE_RTS_HANDLER_SOURCE_LOCATION" : preProc,
        "DEVICE_TYPE_IS_EXTERNAL" : 1 if dt.isExternal else 0
        
    })
    
def make_message_type_properties(mt):
    return add_properties(make_graph_type_properties(mt.parent),{
        "MESSAGE_TYPE_ID" : mt.id,
        "MESSAGE_TYPE_FULL_ID" : "{}_{}".format(mt.parent.id,mt.id),
        "MESSAGE_TYPE_T" : "{}_{}_message_t".format(mt.parent.id,mt.id)
    })
    
def make_input_pin_properties(ip):
    if ip.source_line and ip.source_file: 
        preProc = '#line {} "{}"\n'.format(ip.source_line-1,ip.source_file) 
    else:
        preProc = "// No line/file information for handler"
    
    # names smaller than this get optimised by the compiler, breaking sending string addrs to host
    name = ip.name
    if len(ip.name) <= 4:
        spaces=' '
        for i in range(4 - len(ip.name)):
            spaces = spaces + ' ' 
        name = ip.name + spaces 

    return add_properties(make_device_type_properties(ip.parent),{
        "INPUT_PORT_NAME" : name,
        "INPUT_PORT_INDEX" : ip.parent.inputs_by_index.index(ip),
        "INPUT_PORT_FULL_ID" : "{}_{}_{}".format(ip.parent.parent.id,ip.parent.id,ip.name),
        "INPUT_PORT_PROPERTIES_T" : "{}_{}_{}_properties_t".format(ip.parent.parent.id,ip.parent.id,ip.name),
        "INPUT_PORT_STATE_T" : "{}_{}_{}_state_t".format(ip.parent.parent.id,ip.parent.id,ip.name),
        "INPUT_PORT_MESSAGE_T" : "{}_{}_message_t".format(ip.parent.parent.id,ip.message_type.id),
        "INPUT_PORT_RECEIVE_HANDLER" : ip.receive_handler,
        "INPUT_PORT_RECEIVE_HANDLER_SOURCE_LOCATION" : preProc,
	"IS_APPLICATION" : 0
    })
    
def make_output_pin_properties(op):
    if op.source_line and op.source_file: 
        preProc = '#line {} "{}"\n'.format(op.source_line-1,op.source_file) 
    else:
        preProc = "// No line/file information for handler"
    
    # names smaller than this get optimised by the compiler, breaking sending string addrs to host
    name = op.name
    if len(op.name) <= 4:
        spaces=' '
        for i in range(4 - len(op.name)):
            spaces = spaces + ' ' 
        name = op.name + spaces 

    return add_properties(make_device_type_properties(op.parent),{
        "OUTPUT_PORT_NAME" : name,
        "OUTPUT_PORT_INDEX" : op.parent.outputs_by_index.index(op),
        "OUTPUT_PORT_FULL_ID" : "{}_{}_{}".format(op.parent.parent.id,op.parent.id,op.name),
        "OUTPUT_PORT_MESSAGE_T" : "{}_{}_message_t".format(op.parent.parent.id,op.message_type.id),
        "OUTPUT_PORT_SEND_HANDLER" : op.send_handler,
        "OUTPUT_PORT_SEND_HANDLER_SOURCE_LOCATION" : preProc,
        "MESSAGETYPE_NUMID" : op.message_type.numid,
	"IS_APPLICATION" : 0
    })
    
    
