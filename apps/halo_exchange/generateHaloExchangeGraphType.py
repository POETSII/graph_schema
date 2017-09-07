import sys, getopt
import re


def main(argv):
	outFileName=''
	inFileName=''
	try:
		opts, args = getopt.getopt(argv, "hi:o", ["input=","output="])
	except getopt.GetoptError:
		print 'Usage python generateHaloExchangeGraphType.py --input description --output graph_type.xml'
		sys.exit()
	for opt, arg in opts:
		if opt == '-h':
			print 'Usage python generateHaloExchangeGraphType.py --input description --output graph_type.xml'
			sys.exit()
		if opt in ("-i", "--input"):
			inFileName = arg
		if opt in ("-o", "--output"):
			outFileName = arg	
	assert(outFileName != '')	
	assert(inFileName != '')

	N = 9;
	debug = True;

	xmlo = '<?xml version=\"1.0\"?>\n' 
	xmlo+= '<Graphs xmlns=\"https://poets-project.org/schemas/virtual-graph-schema-v2\">\n'
	xmlo+= '\t<GraphType id=\"halo_exchange\">\n'
	
	xmlo+= '\t<Properties>\n'
	xmlo+= '\t\t<Scalar type=\"uint32_t\" name=\"maxTime\" default=\"65\" />\n'
	xmlo+= '\t\t<Scalar type=\"uint32_t\" name=\"nodesPerDevice\" default=\"'+str(N*N)+'\" />\n'
	xmlo+= '\t\t<Scalar type=\"uint32_t\" name=\"haloLength\" default=\"'+str(N)+'\" />\n'
	xmlo+= '\t\t<Scalar type=\"uint32_t\" name=\"dt\" default=\"1\" />\n'
	xmlo+= '\t</Properties>\n'
	xmlo+= '\n'

	xmlo+=generate_messages(N)

	xmlo+= '\n'
	xmlo+= '\t<DeviceTypes>\n'

	tabs = '\t\t\t\t'

	# Example of a 4x4 FE agglomerated node
	#	hL is the length of the halo exchange (i.e. the length of the grid)
	#
	# 				N(0)		N(1)		N(2)		N(3) 
	# W(0)		0				1				2				3		E(0)	
	# W(1)		4				5				6				7		E(1)
	# W(2)		8				9				10			11	E(2)
	# W(3)		12			13			14			15	E(3)
	#					S(0)		S(1)		S(2)		S(3)

	# if ( i < hL ) we are at a north facing edge 
  # if ( i % (hL-1) == 0 ) we are at an east facing edge 
	# if ( i >= (hL*hL -1) - (hL - 1) ) we are at a south facing edge 
	# if ( i % hL == 0 ) we are at a west facing edge 

	c_timestep =  tabs + ' uint32_t hL = gp->haloLength;\n'
	c_timestep += tabs + ' uint32_t tmpV[hL*hL] = { 0 };\n'
	c_timestep += tabs + ' for(uint32_t i=0; i<hL*hL; i++) {\n'
	c_timestep += tabs + ' 	//Accumulate all points surrounding the current point\n'
	c_timestep += tabs + ' \n'
	c_timestep += tabs + ' 	//North of point\n'
	c_timestep += tabs + ' 	if(i<hL) //We are at the northern face of this node and need to use the halo value	\n'
	c_timestep += tabs + ' 		tmpV[i] += state->cN[i];\n'
	c_timestep += tabs + ' 	else\n'
	c_timestep += tabs + ' 		tmpV[i] += state->v[i-hL];\n'
	c_timestep += tabs + ' \n'
	c_timestep += tabs + ' 	//East of point\n'
	c_timestep += tabs + ' 	if((i!=0) && (i % hL == (hL-1))) //We are at the eastern face of this node and need to use the halo value\n'
	c_timestep += tabs + ' 		tmpV[i] += state->cE[i/(hL-1)]; \n'
	c_timestep += tabs + ' 	else\n'
	c_timestep += tabs + ' 		tmpV[i] += state->v[i+1];\n'
	c_timestep += tabs + ' \n'
	c_timestep += tabs + ' 	//South of point\n'
	c_timestep += tabs + ' 	if(i >= (hL*hL -1) - (hL-1)) //We are at the southern face of this node and need to use the halo value\n'
	c_timestep += tabs + ' 		tmpV[i] += state->cS[i - ((hL*hL -1) - (hL-1))]; \n'
	c_timestep += tabs + ' 	else\n'
	c_timestep += tabs + ' 		tmpV[i] += state->v[i+hL];\n'
	c_timestep += tabs + ' \n'
	c_timestep += tabs + ' 	//West of point\n'
	c_timestep += tabs + ' 	if(i % hL == 0) //We are at the western face of this node and need to use the halo value\n'
	c_timestep += tabs + ' 		tmpV[i] += state->cW[i/hL];\n'
	c_timestep += tabs + ' 	else\n'
	c_timestep += tabs + ' 		tmpV[i] += state->v[i-1];\n'
	c_timestep += tabs + ' }\n'
	c_timestep += tabs + ' \n'
	c_timestep += tabs + ' for(uint32_t i=0; i<hL*hL; i++) {\n'
	c_timestep += tabs + ' 	state->v[i] = tmpV[i];\n'
	c_timestep += tabs + ' }\n'
	c_timestep += tabs + ' for(uint32_t i=0; i<hL; i++){' 
	c_timestep += tabs + '     handler_log(2 ,"%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u", tmpV[(i*hL)+0],tmpV[(i*hL)+1],tmpV[(i*hL)+2],tmpV[(i*hL)+3],tmpV[(i*hL)+4],tmpV[(i*hL)+5],tmpV[(i*hL)+6],tmpV[(i*hL)+7],tmpV[(i*hL)+8]);\n' 
	c_timestep += tabs + ' }\n' 
	c_timestep += tabs + ' return 0;\n'

	
	# Boundary update function body, example boundary_4
	#
	# 0	- cV[0]
	#	1 - cV[1]
	#	2 - cV[2]
	#	3 - cV[3]
	#
	# Currently it ignores it's own boundary and does nothing special, just accumulates it's neighbours together where it can
	# Boundary only has N, E, S -- the west edge is assumed to be the boundary. In reality this might be rotated and not truely be the western face..

	b_timestep =  tabs + '\n'
	b_timestep += tabs + 'uint32_t hL = gp->haloLength;\n'
	b_timestep += tabs + 'uint32_t tmpV[hL];	\n'
	b_timestep += tabs + '\n'
	b_timestep += tabs + '//c = NorthOf(c) + EastOf(c) + SouthOf(c) \n'
	b_timestep += tabs + '//Northmost element \n'
	b_timestep += tabs + 'tmpV[0] = 0 + state->cV[0] + state->v[1]; \n'
	b_timestep += tabs + '\n'
	b_timestep += tabs + '//Middle Elements \n'
	b_timestep += tabs + 'for(uint32_t i=1; i<hL-1; i++) { \n'
	b_timestep += tabs + '\ttmpV[i] = state->v[i-1] + state->cV[i] + state->v[i+1]; \n'
	b_timestep += tabs + '} \n'
	b_timestep += tabs + '\n'
	b_timestep += tabs + '//Southmost element \n'
	b_timestep += tabs + 'tmpV[hL-1] = state->v[hL-2] + state->cV[hL-1] + 0; \n'
	b_timestep += tabs + '\n'
	b_timestep += tabs + '//Load tmp value array into the real value array \n'
	b_timestep += tabs + 'for(uint32_t i=0; i<hL; i++) { \n'
	b_timestep += tabs + '\tstate->v[i] = tmpV[i]; \n'
	b_timestep += tabs + '} \n'
	b_timestep += tabs + 'return 1;\n'

	xmlo+= generate_NbyN_Cluster(N, debug, c_timestep)
	xmlo+= generate_lengthN_boundary(N, debug, b_timestep)
	xmlo+= generate_exitNode(debug)

	xmlo+= '\t</DeviceTypes>\n'
	xmlo+= '</GraphType>\n'	
	xmlo+= generate_simpleTestInstance(N)
	xmlo+= '</Graphs>\n'	

	outfile = open(outFileName,'w')
	outfile.write(xmlo)

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

def generate_lengthN_boundary(N,debug, computeTimestep):
	o='\n'
	o+='\t<DeviceType id=\"boundary_'+str(N)+'\">\n'
	o+='\t\t<State>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"cV\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"nV\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Array type=\"uint32_t\" name=\"v\" length=\"'+str(N)+'\"/>\n'
	o+='\t\t\t<Scalar type=\"uint32_t\" name=\"t\"/>\n'
	o+='\t\t\t<Scalar type=\"int8_t\" name=\"cAvail\"/>\n'
	o+='\t\t\t<Scalar type=\"int8_t\" name=\"nAvail\"/>\n'
	o+='\t\t</State>\n'
	o+='\n'
	
	o+='\t\t<ReadyToSend><![CDATA[\n'
	o+='\t\t\t*readyToSend=0;\n'
	o+='\t\t\tif(deviceState->t == 0) {\n'
	o+='\t\t\t\t*readyToSend = RTS_FLAG_out;\n'
	o+='\t\t\t} else if (deviceState->cAvail) {\n'	
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
	o+='\t\t\t\tfor(uint32_t i=0; i<graphProperties->haloLength; i++){\n'
	o+='\t\t\t\t\tdeviceState->cV[i] = 0;\n'
	o+='\t\t\t\t\tdeviceState->nV[i] = 0;\n'
	o+='\t\t\t\t\tdeviceState->v[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t]]></OnReceive>\n'
	o+='\t\t</InputPin>\n'
	o+='\n'

	o+='\t\t<SharedCode><![CDATA[\n'
	o+='\t\t\tenum orientation_t { north, east, south, west };\n'
	o+='\t\t\tuint32_t computeTimestep(boundary_9_state_t *state, const halo_exchange_properties_t *gp, OrchestratorServices *orchestrator) {\n'
	o+='\t\t\t\t//Process the timestep for this device\n'
	o+='\t\t\t\tHandlerLogImpl handler_log(orchestrator);\n'
	o+= computeTimestep #Inject the input code for how the timestep should be computed
	o+='\t\t\t}\n'
	o+='\t\t]]></SharedCode>\n'
	o+='\n'

	o+='\t\t<OutputPin name=\"out\" messageTypeId=\"update\">\n'	
	o+='\t\t\t<OnSend><![CDATA[\n'
	o+='\t\t\t\tuint32_t hL = graphProperties->haloLength;\n'
	o+='\t\t\t\tfor(uint32_t i=0; i< hL; i++) {\n'
	o+='\t\t\t\t\tmessage->v[i] = deviceState->v[i];\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tif(deviceState->cAvail) {\n'
	o+='\t\t\t\t\tdeviceState->t = deviceState->t + graphProperties->dt;\n'
	o+='\t\t\t\t\tdeviceState->cAvail = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tmessage->t = deviceState->t;\n'
	if debug:
		o+='\t\t\t\thandler_log(2, \"boundary is sending a message.  message->t=%d\", message->t);\n'
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
	o+='\t\t\t\tuint32_t hL = graphProperties->haloLength;\n'
	o+='\t\t\t\tif(message->t == deviceState->t) {\n'
	o+='\t\t\t\t\tdeviceState->cAvail = 1;\n'
	o+='\t\t\t\t\tcomputeTimestep(deviceState, graphProperties, orchestrator);\n'
	o+='\t\t\t\t} else if (message->t == deviceState->t+hL) {\n'
	o+='\t\t\t\t\tdeviceState->nAvail = 1;\n'
	o+='\t\t\t\t\tfor(uint32_t i=0; i<hL; i++) {\n'
	o+='\t\t\t\t\t\tdeviceState->nV[i] = message->v[i];\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t]]></OnReceive>\n'
	o+='\t\t</InputPin>\n'

	o+='\t</DeviceType>\n'
	return o 

def generate_NbyN_Cluster(N, debug, computeTimestep): 
#returns a string containing xml code describing a halo exchange N x N device
	o='\n'
	o+='\t<DeviceType id=\"cell_' + str(N) +'by'+str(N)+'\">\n' 	
	o+='\t\t<Properties>\n'
	o+='\t\t\t<Scalar type=\"int32_t\" name=\"updateDelta\" />\n' 
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
	o+='\t\t\t<Scalar type=\"uint8_t\" name=\"processed\"/>\n'
	o+='\t\t</State>\n'
	o+='\n'
	o+='\t\t<ReadyToSend><![CDATA[\n'
	o+='\t\t\t*readyToSend=0;\n'
	o+='\t\t\tif(deviceState->processed) {\n'
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
	o+='\t\t\t\tdeviceState->processed=0;\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<graphProperties->nodesPerDevice; i++) {\n'
	o+='\t\t\t\t\tdeviceState->v[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<graphProperties->haloLength; i++) {\n'
	o+='\t\t\t\t\tdeviceState->cN[i]=0; deviceState->cE[i]=0; deviceState->cS[i]=0; deviceState->cW[i]=0;\n'
	o+='\t\t\t\t\tdeviceState->nN[i]=0; deviceState->nE[i]=0; deviceState->nS[i]=0; deviceState->nW[i]=0;\n'
	o+='\t\t\t\t\tdeviceState->n_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t\tdeviceState->c_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t]]></OnReceive>\n'
	o+='\t\t</InputPin>\n'
	o+='\n'
	o+='\t\t<SharedCode><![CDATA[\n'
	o+='\t\t\tenum orientation_t { north, east, south, west };\n'
	o+='\n'
	o+='\t\t\tuint32_t computeTimestep(cell_'+str(N)+'by'+str(N)+'_state_t *state, const halo_exchange_properties_t *gp, OrchestratorServices *orchestrator) {\n'
	o+='\t\t\t\t //Process the timestep for this device\n'
	o+='\t\t\t\t HandlerLogImpl handler_log(orchestrator);\n\n'
	o+= computeTimestep #inject the code for computing each timestep
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tbool allArrived(const cell_'+str(N)+'by'+str(N)+'_state_t *state, const halo_exchange_properties_t *gp) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<4; i++) {\n'
	o+='\t\t\t\t\tif (!state->c_arrivalFlags[i])\n'
	o+='\t\t\t\t\t\treturn false;\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\treturn true;\n'
	o+='\t\t\t}\n'
	o+='\t\t\tvoid clearArrived(cell_'+str(N)+'by'+str(N)+'_state_t *state) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<4; i++) {\n'
	o+='\t\t\t\t\t\tstate->c_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t\t}\n'
	o+='\t\t\t\treturn;\n'
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tbool allSent(const cell_'+str(N)+'by'+str(N)+'_state_t *state, const halo_exchange_properties_t *gp) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i<4; i++) {\n'
	o+='\t\t\t\t\tif(!state->sentFlags[i])\n'
	o+='\t\t\t\t\t\treturn false;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\treturn true;\n'
	o+='\t\t\t}\n'
	o+='\n'
	o+='\t\t\tvoid moveToNextIter(cell_'+str(N)+'by'+str(N)+'_state_t *state, const halo_exchange_properties_t *gp) {\n'
	o+='\t\t\t\tfor(uint32_t i=0; i< 4; i++) {\n'
	o+='\t\t\t\t\tstate->c_arrivalFlags[i] = state->n_arrivalFlags[i];\n'
	o+='\t\t\t\t\tstate->n_arrivalFlags[i] = 0;\n'
	o+='\t\t\t\t\tstate->sentFlags[i] = 0;\n'
	o+='\t\t\t\t}\n'
	o+='\t\t\t\tconst uint32_t hL = gp->haloLength;\n'
	o+='\t\t\t\tfor(uint32_t i=0; i< hL; i++) {\n'
	o+='\t\t\t\t\tstate->cN[i] = state->nN[i];\n'
	o+='\t\t\t\t\tstate->cE[i] = state->nE[i];\n'
	o+='\t\t\t\t\tstate->cS[i] = state->nS[i];\n'
	o+='\t\t\t\t\tstate->cW[i] = state->nW[i];\n'
	o+='\t\t\t}\n'
	o+='\t\t\tstate->t = state->t + gp->dt;\n'
	o+='\t\t\tstate->processed = 0;\n'
	o+='\t\t\treturn;\n'
	o+='\t\t}\n'
	o+='\n'
	o+='\t\t\tvoid loadMessage(const update_message_t *message, cell_'+str(N)+'by'+str(N)+'_state_t *state, const halo_exchange_properties_t *gp, orientation_t dir) {\n'
	o+='\t\t\t\tassert( message->t == state->t || message->t == state->t + gp->dt );\n'
	o+='\t\t\t\tconst uint32_t hL = gp->haloLength;\n'
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
	o+='\t\t]]></SharedCode>\n'

	#Input Pins for the four directions north, east, south, and west.
	directions=['north', 'east', 'south', 'west']
	for direction in directions:
		o+='\n'
		o+='\t\t<InputPin name=\"'+direction+'_in\" messageTypeId=\"update\">\n'
		o+='\t\t\t<OnReceive><![CDATA[\n'
		o+='\t\t\t\tloadMessage(message, deviceState, graphProperties, '+direction+');\n'
		if debug:
			o+='\t\t\t\thandler_log(2, "\tcurrent arrivals: n:%d e:%d s:%d w%d", deviceState->c_arrivalFlags[north], deviceState->c_arrivalFlags[east],deviceState->c_arrivalFlags[south],deviceState->c_arrivalFlags[west]);\n'
		o+='\t\t\t\tif(message->t == deviceState->t) {\n'
		o+='\t\t\t\t\tif(allArrived(deviceState, graphProperties)) {\n'
		o+='\t\t\t\t\t\tdeviceState->processed=1;;\n'
		o+='\t\t\t\t\t\tclearArrived(deviceState);\n'
		o+='\t\t\t\t\t\tcomputeTimestep(deviceState, graphProperties, orchestrator);\n'
		if debug:
			o+='\t\t\t\t\t\thandler_log(2, "data-transfer complete computing timestep.");\n'
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
		o+='\t\t\t\tuint32_t hL = graphProperties->haloLength;\n'
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
		o+='\t\t\t\tif(allSent(deviceState, graphProperties)) {\n'
		o+='\t\t\t\t\tmoveToNextIter(deviceState, graphProperties);\n'
		if debug:
			o+='\t\t\t\t\thandler_log(2, "moved to next timestep device->t=%d", deviceState->t);\n'		
		o+='\t\t\t\t}\n'
		o+='\t\t\t]]></OnSend>\n'
		o+='\t\t</OutputPin>\n'
	o+='\t</DeviceType>\n'
	return o

def generate_simpleTestInstance(N):
	o='<GraphInstance id=\"test_'+str(N)+'by'+str(N)+'\" graphTypeId=\"halo_exchange\">\n'
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

if __name__== "__main__":
	main(sys.argv[1:])
