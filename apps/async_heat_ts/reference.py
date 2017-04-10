import numpy
import math
import random
import sys

class SY:
    def __init__(self,u0,a,dx,dy,dt):
        """
        u0 : initial state (determines width and height, plus gives boundaries)
        a : Diffusion constant
        dt : Time step
        """
        self.w=u0.shape[0]
        self.h=u0.shape[1]
        self.a=a
        self.dt=dt
        self.dx=dx
        self.dy=dy
        
        # Equation 2.1
        self.r_x = a*a * dt / (dx*dx)
        self.r_y = a*a * dt / (dy*dy)
        
        self.t=0
        self.u=numpy.copy(u0)
        
        
    def step_point(self,F,u,t,x,y):
        d2_ux2 = u[x-1,y] - 2*u[x,y] + u[x+1,y]
        d2_uy2 = u[x,y-1] - 2*u[x,y] + u[x,y+1]
        
        return (u[x,y] +
                self.r_x * d2_ux2 +
                self.r_y * d2_uy2 +
                self.dt*F(t,x,y))
        
    def step(self,F):
        up=numpy.copy(self.u)
        for x in range(1,self.w-1):
            for y in range(1,self.h-1):
                up[x,y]=self.step_point(F,self.u,self.t,x,y)
        self.t+=self.dt
        self.u=up
    
    def current_sol(self):
        return (numpy.ones_like(self.u)*self.t,self.u)


class AS:
    def __init__(self,u0,a,dx,dy,dt):
        """
        u0 : initial state (determines width and height, plus gives boundaries)
        a : Diffusion constant
        dt : Time step
        """
        self.w=u0.shape[0]
        self.h=u0.shape[1]
        self.a=a
        self.dt=dt
        self.dx=dx
        self.dy=dy
        self.pUpdate=0.5
        
        # Equation 2.1
        self.r_x = a*a * dt / (dx*dx)
        self.r_y = a*a * dt / (dy*dy)
        
        self.t=numpy.zeros_like(u0)
        self.u=numpy.copy(u0)
        
        
    def step_point(self,F,u,t,x,y):
        d2_ux = u[x-1,y] - 2*u[x,y] + u[x+1,y]
        d2_uy = u[x,y-1] - 2*u[x,y] + u[x,y+1]
        
        nu = (  u[x,y] +
                self.r_x * d2_ux +
                self.r_y * d2_uy +
                self.dt*F(t[x,y],x,y))
        nt = t[x,y]+self.dt
        return (nt,nu)
        
    def step(self,F):
        up=numpy.copy(self.u)
        tp=numpy.copy(self.t)
        for x in range(1,self.w-1):
            for y in range(1,self.h-1):
                if random.random() < self.pUpdate:
                    (nt,nu)=self.step_point(F,self.u,self.t,x,y)
                    tp[x,y]=nt
                    up[x,y]=nu
        
        # Boundaries are always at the same time as neighbour
        tp[1:w-1,0]=tp[1:w-1,1]
        tp[1:w-1,h-1]=tp[1:w-1,h-2]
        tp[0,1:h-1]=tp[1:w-1,1]
        tp[w-1,1:h-1]=tp[h-2,1:h-1]
        tp[0,0]=tp[0,1]
        tp[0,h-1]=tp[0,h-2]
        tp[w-1,0]=tp[w-1,1]
        tp[w-1,h-1]=tp[w-1,h-2]
        
        self.t=tp
        self.u=up
    
    def current_sol(self):
        return (self.t,self.u)


class CA:
    def __init__(self,u0,a,dx,dy,dt):
        """
        u0 : initial state (determines width and height, plus gives boundaries)
        a : Diffusion constant
        dt : Time step
        """
        self.w=u0.shape[0]
        self.h=u0.shape[1]
        self.a=a
        self.dt=dt
        self.dx=dx
        self.dy=dy
        self.pUpdate=0.5
        
        self.t=numpy.zeros_like(u0)
        self.u=numpy.copy(u0)
        
        
    def step_point(self,F,u,t,x,y):
        a=self.a
        dx=self.dx
        dy=self.dy
        dt=self.dt
        
        # The over-bar version s^2 u_j
        d2b_ux = u[x-1,y] - 2*u[x,y] + u[x+1,y]
        d2b_uy = u[x,y-1] - 2*u[x,y] + u[x,y+1]
        
        d2_tx = (t[x-1,y] - 2*t[x,y] + t[x+1,y])
        d2_ty = (t[x,y-1] - 2*t[x,y] + t[x,y+1])
        
        K = ( 1 +
              a*a * d2_tx / (dx*dx) +
              a*a * d2_ty / (dy*dy) )
        
        nu = (  u[x,y] +
                (dt/K) * (a*a*d2b_ux) / (dx*dx) +
                (dt/K) * (a*a*d2b_uy) / (dy*dy) +
                (dt/K) * F(t[x,y],x,y) )
        
        nt = t[x,y]+self.dt
                
        # Stability
        alpha_x=(t[x-1,y]-t[x,y])/dt
        beta_x=(t[x+1,y]-t[x,y])/dt
        alpha_y=(t[x,y-1]-t[x,y])/dt
        beta_y=(t[x,y+1]-t[x,y])/dt
        rx=a*a*dt/(dx*dx)
        ry=a*a*dt/(dy*dy)
        s = (rx+ry) / (1 + rx * (alpha_x+beta_x) + ry * (alpha_y+beta_y))
        
        assert(0< s <= 0.5), "  s={}".format(s)
        
        return (nt,nu)
        
    def step(self,F):
        up=numpy.copy(self.u)
        tp=numpy.copy(self.t)
        for x in range(1,self.w-1):
            for y in range(1,self.h-1):
                if random.random() < self.pUpdate:
                    (nt,nu)=self.step_point(F,self.u,self.t,x,y)
                    tp[x,y]=nt
                    up[x,y]=nu
        
        # Boundaries are always at the same time as neighbour
        tp[1:w-1,0]=tp[1:w-1,1]
        tp[1:w-1,h-1]=tp[1:w-1,h-2]
        tp[0,1:h-1]=tp[1:w-1,1]
        tp[w-1,1:h-1]=tp[h-2,1:h-1]
        tp[0,0]=tp[0,1]
        tp[0,h-1]=tp[0,h-2]
        tp[w-1,0]=tp[w-1,1]
        tp[w-1,h-1]=tp[w-1,h-2]
        
        self.t=tp
        self.u=up
    
    def current_sol(self):
        return (self.t,self.u)


class CA2:
    def __init__(self,u0,a,dx,dy,dt):
        """
        u0 : initial state (determines width and height, plus gives boundaries)
        a : Diffusion constant
        dt : Time step
        """
        self.w=u0.shape[0]
        self.h=u0.shape[1]
        self.a=a
        self.dt=dt
        self.dx=dx
        self.dy=dy
        self.pUpdate=0.5
        
        self.t=numpy.zeros_like(u0)
        self.u=numpy.copy(u0)
        
        
    def step_point(self,F,u,t,x,y):
        a=self.a
        dx=self.dx
        dy=self.dy
        dt=self.dt
        
        alpha_x=(t[x-1,y]-t[x,y])/dt
        beta_x=(t[x+1,y]-t[x,y])/dt
        alpha_y=(t[x,y-1]-t[x,y])/dt
        beta_y=(t[x,y+1]-t[x,y])/dt
        rx=a*a*dt/(dx*dx)
        ry=a*a*dt/(dy*dy)
        
        d2b_ux = u[x-1,y] - 2*u[x,y] + u[x+1,y]
        d2b_uy = u[x,y-1] - 2*u[x,y] + u[x,y+1]        
        
        K = 1 / ( 1 +
                    rx*(alpha_x+beta_x) +
                    ry*(alpha_y+beta_y) )
        
        nu = (  u[x,y] +
                (1/K) * rx * d2b_ux + 
                (1/K) * ry * d2b_uy +
                (1/K) * dt * F(t[x,y],x,y) )
        
        nt = t[x,y]+self.dt
                
        # Stability
        s = (rx+ry) / (1 + rx * (alpha_x+beta_x) + ry * (alpha_y+beta_y))
        
        assert(0< s <= 0.5), "  s={}".format(s)
        
        return (nt,nu)
        
    def step(self,F):
        up=numpy.copy(self.u)
        tp=numpy.copy(self.t)
        for x in range(1,self.w-1):
            for y in range(1,self.h-1):
                if random.random() < self.pUpdate:
                    (nt,nu)=self.step_point(F,self.u,self.t,x,y)
                    tp[x,y]=nt
                    up[x,y]=nu
        
        # Boundaries are always at the same time as neighbour
        tp[1:w-1,0]=tp[1:w-1,1]
        tp[1:w-1,h-1]=tp[1:w-1,h-2]
        tp[0,1:h-1]=tp[1:w-1,1]
        tp[w-1,1:h-1]=tp[h-2,1:h-1]
        tp[0,0]=tp[0,1]
        tp[0,h-1]=tp[0,h-2]
        tp[w-1,0]=tp[w-1,1]
        tp[w-1,h-1]=tp[w-1,h-2]
        
        self.t=tp
        self.u=up
    
    def current_sol(self):
        return (self.t,self.u)


w=8
h=8
dx=1.0/(w-1)
dy=1.0/(h-1)
a=0.05
dt=0.1
u0=numpy.zeros([w,h])

def F(t,x,y):
    if x==1 and y==1:
        return 1
    elif x==w-2 and y==h-2:
        return math.sin(t)
    elif x==1 and y==h-2:
        return math.cos(t/math.pi)
    else:
        return 0

def create_solution_cuboid(S,F,u0,a,dx,dy,dt,steps):
    cuboid=numpy.zeros([u0.shape[0],u0.shape[1],steps])
    solver=S(u0,a,dx,dy,dt)
    ts=[t*dt for t in range(0,steps)]
    ti=numpy.zeros(u0.shape,dtype=numpy.long)
    
    while( (ti < steps).any() ):
        solver.step(F)
        (tt,uu)=solver.current_sol()
        for x in range(0,u0.shape[0]):
            for y in range(0,u0.shape[1]):
                while ti[x,y] < steps and tt[x,y] >= ts[ti[x,y]]:
                    cuboid[x,y,ti[x,y]] = uu[x,y]
                    ti[x,y]+=1
        sys.stderr.write(" min(t) = {}\n".format(ti.min()))
    
    return cuboid

solver=SY(u0,a,dx,dy,dt)
#solver=AS(u0,a,dx,dy,dt)
#solver=CA(u0,a,dx,dy,dt)
#for i in range(1000):
#    solver.step(F)
#    (tt,uu)=solver.current_sol()
#    print(numpy.array_str(uu,precision=3,suppress_small=True))
#    print(numpy.array_str(tt,precision=3,suppress_small=True))
 
cubeSY=create_solution_cuboid(SY,F,u0,a,dx,dy,dt,100)    
cubeAS=create_solution_cuboid(AS,F,u0,a,dx,dy,dt,100)    
cubeCA2=create_solution_cuboid(CA2,F,u0,a,dx,dy,dt,100)    

for ti in range(cubeSY.shape[2]):
    errAS=cubeSY[:,:,ti]-cubeAS[:,:,ti]
    errCA2=cubeSY[:,:,ti]-cubeCA2[:,:,ti]
    print(numpy.array_str(errAS,precision=3,suppress_small=True))
    print(numpy.array_str(errCA2,precision=3,suppress_small=True))
    print()
    print("AS = {}, CA2 = {}\n\n".format( numpy.linalg.norm(errAS,ord=2), numpy.linalg.norm(errCA2,ord=2)))
