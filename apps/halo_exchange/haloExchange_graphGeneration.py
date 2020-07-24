

def generate_gridGraphInstance(problemSize, agglomerationFactor):
	P=problemSize
	N=agglomerationFactor
	assert N <= 11, "Currently agglomeration factors greater than 11 are not supported."

	# primary cells - these are the 2D agglomerated square cells that make up the bulk of the grid
	#					they have size NxN, 
	# secondary cells - these are the rectangular cells that make up the remaining space
	#					they have size N x (N % P) 

	primaryCellsDim = floor(P/N)
	secondaryCellsDim = P % N
	
		 		
