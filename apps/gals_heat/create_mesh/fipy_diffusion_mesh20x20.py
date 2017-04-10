#!/usr/bin/env python

##
# ###################################################################
#  FiPy - Python-based finite volume PDE solver
#
#  FILE: "mesh20x20.py"
#
#  Author: Jonathan Guyer <guyer@nist.gov>
#  Author: Daniel Wheeler <daniel.wheeler@nist.gov>
#  Author: James Warren   <jwarren@nist.gov>
#    mail: NIST
#     www: http://www.ctcms.nist.gov/fipy/
#
# ========================================================================
# This software was developed at the National Institute of Standards
# and Technology by employees of the Federal Government in the course
# of their official duties.  Pursuant to title 17 Section 105 of the
# United States Code this software is not subject to copyright
# protection and is in the public domain.  FiPy is an experimental
# system.  NIST assumes no responsibility whatsoever for its use by
# other parties, and makes no guarantees, expressed or implied, about
# its quality, reliability, or any other characteristic.  We would
# appreciate acknowledgement if the software is used.
#
# This software can be redistributed and/or modified freely
# provided that any derivative works bear some notice that they are
# derived from it, and any modified versions bear some notice that
# they have been modified.
# ========================================================================
#
# ###################################################################
##

#Solve a two-dimensional diffusion problem in a square domain.
#This example solves a diffusion problem and demonstrates the use of
#applying boundary condition patches.
#.. index:: Grid2D
from fipy import CellVariable, Grid2D, Viewer, TransientTerm, DiffusionTerm
from fipy.tools import numerix
import random

doBlob=True
doCirc=False

if doBlob:
    from fipy import Gmsh2D
    
    mesh = Gmsh2D("meshed.msh")

elif doCirc:
    from fipy import Gmsh2D
    
    cellSize = 0.05
    radius = 1.
    
    mesh = Gmsh2D('''
               cellSize = %(cellSize)g;
               radius = %(radius)g;
               Point(1) = {0, 0, 0, cellSize};
               Point(2) = {-radius, 0, 0, cellSize};
               Point(3) = {0, radius, 0, cellSize};
               Point(4) = {radius, 0, 0, cellSize};
               Point(5) = {0, -radius, 0, cellSize};
               Circle(6) = {2, 1, 3};
               Circle(7) = {3, 1, 4};
               Circle(8) = {4, 1, 5};
               Circle(9) = {5, 1, 2};
               Line Loop(10) = {6, 7, 8, 9};
               Plane Surface(11) = {10};
               ''' % locals()) 
else:
    nx = 20
    ny = nx
    dx = 1.
    dy = dx
    L = dx * nx
    mesh = Grid2D(dx=dx, dy=dy, nx=nx, ny=ny)



#We create a :class:`~fipy.variables.cellVariable.CellVariable` and initialize it to zero:
phi = CellVariable(name = "solution variable",
                    mesh = mesh,
                    value = 0.)
#and then create a diffusion equation.  This is solved by default with an
#iterative conjugate gradient solver.
D = 1.
eq = TransientTerm() == DiffusionTerm(coeff=D)

X, Y = mesh.faceCenters

if doBlob:
    phi.constrain([random.random() for x in X], mesh.exteriorFaces)
    
elif doCirc:
    
    phi.constrain(X, mesh.exteriorFaces)
else:
    #We apply Dirichlet boundary conditions
    valueTopLeft = 0
    valueBottomRight = 1
    #to the top-left and bottom-right corners.  Neumann boundary conditions
    #are automatically applied to the top-right and bottom-left corners.
    
    
    facesTopLeft = ((mesh.facesLeft & (Y > L / 2))
                     | (mesh.facesTop & (X < L / 2)))
    facesBottomRight = ((mesh.facesRight & (Y < L / 2))
                         | (mesh.facesBottom & (X > L / 2)))
    phi.constrain(valueTopLeft, facesTopLeft)
    phi.constrain(valueBottomRight, facesBottomRight)





#We create a viewer to see the results
#.. index::
#   module: fipy.viewers
if __name__ == '__main__':
    if doBlob:
        try:
            viewer = Viewer(vars=phi, datamin=-1.5, datamax=1.)
            viewer.plot()
            raw_input("Irregular circular mesh. Press <return> to proceed...") # doctest: +GMSH
        except:
            print "Unable to create a viewer for an irregular mesh (try Matplotlib2DViewer or MayaviViewer)"
    elif doCirc:
        try:
            viewer = Viewer(vars=phi, datamin=-1, datamax=1.)
            viewer.plotMesh()
            raw_input("Irregular circular mesh. Press <return> to proceed...") # doctest: +GMSH
        except:
            print "Unable to create a viewer for an irregular mesh (try Matplotlib2DViewer or MayaviViewer)"
    else:
        viewer = Viewer(vars=phi, datamin=0., datamax=1.)
        viewer.plot()


#and solve the equation by repeatedly looping in time:
if doBlob:
    timeStepDuration = 10 * 0.9 * 0.05**2 / (2 * D)
elif doCirc:
    timeStepDuration = 10 * 0.9 * cellSize**2 / (2 * D)
else:
    timeStepDuration = 10 * 0.9 * dx**2 / (2 * D)
steps = 1000
for step in range(steps):
     eq.solve(var=phi,
              dt=timeStepDuration)
     if __name__ == '__main__':
         viewer.plot()
     print("step")
