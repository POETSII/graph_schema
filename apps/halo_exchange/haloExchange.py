from collections import namedtuple

Device = namedtuple('Device', 'name celltype size properties state updateFunc initFunc')

def generate_graph(graphname, devicelist, haloLength, debug):
	o = generate_graphHeader(graphname)
	o+= generate_sharedcode(graphname, devicelist, debug)
	#o+= generate_fixpt_sharedcode()
	o+= generate_graphProperties(haloLength)	
	o+= generate_messages(haloLength)
	o+= generate_devices(graphname, devicelist, debug)
	o+= '</GraphType>\n'	
	#o+= generate_simpleTestInstance(haloLength, graphname)
	o+= '</Graphs>\n'	
	return o

def generate_devices(graphname, devicelist, debug):
	o= '\n'
	o+= '\t<DeviceTypes>\n'
	for d in devicelist:
		if d.celltype == 'box':		
			o+= generate_NbyN_Cluster(graphname, d.size, debug, d.properties, d.state, d.updateFunc, d.initFunc)	
		elif d.celltype == 'edge':
			o+= generate_lengthN_boundary(graphname, d.size, debug, d.properties, d.state, d.updateFunc, d.initFunc)
		elif d.celltype == 'exit':
			o+= generate_exitNode(debug)
	o+= '\t</DeviceTypes>\n'
	return o
				

def generate_graphProperties(N):
	o=  '\t<Properties>\n'
	o+= '\t\t<Scalar type=\"uint32_t\" name=\"maxTime\" default=\"64\" />\n'
	o+= '\t\t<Scalar type=\"uint32_t\" name=\"nodesPerDevice\" default=\"'+str(N*N)+'\" />\n'
	o+= '\t\t<Scalar type=\"uint32_t\" name=\"dt\" default=\"1\" />\n'
	o+= '\t</Properties>\n'
	o+= '\n'
	return o

def generate_graphHeader(name):
	o = '<?xml version=\"1.0\"?>\n' 
	o+= '<Graphs xmlns=\"https://poets-project.org/schemas/virtual-graph-schema-v2\">\n'
	o+= '\t<GraphType id=\"'+name+'\">\n'
	return o

def generate_lengthN_boundary(graphname, N, debug, properties, state, updateFunc, initFunc):
	o='\n'
	o+='\t<DeviceType id=\"boundary_'+str(N)+'\">\n'
	o+='\t\t<Properties>\n'
	o+='\t\t\t<Scalar type=\"uint32_t\" name=\"haloLength\" default=\"'+str(N)+'\" />\n'
	o+=properties
	o+='\t\t</Properties>\n'
	o+='\t\t<State>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"cV\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"nV\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"v\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Scalar type=\"uint32_t\" name=\"t\"/>\n'
	o+='\t\t\t<Scalar type=\"int8_t\" name=\"cAvail\"/>\n'
	o+='\t\t\t<Scalar type=\"int8_t\" name=\"nAvail\"/>\n'
	o+=state
	o+='\t\t</State>\n'
	o+='\n'
	
	o+='\t\t<ReadyToSend><![CDATA[\n'
	o+='\t\t\t*readyToSend=0;\n'
	o+='\t\t\tif (deviceState->cAvail) {\n'	
	o+='\t\t\t\t*readyToSend = RTS_FLAG_out;\n'
	o+='\t\t\t} else if (deviceState->t >= graphProperties->maxTime) {\n'
	o+='\t\t\t\t*readyToSend = RTS_FLAG_finish;\n'
	o+='\t\t\t}\n'
	o+='\t\t]]></ReadyToSend>\n'
	o+='\n'

	o+='\t\t<InputPin name=\"__init__\" messageTypeId=\"__init__\">\n'
	o+='\t\t\t<OnReceive><![CDATA[\n'
	if debug:
		o+='\t\t\t\thandler_log(2, "edge device is being initialised");\n'
	o+='\t\t\t\tdeviceState->t = 0;\n'
	o+='\t\t\t\tdeviceState->cAvail = 0;\n'
	o+='\t\t\t\tdeviceState->nAvail = 0;\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<deviceProperties->haloLength; i++){\n'
	o+='\t\t\t\t\tdeviceState->cV[i] = 0;\n'
	o+='\t\t\t\t\tdeviceState->nV[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tboundary_initFunc(deviceState, deviceProperties);\n'
	o+='\t\t\t]]></OnReceive>\n'
	o+='\t\t</InputPin>\n'
	o+='\n'


	o+='\t\t<OutputPin name=\"out\" messageTypeId=\"update\">\n'	
	o+='\t\t\t<OnSend><![CDATA[\n'
	o+='\t\t\t\tuint32_t hL = deviceProperties->haloLength;\n'
	o+='\t\t\t\tfor(uint32_t i=0; i< hL; i++) {\n'
	o+='\t\t\t\t\tmessage->v[i] = deviceState->v[i];\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tmessage->t = deviceState->t;\n'
	if debug:
		o+='\t\t\t\thandler_log(2, \"boundary is sending a message.  message->t=%d\", message->t);\n\n'
	o+='\t\t\t\t\tboundary_updateFunc(deviceState, deviceProperties);\n'
	o+='\t\t\t\t\tdeviceState->t = deviceState->t + graphProperties->dt;\n'
	if debug:
		o+='\t\t\t\thandler_log(2, \"boundary is movint to the next timestep %d\", message->t);\n\n'
	o+='\t\t\t\t\tdeviceState->cAvail = 0;\n\n'
	o+='\t\t\t\tif(deviceState->nAvail) {\n'
	o+='\t\t\t\t\tdeviceState->nAvail = 0;\n'
	o+='\t\t\t\t\tdeviceState->cAvail = 1;\n'
	o+='\t\t\t\t\tfor(uint32_t i=0; i<hL; i++) {\n'
	o+='\t\t\t\t\t\tdeviceState->cV[i] = deviceState->nV[i];\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t]]></OnSend>\n'
	o+='\t\t</OutputPin>\n'
	o+='\n'

	o+='\t\t<OutputPin name=\"finish\" messageTypeId=\"finished\">\n'
	o+='\t\t\t<OnSend><![CDATA[\n'
	o+='\t\t\t]]></OnSend>\n'
	o+='\t\t</OutputPin>\n'
	o+='\n'
	
	o+='\t\t<InputPin name=\"in\" messageTypeId=\"update\">\n'
	o+='\t\t\t<OnReceive><![CDATA[\n'
	if debug:
		o+='\t\t\t\thandler_log(2, \"received message. state->t=%d \t message->t=%d\", deviceState->t, message->t);\n'
	o+='\t\t\t\tuint32_t hL = deviceProperties->haloLength;\n'
	o+='\t\t\t\tif(message->t == deviceState->t) {\n'
	o+='\t\t\t\t\tdeviceState->cAvail = 1;\n'
	o+='\t\t\t\t\tfor(uint32_t i=0; i<hL; i++) {\n'
	o+='\t\t\t\t\tdeviceState->cV[i] = message->v[i];\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\t} else if (message->t == deviceState->t+graphProperties->dt) {\n'
	o+='\t\t\t\t\tdeviceState->nAvail = 1;\n'
	o+='\t\t\t\t\tfor(uint32_t i=0; i<hL; i++) {\n'
	o+='\t\t\t\t\t\tdeviceState->nV[i] = message->v[i];\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t]]></OnReceive>\n'
	o+='\t\t</InputPin>\n'

	o+='\t</DeviceType>\n'
	return o 

def generate_NbyN_Cluster(graphname, N, debug, properties, state, updateFunc, initFunc): 
#returns a string containing xml code describing a halo exchange N x N device
	o='\n'
	o+='\t<DeviceType id=\"cell_' + str(N) +'by'+str(N)+'\">\n' 	
	o+='\t\t<Properties>\n'
	o+='\t\t\t<Scalar type=\"int32_t\" name=\"updateDelta\" />\n' 
	o+='\t\t\t<Scalar type=\"uint32_t\" name=\"haloLength\" default=\"'+str(N)+'\" />\n' 
	o+=properties
	o+='\t\t</Properties>\n'
	o+='\t\t<State>\n'
	o+='\t\t\t<Scalar type=\"uint32_t\" name=\"t\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"v\" length=\"'+str(N*N)+'\"/>\n'
	o+='\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"cN\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"cE\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"cS\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"cW\" length=\"'+str(N)+'\"/>\n'
	o+='\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"nN\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"nE\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"nS\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"nW\" length=\"'+str(N)+'\"/>\n'
	o+='\n'
	o+='\t\t\t<Array type=\"uint8_t\" name=\"c_arrivalFlags\" length=\"4\"/>\n'
	o+='\t\t\t<Array type=\"uint8_t\" name=\"n_arrivalFlags\" length=\"4\"/>\n'
	o+='\t\t\t<Array type=\"uint8_t\" name=\"sentFlags\" length=\"4\"/>\n'
	o+='\t\t\t<Scalar type=\"uint8_t\" name=\"needToSend\"/>\n'
	o+=state
	o+='\t\t</State>\n'
	o+='\n'
	o+='\t\t<ReadyToSend><![CDATA[\n'
	o+='\t\t\t*readyToSend=0;\n'
	o+='\t\t\tif(deviceState->needToSend) {\n'
	o+='\t\t\t\tif(!deviceState->sentFlags[north])\n'
	o+='\t\t\t\t\t*readyToSend = *readyToSend | RTS_FLAG_north_out;\n'
	o+='\t\t\t\tif(!deviceState->sentFlags[east])\n'
	o+='\t\t\t\t\t*readyToSend = *readyToSend | RTS_FLAG_east_out;\n'
	o+='\t\t\t\tif(!deviceState->sentFlags[south])\n'
	o+='\t\t\t\t\t*readyToSend = *readyToSend | RTS_FLAG_south_out;\n'
	o+='\t\t\t\tif(!deviceState->sentFlags[west])\n'
	o+='\t\t\t\t\t*readyToSend = *readyToSend | RTS_FLAG_west_out;\n'
	o+='\t\t\t} else if(deviceState->t >= graphProperties->maxTime) {\n'
	o+='\t\t\t\t*readyToSend = RTS_FLAG_finish;\n'
	o+='\t\t\t} else {\n'
	o+='\t\t\t\t*readyToSend=0;\n'
	o+='\t\t\t}\n'
	o+='\t\t]]></ReadyToSend>\n'
	o+='\n'
	o+='\t\t<OutputPin name=\"finish\" messageTypeId=\"finished\">\n'
	o+='\t\t\t<OnSend><![CDATA[\n'
	o+='\t\t\t]]></OnSend>\n'
	o+='\t\t</OutputPin>\n'
	o+='\n'
	o+='\t\t<InputPin name=\"__init__\" messageTypeId=\"__init__\">\n'
	o+='\t\t\t<OnReceive><![CDATA[\n'
	if debug:
		o+='\t\t\t\thandler_log(2, \"center device is being initialised\");\n'
	o+='\t\t\t\tdeviceState->t=0;\n'
	o+='\t\t\t\tdeviceState->needToSend=1;\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<deviceProperties->haloLength; i++) {\n'
	o+='\t\t\t\t\tdeviceState->cN[i]=0; deviceState->cE[i]=0; deviceState->cS[i]=0; deviceState->cW[i]=0;\n'
	o+='\t\t\t\t\tdeviceState->nN[i]=0; deviceState->nE[i]=0; deviceState->nS[i]=0; deviceState->nW[i]=0;\n'
	o+='\t\t\t\t\tdeviceState->n_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t\tdeviceState->c_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tcell_initFunc(deviceState, deviceProperties);\n'
	o+='\t\t\t]]></OnReceive>\n'
	o+='\t\t</InputPin>\n'
	o+='\n'

	#Input Pins for the four directions north, east, south, and west.
	directions=['north', 'east', 'south', 'west']
	for direction in directions:
		o+='\n'
		o+='\t\t<InputPin name=\"'+direction+'_in\" messageTypeId=\"update\">\n'
		o+='\t\t\t<OnReceive><![CDATA[\n'
		o+='\t\t\t\tloadMessage(message, deviceState, deviceProperties, graphProperties, '+direction+');\n'
		if debug:
			o+='\t\t\t\thandler_log(2, "\tcurrent arrivals: n:%d e:%d s:%d w%d", deviceState->c_arrivalFlags[north], deviceState->c_arrivalFlags[east],deviceState->c_arrivalFlags[south],deviceState->c_arrivalFlags[west]);\n'
		o+='\t\t\t\tif(message->t == deviceState->t) {\n'
		o+='\t\t\t\t\tif( attemptTimestepTransition(deviceState, deviceProperties, graphProperties)) {\n'
		if debug:
			o+='\t\t\t\t\t\thandler_log(2, "safely transitioned to the next timestep %d", deviceState->t);\n'
		o+='\t\t\t\t\t}\n'
		o+='\t\t\t\t}\n'
		o+='\t\t\t]]></OnReceive>\n'
		o+='\t\t</InputPin>\n'
		o+='\n'
	o+='\n'

	#Output pins for the four directions north, east, south, and west.
	for direction in directions:
		o+='\t\t<OutputPin name=\"'+direction+'_out\" messageTypeId=\"update\">\n'	
		o+='\t\t\t<OnSend><![CDATA[\n'
		if debug:
			o+='\t\t\t\thandler_log(2, "sending message");\n'
		o+='\t\t\t\tuint32_t hL = deviceProperties->haloLength;\n'
		o+='\t\t\t\tfor(uint32_t i=0; i < hL; i++) {\n'
		if direction =='north':
			o+='\t\t\t\t\tmessage->v[i] = deviceState->v[i];\n'
		elif direction == 'east':
			o+='\t\t\t\t\tmessage->v[i] = deviceState->v[((i+1)*hL)-1];\n'
		elif direction == 'south':
			o+='\t\t\t\t\tmessage->v[i] = deviceState->v[(hL * (hL - 1)) + i];\n'
		elif direction == 'west':
			o+='\t\t\t\t\tmessage->v[i] = deviceState->v[i*hL];\n'
		o+='\t\t\t\t}\n'
		o+='\t\t\t\tmessage->t = deviceState->t;\n'
		o+='\t\t\t\tdeviceState->sentFlags['+direction+'] = 1;\n'
		o+='\t\t\t\tif(allSent(deviceState, deviceProperties)) {\n'
		o+='\t\t\t\t\t\tdeviceState->needToSend = 0;\n'
		o+='\t\t\t\t\t\tif( attemptTimestepTransition(deviceState, deviceProperties, graphProperties) ) {\n'
		if debug:
			o+='\t\t\t\t\t\thandler_log(2, "safely transitioned to the next timestep %d", deviceState->t);\n'
		o+='\t\t\t\t\t\t}\n'
		o+='\t\t\t\t\t}\n'
		o+='\t\t\t]]></OnSend>\n'
		o+='\t\t</OutputPin>\n'
	o+='\t</DeviceType>\n'
	return o

def generate_simpleTestInstance(N, name):
	o='<GraphInstance id=\"test_'+str(N)+'by'+str(N)+'\" graphTypeId=\"'+name+'\">\n'
	o+='\t<DeviceInstances sorted=\"1\">\n'
	o+='\t\t<DevI id=\"center\" type=\"cell_'+str(N)+'by'+str(N)+'\" />\n'
	o+='\t\t<DevI id=\"nEdge\" type=\"boundary_'+str(N)+'\" />\n'
	o+='\t\t<DevI id=\"eEdge\" type=\"boundary_'+str(N)+'\" />\n'
	o+='\t\t<DevI id=\"sEdge\" type=\"boundary_'+str(N)+'\" />\n'
	o+='\t\t<DevI id=\"wEdge\" type=\"boundary_'+str(N)+'\" />\n'
	o+='\t\t<DevI id=\"exit_node_0\" type=\"exit_node\">\n'
	o+='\t\t\t<P>\"fanin\": 5</P>\n'
	o+='\t\t</DevI>\n'
	o+='\t</DeviceInstances>\n'
	o+='\t<EdgeInstances sorted=\"1\">\n'
	o+='\t\t<EdgeI path=\"center:north_in-nEdge:out\"/>\n'
	o+='\t\t<EdgeI path=\"center:east_in-eEdge:out\"/>\n'
	o+='\t\t<EdgeI path=\"center:south_in-sEdge:out\"/>\n'
	o+='\t\t<EdgeI path=\"center:west_in-wEdge:out\"/>\n'
	o+='\t\t<EdgeI path=\"nEdge:in-center:north_out\"/>\n'
	o+='\t\t<EdgeI path=\"eEdge:in-center:east_out\"/>\n'
	o+='\t\t<EdgeI path=\"sEdge:in-center:south_out\"/>\n'
	o+='\t\t<EdgeI path=\"wEdge:in-center:west_out\"/>\n'
	o+='\t\t<EdgeI path=\"exit_node_0:done-center:finish\"/>\n'
	o+='\t\t<EdgeI path=\"exit_node_0:done-nEdge:finish\"/>\n'
	o+='\t\t<EdgeI path=\"exit_node_0:done-eEdge:finish\"/>\n'
	o+='\t\t<EdgeI path=\"exit_node_0:done-sEdge:finish\"/>\n'
	o+='\t\t<EdgeI path=\"exit_node_0:done-wEdge:finish\"/>\n'
	o+='\t</EdgeInstances>\n'
	o+='</GraphInstance>\n'
	return o



def generate_messages(N):
	o= '\n'
	o+= '\t<MessageTypes>\n' 
	o+= '\t\t<MessageType id=\"__init__\">\n'
	o+= '\t\t</MessageType>\n'
	o+= '\n'
	o+= '\t\t<MessageType id=\"finished\">\n'
	o+= '\t\t</MessageType>\n'
	o+= '\n'
	o+= '\t\t<MessageType id=\"update\">\n'
	o+= '\t\t\t<Message>\n'
	o+= '\t\t\t\t<Scalar type=\"uint32_t\" name=\"t\" />\n'
	o+= '\t\t\t\t<Array type=\"uint32_t\" name=\"v\" length=\"'+str(N)+'\" />\n'
	o+= '\t\t\t</Message>\n'
	o+= '\t\t</MessageType>\n'
	o+= '\t</MessageTypes>\n'
	return o 
	

def generate_exitNode(debug):
	o='\n'
	o+='\t<DeviceType id=\"exit_node\">\n'
	o+='\t\t<Properties>\n'
	o+='\t\t\t<Scalar name=\"fanin\" type=\"uint32_t\" />\n'
	o+='\t\t</Properties>\n'
	o+='\n'

	o+='\t\t<State>\n'
	o+='\t\t\t<Scalar name=\"done\" type=\"uint32_t\" />\n'
	o+='\t\t</State>\n'
	o+='\n'

	o+='\t\t<ReadyToSend><![CDATA[\n'
	o+='\t\t\t*readyToSend=0;\n'
	o+='\t\t]]></ReadyToSend>\n'	
	o+='\n'
	
	o+='\t\t<InputPin name=\"done\" messageTypeId=\"finished\">\n'
	o+='\t\t\t<OnReceive><![CDATA[\n'
	o+='\t\t\t\tdeviceState->done++;\n'
	if debug:
		o+='\t\t\t\thandler_log(2, \"done=0x%x, fanin=0x%x\", deviceState->done, deviceProperties->fanin);\n'
	o+='\t\t\t\tif(deviceState->done == deviceProperties->fanin){\n'
	o+='\t\t\t\t\thandler_exit(0);\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t]]></OnReceive>\n'
	o+='\t\t</InputPin>\n'
		
	o+='\t</DeviceType>\n'
	return o

def generate_fixpt_sharedcode():
	tabs = '\t\t'
	o=  tabs+ '\n'   
	o+= tabs+ 'int32_t fix_mul(int32_t x, int32_t y)\n'
	o+= tabs+ '{\n'
	o+= tabs+ '  auto xy=x*(int64_t)y;\n'
	o+= tabs+ '  xy += (1<<23);\n'
	o+= tabs+ '  xy = xy>>24;\n'
	o+= tabs+ '  assert( -2147483648 <= xy && xy <= 2147483647 );\n'
	o+= tabs+ '  return (int32_t)(xy);\n'
	o+= tabs+ '}\n'
	o+= tabs+ '\n'
	o+= tabs+ 'int32_t fix_add(int32_t x, int32_t y)\n'
	o+= tabs+ '{\n'
	o+= tabs+ '  int64_t xy64=x+(int64_t)y;\n'
	o+= tabs+ '  assert( -2147483648 <= xy64 && xy64 <= 2147483647 );\n'
	o+= tabs+ '  int32_t xy=x+y;\n'
	o+= tabs+ '  return xy;\n'
	o+= tabs+ '}\n'
	o+= tabs+ '\n'
	o+= tabs+ 'int32_t fix_sub(int32_t x, int32_t y)\n'
	o+= tabs+ '{\n'
	o+= tabs+ '  int64_t xy64=x-(int64_t)y;\n'
	o+= tabs+ '  assert( -2147483648 <= xy64 && xy64 <= 2147483647 );\n'
	o+= tabs+ '  int32_t xy=x-y;\n'
	o+= tabs+ '  return xy;\n'
	o+= tabs+ '}\n'
	o+= tabs+ '\n'
	o+= tabs+ 'bool fix_gt(int32_t x, int32_t y)\n'
	o+= tabs+ '{\n'
	o+= tabs+ '  return x>y;\n'
	o+= tabs+ '}\n'
	o+= tabs+ '\n'
	o+= tabs+ 'bool fix_lt(int32_t x, int32_t y)\n'
	o+= tabs+ '{\n'
	o+= tabs+ '  return x>y;\n'
	o+= tabs+ '}\n'
	return o

def generate_NbyN_sharedcode(graphname, N, initFunc, updateFunc, debug):
	o ='\n'
	o+='\t\t\tuint32_t cell_updateFunc('+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state, const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_properties_t *properties) {\n'
	o+='\t\t\t\t //Process the timestep for this device\n'
	o+= updateFunc #inject the code for computing each timestep
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tuint32_t cell_initFunc('+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state, const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_properties_t *properties) {\n'
	o+='\t\t\t\t //the initial state for this device\n'
	o+= initFunc #inject the code for computing each timestep
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tbool allArrived(const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state, const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_properties_t *gp) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<4; i++) {\n'
	o+='\t\t\t\t\tif (!state->c_arrivalFlags[i])\n'
	o+='\t\t\t\t\t\treturn false;\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\treturn true;\n'
	o+='\t\t\t}\n'
	o+='\t\t\tvoid clearArrived('+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<4; i++) {\n'
	o+='\t\t\t\t\t\tstate->c_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\treturn;\n'
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tbool allSent(const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state, const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_properties_t *gp) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<4; i++) {\n'
	o+='\t\t\t\t\tif(!state->sentFlags[i])\n'
	o+='\t\t\t\t\t\treturn false;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\treturn true;\n'
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tvoid clearSentFlags('+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<4; i++) {\n'
	o+='\t\t\t\t\tstate->sentFlags[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\treturn;\n'
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tvoid moveToNextIter('+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state, const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_properties_t *dp, const '+graphname+'_properties_t *gp) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i< 4; i++) {\n'
	o+='\t\t\t\t\tstate->c_arrivalFlags[i] = state->n_arrivalFlags[i];\n'
	o+='\t\t\t\t\tstate->n_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t\tstate->sentFlags[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tconst uint32_t hL = dp->haloLength;\n'
	o+='\t\t\t\tfor(uint32_t i=0; i< hL; i++) {\n'
	o+='\t\t\t\t\tstate->cN[i] = state->nN[i];\n'
	o+='\t\t\t\t\tstate->cE[i] = state->nE[i];\n'
	o+='\t\t\t\t\tstate->cS[i] = state->nS[i];\n'
	o+='\t\t\t\t\tstate->cW[i] = state->nW[i];\n'
	o+='\t\t\t}\n'
	o+='\t\t\tstate->t = state->t + gp->dt;\n'
	o+='\t\t\treturn;\n'
	o+='\t\t}\n'
	o+='\n'
	o+='\t\t\tvoid loadMessage(const '+graphname+'_update_message_t *message, '+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state, const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_properties_t *dp, const '+graphname+'_properties_t *gp, orientation_t dir) {\n'
	o+='\t\t\t\tassert( message->t == state->t || message->t == state->t + gp->dt );\n'
	o+='\t\t\t\tconst uint32_t hL = dp->haloLength;\n'
	o+='\t\t\t\tif(message->t == state->t) {\n'
	o+='\t\t\t\t\tfor(uint32_t i=0; i<hL; i++) {\n'
	o+='\t\t\t\t\t\tif(dir == north)\n'
	o+='\t\t\t\t\t\t\tstate->cN[i] = message->v[i];\n'
	o+='\t\t\t\t\t\tif(dir == east)\n'
	o+='\t\t\t\t\t\t\tstate->cE[i] = message->v[i];\n'
	o+='\t\t\t\t\t\tif(dir == south)\n'
	o+='\t\t\t\t\t\t\tstate->cS[i] = message->v[i];\n'
	o+='\t\t\t\t\t\tif(dir == west)\n'
	o+='\t\t\t\t\t\t\tstate->cW[i] = message->v[i];\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\t\tstate->c_arrivalFlags[dir] = 1;\n'
	o+='\t\t\t\t}else if(message->t == state->t + gp->dt) {\n'
	o+='\t\t\t\t\tfor(uint32_t i=0; i<hL; i++) {\n'
	o+='\t\t\t\t\t\tif(dir == north)\n'
	o+='\t\t\t\t\t\tstate->nN[i] = message->v[i];\n'
	o+='\t\t\t\t\t\tif(dir == east)\n'
	o+='\t\t\t\t\t\t\tstate->nE[i] = message->v[i];\n'
	o+='\t\t\t\t\t\tif(dir == south)\n'
	o+='\t\t\t\t\t\t\tstate->nS[i] = message->v[i];\n'
	o+='\t\t\t\t\t\tif(dir == west)\n'
	o+='\t\t\t\t\t\t\tstate->nW[i] = message->v[i];\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\t\tstate->n_arrivalFlags[dir] = 1;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t}\n'
	o+='\n'
	o+='//It is only safe to transition to the next timestep if all the messages from the current one have\n'
	o+='//been dispatched and the complete halo for the current timestep have been received.\n'
	o+='bool attemptTimestepTransition('+graphname+'_cell_'+str(N)+'by'+str(N)+'_state_t *state, const '+graphname+'_cell_'+str(N)+'by'+str(N)+'_properties_t *dp, const '+graphname+'_properties_t *gp) {\n'
	o+='  if(allArrived(state, dp) && !state->needToSend) {\n'
	o+='    clearArrived(state);\n'
	o+='    clearSentFlags(state);\n'      
	o+='    cell_updateFunc(state, dp);\n'
	o+='    moveToNextIter(state, dp, gp);\n'
	o+='    state->needToSend = 1;\n'
	o+='    return 1; //true for success\n'
	o+='  } else { \n' 
	o+='    return 0; //false for failure\n'
	o+='  }\n'
	o+='}\n'
	o+='\n'

	return o

def generate_boundary_sharedcode(graphname, N, initFunc, updateFunc, debug):
	o='\n'
	o+='\t\t\tuint32_t boundary_updateFunc('+graphname+'_boundary_'+str(N)+'_state_t *state, const '+graphname+'_boundary_'+str(N)+'_properties_t *properties) {\n'
	o+='\t\t\t\t//Process the timestep for this device\n'
	o+= updateFunc #Inject the input code for how the timestep should be computed
	o+='\t\t\t}\n\n'
	o+='\t\t\tuint32_t boundary_initFunc('+graphname+'_boundary_'+str(N)+'_state_t *state, const '+graphname+'_boundary_'+str(N)+'_properties_t *properties) {\n'
	o+='\t\t\t\t//used to initialise the state of the device\n'
	o+= initFunc #Inject the input code for how the timestep should be computed
	o+='\t\t\t}\n'
	o+='\n'
	return o

def generate_sharedcode(graphname, devicelist, debug):
	tabs = '\t\t'
	o = tabs+ '\n'
	o+= tabs+ '<SharedCode><![CDATA[\n'
	o+= tabs+ '\tenum orientation_t { north, east, south, west };\n'
	o+= generate_fixpt_sharedcode()
	for d in devicelist:
		if d.celltype == 'box':
			o+= generate_NbyN_sharedcode(graphname, d.size, d.initFunc, d.updateFunc, debug)
		if d.celltype == 'edge':
			o+= generate_boundary_sharedcode(graphname, d.size, d.initFunc, d.updateFunc, debug)	
	o+= tabs+ ']]></SharedCode>\n\n'
	return o

