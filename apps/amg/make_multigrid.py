import pyamg
import numpy as np
import scipy.sparse

class MultiGrid:
    class Level:
        def __init__(self, pyamgLevel, iterations, omega):
            self.A=pyamgLevel.A
            self.n=self.A.shape[0]
            self.iterations=iterations
            
            if self.n>1: 
                self.P=pyamgLevel.P
                self.R=pyamgLevel.R
            
                
                from pyamg.relaxation.smoothing import rho_D_inv_A
                self.omega = omega/rho_D_inv_A(self.A)
            
                Adiag=self.A.diagonal()
                AdiagInv=1.0/Adiag
                self.AdiagInvOmega = AdiagInv * self.omega
                
                Atmp=scipy.sparse.lil_matrix(self.A.shape)
                Atmp.setdiag(Adiag)
                self.Ar=scipy.sparse.csr_matrix(self.A-Atmp)
        
        def smooth_iteration(self,x,b):
            if self.n==1:
                x[0 ] = x[0] / self.A[0,0]
            else:
                # x' = (b-Ar*x)/Ad*omega + (1-omega)*x 
                # x' = (b-Ar*x)/Ad*omega - omega*x + x
                # x'-x = (b-Ar*x)/Ad*omega - omega*x
                x += (b-self.Ar*x)*self.AdiagInvOmega - self.omega*x
    
        def smooth(self,x,b):
            for i in range(self.iterations):
                self.smooth_iteration(x,b)
    
    def __init__(self, A, iterations=1, omega=1.2):
        """Create an exploded version of multi-grid solver using pyamg
        
        Parameters
        ----------
        A : an NxN matrix (ideally a sparse csr_matrix)
        
        omega : damping parameter.   x' = omega * Jacobi(A,x,b) + (1-omega)*x
          omega=1.0 : straight Jacobi
          omega<1.0 : damped relaxation (avoid over-shoot)
          omega>1.0 : over relaxation (anticipate bigger movements)
        """
          
        
        self.A=A
    
        print("Creating pyamg")
        self.ml = pyamg.ruge_stuben_solver(A,
            max_coarse=1,
            max_levels=100,
            presmoother=('jacobi', {"withrho":True, "omega": omega} ), 
            postsmoother=('jacobi', {"withrho":True, "omega": omega} )
        )                    

        print("Creating MultiGrid")
        self.levels=[]
        for L in self.ml.levels:
            self.levels.append( MultiGrid.Level( L, iterations, omega ) )
            
    def vcycle(self,x,b,lvl=0):
        L=self.levels[lvl]
        
        L.smooth(x,b)
        
        if lvl==len(self.levels)-1:
            return
            
        r=b-L.A*x   
        coarse_x=np.zeros(self.levels[lvl+1].n)
        coarse_b=L.R*r
        
        self.vcycle(coarse_x, coarse_b, lvl+1)
        
        x+=L.P*coarse_x

        L.smooth(x,b)
        
    # Take the residual calculation from _before_ smoothing,
    # so we can do residual and jacobi at the same time
    def vcycle_alt(self,x,b,lvl=0):
        L=self.levels[lvl]
        
        r=b-L.A*x  # wrong place to do it 
        L.smooth(x,b)
        
        if lvl==len(self.levels)-1:
            return
         
        # Correct place to do it
        #r=b-L.A*x   
        coarse_x=np.zeros(self.levels[lvl+1].n)
        coarse_b=L.R*r
        
        self.vcycle(coarse_x, coarse_b, lvl+1)
        
        x+=L.P*coarse_x

        L.smooth(x,b)

    # Don't do any smoothing on up cycle (!)
    def vcycle_alt2(self,x,b,lvl=0):
        L=self.levels[lvl]
        
        r=b-L.A*x  # wrong place to do it 
        # L.smooth(x,b) # No smoothing!
        
        if lvl==len(self.levels)-1:
            return
         
        # Correct place to do it
        #r=b-L.A*x   
        coarse_x=np.zeros(self.levels[lvl+1].n)
        coarse_b=L.R*r
        
        self.vcycle(coarse_x, coarse_b, lvl+1)
        
        x+=L.P*coarse_x

        L.smooth(x,b)


    def wcycle(self,x,b,lvl=0):
        L=self.levels[lvl]
        
        L.smooth(x,b)
        
        if lvl==len(self.levels)-1:
            return
            
        r=b-L.A*x   
        coarse_x=np.zeros(self.levels[lvl+1].n)
        coarse_b=L.R*r
        
        self.vcycle(coarse_x, coarse_b, lvl+1)
        self.vcycle(coarse_x, coarse_b, lvl+1)
        
        x+=L.P*coarse_x

        L.smooth(x,b)
        
        
    
def test(n=100):
    omega=1.0
    iterations=1
    print("Creating A")
    A = pyamg.gallery.poisson((n,n), format='csr')  # 2D Poisson problem on nxn grid
    print("Creating ref")
    ml = pyamg.ruge_stuben_solver(A,
        max_coarse=1,
        max_levels=100,
        presmoother=('jacobi', {"withrho":True, "omega":omega}),
        postsmoother=('jacobi', {"withrho":True, "omega":omega})
        )                    # construct the multigrid hierarchy
    
    mg=MultiGrid(A, omega=omega, iterations=iterations)
        
    b = np.random.rand(A.shape[0])                      # pick a random right hand side
    x0 = np.random.rand(A.shape[0])
    xP = x0.copy()
    xV = x0.copy()
    xVa = x0.copy()
    xVa2 = x0.copy()
    xW = x0.copy()
    i=0
    print("residuals : ", n, i, np.linalg.norm(b-A*xP), np.linalg.norm(b-A*xV), np.linalg.norm(b-A*xW))
    while True:
        i=i+1
        xP = ml.solve(b, tol=1e-9, maxiter=iterations, x0=xP)
        mg.vcycle(xV, b)
        mg.vcycle_alt(xVa, b)
        mg.vcycle_alt2(xVa2, b)
        mg.wcycle(xW, b)        
        print("residuals: ", n, i, np.linalg.norm(b-A*xP), np.linalg.norm(b-A*xV),np.linalg.norm(b-A*xVa),np.linalg.norm(b-A*xVa2), np.linalg.norm(b-A*xW))
        if np.linalg.norm(b-A*xV) < 1e-3:
            break

test(50)
test(100)
test(200)
test(400)
test(800)
test(1600)
