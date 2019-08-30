from graph.core import *
import sys

def createTimerType(graphType, timer_message):
    # def __init__(self,name,type,default=None,documentation=None):
    boardStartCycleU = ScalarTypedDataSpec("boardStartCycleU", "uint32_t")
    boardStartCycle  = ScalarTypedDataSpec("boardStartCycle", "uint32_t")
    timerSent = ScalarTypedDataSpec("timerSent", "uint8_t")
    # def __init__(self,name,elements,default=None,documentation=None):
    state = TupleTypedDataSpec("timer_state", [boardStartCycleU, boardStartCycle, timerSent])
    # def __init__(self,parent,id,properties,state,metadata=None,shared_code=[], isExternal=False, documentation=None):
    timerType = DeviceType(graphType, "auto_timer", None, state)
    timerInit = """
        deviceState->timerSent = false;
        deviceState->boardStartCycleU = tinselCycleCountU();
        deviceState->boardStartCycle = tinselCycleCount();
    """
    timerType.init = timerInit
    timerRTS = """
        *readyToSend = 0;
        if (!timerSent) {{
            *readyToSend = 1;
        }}
    """
    # def add_output(self,name,message_type,is_application, metadata,send_handler,source_file=None,source_line=None,documentation=None):
    timerType.add_output("start_app", timer_message, False, {}, '')
    return timerType

def createTimerInstance(number, graphType):
    # __init__(self,id,graph_type,properties=None,metadata=None,documentation=None):
    id = "auto_timer_" + str(number)
    timerInstance = DeviceInstance(id, graph_type)
    return timerInstance

def add_auto_timers(graphInstance):
    graphType = graphInstance.graph_type
    # def __init__(self,parent,id,message,metadata=None,cTypeName=None,numid=0,documentation=None):
    timer_message = MessageType(graphType, "timer_message", None)
    # def add_message_type(self,message_type):
    graphType.add_message_type(timer_message)
    timerType = createTimerType(graphType, timer_message)

