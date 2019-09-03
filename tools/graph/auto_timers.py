from graph.core import *
import sys

def createHeadTimerType(graphType, timer_message):
    # def __init__(self,name,type,default=None,documentation=None):
    boardStartCycleU = ScalarTypedDataSpec("boardStartCycleU", "uint32_t")
    boardStartCycle  = ScalarTypedDataSpec("boardStartCycle", "uint32_t")
    timerSent = ScalarTypedDataSpec("timerSent", "uint8_t")
    # def __init__(self,name,elements,default=None,documentation=None):
    state = TupleTypedDataSpec("timer_state", [boardStartCycleU, boardStartCycle, timerSent])
    # def __init__(self,parent,id,properties,state,metadata=None,shared_code=[], isExternal=False, documentation=None):
    timerType = DeviceType(graphType, "auto_timer_head", None, state)
    timerType.init_handler = """
        deviceState->timerSent = false;
    """
    timerType.ready_to_send_handler = """
        *readyToSend = 0;
        if (!deviceState->timerSent) {{
            *readyToSend = 1;
        }}
    """
    # def add_output(self,name,message_type,is_application, metadata,send_handler,source_file=None,source_line=None,documentation=None):
    sendHandler = """
        deviceState->timerSent = true;
        deviceState->boardStartCycleU = tinselCycleCountU();
        deviceState->boardStartCycle = tinselCycleCount();
        hostMsg hmsg;
        hmsg.source.thread = tinselId();
        hmsg.type = 0xBA;
        hmsg.payload[0] = deviceState->boardStartCycleU;
        hmsg.payload[1] = deviceState->boardStartCycle;
        hostMsgBufferPush(&hmsg);
    """
    timerType.add_output("sync_out", timer_message, False, {}, sendHandler)
    return timerType

def createTimerType(graphType, timer_message):
    # def __init__(self,name,type,default=None,documentation=None):
    boardStartCycleU = ScalarTypedDataSpec("boardStartCycleU", "uint32_t")
    boardStartCycle  = ScalarTypedDataSpec("boardStartCycle", "uint32_t")
    # def __init__(self,name,elements,default=None,documentation=None):
    state = TupleTypedDataSpec("timer_state", [boardStartCycleU, boardStartCycle])
    # def __init__(self,parent,id,properties,state,metadata=None,shared_code=[], isExternal=False, documentation=None):
    timerType = DeviceType(graphType, "auto_timer", None, state)
    timerType.ready_to_send_handler = """
        *readyToSend = 0;
    """
    recvHandler = """
        deviceState->boardStartCycleU = tinselCycleCountU();
        deviceState->boardStartCycle = tinselCycleCount();
        hostMsg hmsg;
        hmsg.source.thread = tinselId();
        hmsg.type = 0xBA;
        hmsg.payload[0] = deviceState->boardStartCycleU;
        hmsg.payload[1] = deviceState->boardStartCycle;
        hostMsgBufferPush(&hmsg);
    """
    # def add_input(self,name,message_type,is_application,properties,state,metadata,receive_handler,source_file=None,source_line={},documentation=None):
    timerType.add_input("sync_in", timer_message, False, None, None, None, recvHandler)
    return timerType

start_timer = """
        if (deviceState->start_time_sent == false) {
            uint32_t startCountU = tinselCycleCountU();
            uint32_t startCount  = tinselCycleCount();
            hostMsg hmsg;
            hmsg.source.thread = tinselId();
            hmsg.type = 0xBB;
            hmsg.payload[0] = startCountU;
            hmsg.payload[1] = startCount;
            hostMsgBufferPush(&hmsg);
            deviceState->start_time_sent = true;
        }
"""

start_timer_variable = ScalarTypedDataSpec("start_time_sent", "uint8_t")

def addStartTimerToDevices(graphType):
    # Look for the comment "// AUTO_TIMER_START" and replace all with code to send start cycle counts to pts-serve
    for dt in graphType.device_types:
        deviceType = graphType.device_types[dt]
        newStateElements = deviceType.state._elts_by_index
        newStateElements.append(start_timer_variable)
        deviceType.state = TupleTypedDataSpec(deviceType.state.name, newStateElements)
        deviceType.init_handler = deviceType.init_handler.replace("// AUTO_TIMER_START", start_timer)
        deviceType.ready_to_send_handler = deviceType.ready_to_send_handler.replace("// AUTO_TIMER_START", start_timer)
        for ip in deviceType.inputs:
            deviceType.inputs[ip].receive_handler = deviceType.inputs[ip].receive_handler.replace("// AUTO_TIMER_START", start_timer)
        for op in deviceType.outputs:
            deviceType.outputs[op].send_handler = deviceType.outputs[op].send_handler.replace("// AUTO_TIMER_START", start_timer)

handler_exit_timer = """
        uint32_t endCountU = tinselCycleCountU();
        uint32_t endCount  = tinselCycleCount();
        hostMsg hmsg;
        hmsg.source.thread = tinselId();
        hmsg.type = 0xBC;
        hmsg.payload[0] = endCountU;
        hmsg.payload[1] = endCount;
        hostMsgBufferPush(&hmsg);
        handler_exit(
"""

def addTimerToHandlerExit(graphType):
    for dt in graphType.device_types:
        deviceType = graphType.device_types[dt]
        deviceType.ready_to_send_handler = deviceType.ready_to_send_handler.replace("handler_exit(", handler_exit_timer)
        for ip in deviceType.inputs:
            deviceType.inputs[ip].receive_handler = deviceType.inputs[ip].receive_handler.replace("handler_exit(", handler_exit_timer)
        for op in deviceType.outputs:
            deviceType.outputs[op].send_handler = deviceType.outputs[op].send_handler.replace("handler_exit(", handler_exit_timer)

def add_auto_timers(graphInstance, numBoards):
    graphType = graphInstance.graph_type

# ADD TIMER CODE FOR POSSIBLE START AND END POINTS - END IS INDICATED BY "handler_exit("" - START IS INDICATED BY "// AUTO_TIMER_START"
    addTimerToHandlerExit(graphType)
    addStartTimerToDevices(graphType)

    # def __init__(self,parent,id,message,metadata=None,cTypeName=None,numid=0,documentation=None):
    timer_message = MessageType(graphType, "timer_message", None)
    # def add_message_type(self,message_type):
    graphType.add_message_type(timer_message)
    headTimerType = createHeadTimerType(graphType, timer_message)
    timerType = createTimerType(graphType, timer_message)
    graphType.add_device_type(headTimerType)
    graphType.add_device_type(timerType)

    headTimer = DeviceInstance(graphInstance, "auto_timer_0", headTimerType)
    graphInstance.add_device_instance(headTimer)
    timers = []
    for i in range(1, numBoards):
        timer = DeviceInstance(graphInstance, "auto_timer_" + str(i), timerType)
        timers.append(timer)
        graphInstance.add_device_instance(timer)

    for t in timers:
        # def __init__(self,parent,dst_device,dst_pin,src_device,src_pin,properties=None,metadata=None):
        edge = EdgeInstance(graphInstance, t, "sync_in", headTimer, "sync_out")
        # def add_edge_instance(self,ei,validate=False):
        graphInstance.add_edge_instance(edge)

    timers.append(headTimer)

    return timers

