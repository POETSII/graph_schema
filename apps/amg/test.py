#!/usr/env python3

import pyamg
import numpy as np
import scipy.sparse

n=100
omega=1.2

A = pyamg.gallery.poisson((n,n), format='csr')  # 2D Poisson problem on 500x500 grid
ml = pyamg.ruge_stuben_solver(A,
    max_coarse=1,
    max_levels=100,
    presmoother=('jacobi', {"withrho":True, "omega":omega}),
    postsmoother=('jacobi', {"withrho":True, "omega":omega})
    )                    # construct the multigrid hierarchy
print(ml)                                           # print hierarchy information
b = np.random.rand(A.shape[0])                      # pick a random right hand side
x = ml.solve(b, maxiter=2)                          # solve Ax=b to a tolerance of 1e-8
print("residual: ", np.linalg.norm(b-A*x))          # compute norm of residual vector

def jacobi(A, x, b, omega):
    Ad=A.diagonal()
    Atmp=scipy.sparse.lil_matrix(A.shape)
    Atmp.setdiag(Ad)
    Ar=A-Atmp
    return omega*(b-Ar*x)/Ad + (1-omega)*x

def v_solve(ml, b, lvl=0, x=None):
    level=ml.levels[lvl]
    
    if x==None:
        x=np.zeros(b.shape)
    
    if lvl==len(ml.levels)-1:
        assert level.A.shape==(1,1)
        x[0] = b[0] / level.A[0,0]
        return x
        
    from pyamg.relaxation.smoothing import rho_D_inv_A
    localOmega = omega/rho_D_inv_A(level.A)

#    print(" lvl={}, A={}, x={}, b={}".format(lvl,A.shape,x.shape,b.shape))
    
    xo=x.copy()
    level.presmoother(level.A, xo, b)
    x=jacobi(level.A, x, b, localOmega)
    
    r = b - level.A * x
    ro = b - level.A * xo
#    print(r)
#    print(ro)
    
    cb = level.R * r
    cx = np.zeros( cb.shape )
    
    #print("Up = {}".format(cx))
    print("Up/norm = {} vs {}".format(np.linalg.norm(r), np.linalg.norm(ro)))
    cx = v_solve(ml, cb, lvl+1, cx)
    #print("Down = {}".format(cx))
    
    #print(cx)
    c = level.P * cx
    #print(c)
    x = x + c

    xo=x.copy()
    level.postsmoother(level.A, xo, b)
    
    x=jacobi(level.A, x, b, localOmega)
    
    r = b - level.A * x
    ro = b - level.A * xo
    n = np.linalg.norm(r)
    print("Down/norm = {} vs {}".format(np.linalg.norm(r), np.linalg.norm(ro)))
    
    
    return x
    
def w_solve(ml, b, lvl=0, x=None):
    level=ml.levels[lvl]
    
    if x==None:
        x=np.zeros(b.shape)
    
    if lvl==len(ml.levels)-1:
        assert level.A.shape==(1,1)
        x[0] = b[0] / level.A[0,0]
        return x
        
    from pyamg.relaxation.smoothing import rho_D_inv_A
    localOmega = omega/rho_D_inv_A(level.A)

#    print(" lvl={}, A={}, x={}, b={}".format(lvl,A.shape,x.shape,b.shape))
    
    xo=x.copy()
    level.presmoother(level.A, xo, b)
    x=jacobi(level.A, x, b, localOmega)
    
    r = b - level.A * x
    ro = b - level.A * xo
#    print(r)
#    print(ro)
    
    cb = level.R * r
    cx = np.zeros( cb.shape )
    
    #print("Up = {}".format(cx))
    #print("Up/norm = {} vs {}".format(np.linalg.norm(r), np.linalg.norm(ro)))
    cx = w_solve(ml, cb, lvl+1, cx)
    cx = w_solve(ml, cb, lvl+1, cx)
    #print("Down = {}".format(cx))
    
    #print(cx)
    c = level.P * cx
    #print(c)
    x = x + c

    xo=x.copy()
    level.postsmoother(level.A, xo, b)
    
    x=jacobi(level.A, x, b, localOmega)
    
    r = b - level.A * x
    ro = b - level.A * xo
    n = np.linalg.norm(r)
    #print("Down/norm = {} vs {}".format(np.linalg.norm(r), np.linalg.norm(ro)))
    
    
    return x
    
x=v_solve(ml, b)
xw=w_solve(ml, b)
print(" v = {}, w = {}".format(np.linalg.norm(b-A*x), np.linalg.norm(b-A*xw)))
x=v_solve(ml, b, 0, x)
xw=w_solve(ml, b, 0, xw)
print(" v = {}, w = {}".format(np.linalg.norm(b-A*x), np.linalg.norm(b-A*xw)))
x=v_solve(ml, b, 0, x)
xw=w_solve(ml, b, 0, xw)
print(" v = {}, w = {}".format(np.linalg.norm(b-A*x), np.linalg.norm(b-A*xw)))
