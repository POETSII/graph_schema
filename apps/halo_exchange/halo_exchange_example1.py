import sys, getopt
import re
import haloExchange as hE


def main(argv):
	outFileName=''
	try:
		opts, args = getopt.getopt(argv, "h:o", ["output="])
	except getopt.GetoptError:
		print 'Usage python halo_exchange_example1.py --output graph_type.xml'
		sys.exit()
	for opt, arg in opts:
		if opt == '-h':
			print 'Usage python halo_exchange_example1.py --output graph_type.xml'
			sys.exit()
		if opt in ("-o", "--output"):
			outFileName = arg	
	assert(outFileName != '')	

	N = 9;

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
		uint32_t hL = gp->haloLength;
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
	 	for(uint32_t i=0; i<hL; i++){ 
	     handler_log(2 ,"%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u", tmpV[(i*hL)+0],tmpV[(i*hL)+1],tmpV[(i*hL)+2],tmpV[(i*hL)+3],tmpV[(i*hL)+4],tmpV[(i*hL)+5],tmpV[(i*hL)+6],tmpV[(i*hL)+7],tmpV[(i*hL)+8]); 
	 	} 
	 	return 0; 
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
		uint32_t hL = gp->haloLength;
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


	center = hE.Device(name='center', celltype='box', size=N, updateFunc=center_update) 
	boundary = hE.Device(name='boundary', celltype='edge', size=N, updateFunc=boundary_update) 
	exit_node = hE.Device(name='exit_node', celltype='exit', size=N, updateFunc='') 
	dlist = [center, boundary, exit_node]

	xmlo = hE.generate_graph('halo_exchange',dlist,N,True)

	outfile = open(outFileName,'w')
	outfile.write(xmlo)



if __name__== "__main__":
	main(sys.argv[1:])
