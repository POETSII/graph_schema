import sys, getopt, math
import re
import haloExchange as hE


# Instantiate a graph containing MxM devices each which compute NxN elements
def generate_graphInstance(M, N, graphname):
	o = '<GraphInstance id=\"test_'+str(M)+'by'+str(M)+'_of_'+str(N)+'by'+str(N)+'\" graphTypeId=\"'+graphname+'\">\n'
	o+='\t<DeviceInstances sorted=\"1\">\n'

	#-------------------- Devices -------------------------
	
	#boundary devices: nBnode - north boundary node, eBnode - east boundary node, sBnode - south boundary node, wBnode - west... 
	for i in range(M):
		o+='\t\t<DevI id=\"nBnode_'+str(i)+'\" type=\"boundary_'+str(N)+'\" />\n'		
		o+='\t\t<DevI id=\"eBnode_'+str(i)+'\" type=\"boundary_'+str(N)+'\" />\n'		
		o+='\t\t<DevI id=\"sBnode_'+str(i)+'\" type=\"boundary_'+str(N)+'\" />\n'		
		o+='\t\t<DevI id=\"wBnode_'+str(i)+'\" type=\"boundary_'+str(N)+'\" />\n'		

	#center cell devices (cnodes)
	for i in range(M):
		for j in range(M):
			o+='\t\t<DevI id=\"cnode_'+str(i)+'_'+str(j)+'\" type=\"cell_'+str(N)+'by'+str(N)+'\" />\n'

	o+='\t\t<DevI id=\"exit_node_0\" type=\"exit_node\">\n'	
	o+='\t\t\t<P>\"fanin\": '+str(M*M + 4*M)+'</P>\n'
	o+='\t\t</DevI>\n'
	o+='\t</DeviceInstances>\n'

	#------------------------------------------------------

	
	#-------------------- Edges -------------------------
	o+='\t<EdgeInstances sorted=\"1\">\n'
	
	for i in range(M):
		for j in range(M):
			#North face
			if(j==0):
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':north_in-nBnode_'+str(i)+':out\"/>\n'
			else:
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':north_in-cnode_'+str(i)+'_'+str(j-1)+':south_out\"/>\n'

			#East face
			if(i==(M-1)):
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':east_in-eBnode_'+str(j)+':out\"/>\n'
			else:
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':east_in-cnode_'+str(i+1)+'_'+str(j)+':west_out\"/>\n'

			#South face
			if(j==(M-1)):
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':south_in-sBnode_'+str(i)+':out\"/>\n'
			else:
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':south_in-cnode_'+str(i)+'_'+str(j+1)+':north_out\"/>\n'

			#West face
			if(i==0):
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':west_in-wBnode_'+str(j)+':out\"/>\n'
			else:
				o+='\t\t<EdgeI path=\"cnode_'+str(i)+'_'+str(j)+':west_in-cnode_'+str(i-1)+'_'+str(j)+':east_out\"/>\n'
			
			#connecting to the exit_node
			o+='\t\t<EdgeI path=\"exit_node_0:done-cnode_'+str(i)+'_'+str(j)+':finish\"/>\n'

	#connecting the boundary inputs
	for i in range(M):
		o+='\t\t<EdgeI path=\"nBnode_'+str(i)+':in-cnode_'+str(i)+'_0:north_out\"/>\n'
		o+='\t\t<EdgeI path=\"eBnode_'+str(i)+':in-cnode_'+str(M-1)+'_'+str(i)+':east_out\"/>\n'
		o+='\t\t<EdgeI path=\"sBnode_'+str(i)+':in-cnode_'+str(i)+'_'+str(M-1)+':south_out\"/>\n'
		o+='\t\t<EdgeI path=\"wBnode_'+str(i)+':in-cnode_0_'+str(i)+':west_out\"/>\n'

	#connecting the boundaries to the exit node
	for i in range(M):
		o+='\t\t<EdgeI path=\"exit_node_0:done-nBnode_'+str(i)+':finish\"/>\n'
		o+='\t\t<EdgeI path=\"exit_node_0:done-eBnode_'+str(i)+':finish\"/>\n'
		o+='\t\t<EdgeI path=\"exit_node_0:done-sBnode_'+str(i)+':finish\"/>\n'
		o+='\t\t<EdgeI path=\"exit_node_0:done-wBnode_'+str(i)+':finish\"/>\n'
		

	o+='\t</EdgeInstances>\n'
	#------------------------------------------------------

	o+='</GraphInstance>\n'
	return o

def main(argv):
	outFileName=''
	agglomeration_factor=4
	problemsize=800
	try:
		opts, args = getopt.getopt(argv, "ha:p:o", ["agglomeration=", "problemsize=","output="])
	except getopt.GetoptError:
		print 'Usage python generate_halo_exchange_scaling_graph.py --agglomeration <N> --problemsize P --output graph_type.xml'
		sys.exit()
	for opt, arg in opts:
		if opt == '-h':
			print 'Usage python generate_halo_exchange_scaling_graph.py --agglomeration <N> --problemsize P --output graph_type.xml'
			sys.exit()
		if opt in ("-a", "--agglomeration"):
			agglomeration_factor = int(arg)	
		if opt in ("-p", "--problemsize"):
			problemsize = int(arg)	
		if opt in ("-o", "--output"):
			outFileName = arg	
	assert(outFileName != '')	

	N = agglomeration_factor;
	P = problemsize;
	assert N <= 11, "Currently agglomeration factors greater than 11 are not supported."
	
	# primary cells - these are the 2D agglomerated square cells that make up the bulk of the grid
	#                   they have size NxN, 
	# secondary cells - these are the rectangular cells that make up the remaining space
	#                   they have size N x (N % P) 
	
	primaryCellsDim = int(math.floor(P/N))
	secondaryCellsDim = int(P % N)

	print 'creating a graph of '+str(primaryCellsDim)+'x'+str(primaryCellsDim)+' devices each computing '+str(N)+'x'+str(N)+' points'	

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

	center_update = """ 
		uint32_t hL = properties->haloLength;
	  uint32_t tmpV[hL*hL] = { 0 };
	 	for(uint32_t i=0; i<hL*hL; i++){ 
	 	//Accumulate all points surrounding the current point

	 	//North of point
	 	if(i<hL) //We are at the northern face of this node and need to use the halo value	
	 		tmpV[i] += state->cN[i];
	 	else
	 		tmpV[i] += state->v[i-hL];

	 	//East of point
	 	if((i!=0) && (i % hL == (hL-1))) //We are at the eastern face of this node and need to use the halo value
	 		tmpV[i] += state->cE[i/(hL-1)]; 
	 	else
	 		tmpV[i] += state->v[i+1];

	 	//South of point
	 	if(i >= (hL*hL -1) - (hL-1)) //We are at the southern face of this node and need to use the halo value
	 		tmpV[i] += state->cS[i - ((hL*hL -1) - (hL-1))]; 
	 	else
	 		tmpV[i] += state->v[i+hL];
	 
	 	//West of point
	 	if(i % hL == 0) //We are at the western face of this node and need to use the halo value
	 		tmpV[i] += state->cW[i/hL];
	 	else
	 		tmpV[i] += state->v[i-1];
	 	}
	 
	 	for(uint32_t i=0; i<hL*hL; i++) {
	 		state->v[i] = tmpV[i];
	 	}
	 	//for(uint32_t i=0; i<hL; i++){ 
	     //handler_log(2 ,"%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u", tmpV[(i*hL)+0],tmpV[(i*hL)+1],tmpV[(i*hL)+2],tmpV[(i*hL)+3],tmpV[(i*hL)+4],tmpV[(i*hL)+5],tmpV[(i*hL)+6],tmpV[(i*hL)+7],tmpV[(i*hL)+8]); 
	 	//} 
	 	return 0; 
	"""

	#code fragment that initialises the state of the center device
	center_init = """
			uint32_t hL = properties->haloLength;
			for(uint32_t i=0; i<hL*hL; i++) {
					state->v[i] = 0;
			}
			return 1;
	"""

	
	# Boundary update function body, example boundary_4
	#
	# 0	- cV[0]
	#	1 - cV[1]
	#	2 - cV[2]
	#	3 - cV[3]
	#
	# Currently it ignores it's own boundary and does nothing special, just accumulates it's neighbours together where it can
	# Boundary only has N, E, S -- the west edge is assumed to be the boundary. In reality this might be rotated and not truely be the western face..

	boundary_update = """ 
		uint32_t hL = properties->haloLength;
		uint32_t tmpV[hL];	
		
		//c = NorthOf(c) + EastOf(c) + SouthOf(c) 
		//Northmost element 
		tmpV[0] = 0 + state->cV[0] + state->v[1];
		
		//Middle Elements 
		for(uint32_t i=1; i<hL-1; i++) { 
			tmpV[i] = state->v[i-1] + state->cV[i] + state->v[i+1]; 
		} 
		
		//Southmost element 
		tmpV[hL-1] = state->v[hL-2] + state->cV[hL-1] + 0; 
		
		//Load tmp value array into the real value array 
		for(uint32_t i=0; i<hL; i++) { 
			state->v[i] = tmpV[i]; 
		} 
		return 1;
	"""

	#code fragment responsible for initialising the boundary device
	boundary_init = """
		uint32_t hL = properties->haloLength;
		for(uint32_t i=0; i<hL; i++) {
			state->v[i] = 2;
		}
		return 1;
	"""

	center = hE.Device(name='center', celltype='box', size=N, updateFunc=center_update, initFunc=center_init, properties='', state='') 
	boundary = hE.Device(name='boundary', celltype='edge', size=N, updateFunc=boundary_update, initFunc=boundary_init, properties='', state='') 
	exit_node = hE.Device(name='exit_node', celltype='exit', size=N, updateFunc='', initFunc='', properties='', state='') 
	dlist = [center, boundary, exit_node]

	#xmlo = hE.generate_graph('halo_exchange',dlist,N,False)

	#Instantiate the graph type and graph
	graphname = 'halo_exchange'
	debug = True 
	xmlo = hE.generate_graphHeader(graphname)
	xmlo+= hE.generate_sharedcode(graphname, dlist, debug)
	xmlo+= hE.generate_graphProperties(N)
	xmlo+= hE.generate_messages(N)
	xmlo+= hE.generate_devices(graphname, dlist, debug)
	xmlo+= '</GraphType>\n'
	xmlo+= generate_graphInstance(primaryCellsDim, N, graphname) 
	xmlo+= '</Graphs>\n'

	outfile = open(outFileName,'w')
	outfile.write(xmlo)
	outfile.close()


if __name__== "__main__":
	main(sys.argv[1:])
