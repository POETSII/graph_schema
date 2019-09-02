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
        hmsg.type = 0xBB;
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
        hmsg.type = 0xBB;
        hmsg.payload[0] = deviceState->boardStartCycleU;
        hmsg.payload[1] = deviceState->boardStartCycle;
        hostMsgBufferPush(&hmsg);
    """
    # def add_input(self,name,message_type,is_application,properties,state,metadata,receive_handler,source_file=None,source_line={},documentation=None):
    timerType.add_input("sync_in", timer_message, False, None, None, None, recvHandler)
    return timerType

def add_auto_timers(graphInstance, numBoards):
    graphType = graphInstance.graph_type
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

