Consider a 1d chain of cells.

Each cell has a current time and an time increment (t,dt). Within the
time [t,t+dt) the cell is happy that rates are constant and it can
reasonably jump forwards by dt in one burst of events. At the time t+dt
the cell needs to look at its molecules and work out how things have
changed, which will result in a new interval (t+dt,dt'), where dt'
could be different to dt.

Lets assume that dt is always of the form dt=2^j with j an integer.

Lets also assume that dt divides t, so that the intervals are
naturally aligned. So we can't have things like (0.5,1), or
(0.385,0.25). Essentially this means that we can move to a
finer dt on any step, but can only move to a coarser dt every
two steps.

Let us further assume that dt for neighbouring cells is always
within a factor of two. That gives us a few possible scenarios
for a given cell i (ignoring symmetric possibilities):

  dt_{i-1}   = dt_i = dt_{i+1}     // Flat: Equal steps

  dt_{i-1}/2 = dt_i = dt_{i+1}     // FineEdge: Node is the (interior) edge of a fine zone

  dt_{i-1}/2 = dt_i = dt_{i+1}*2   // Slope: Node is in a gradient

  dt_{i-1}*2   = dt_i = dt_{i+1}   // CoarseEdge: Node is the (interior) edge of a coarse zone

  dt_{i-1}/2 = dt_i = dt_{i-1}/2   // Finest: Node is finer than neighbours

  dt_{i-1}*2 = dt_i = dt_{i-1}*2   // Coarsest: Node is coarser than neighbours

This essentially gives a time step gradient and 2nd derivative,
though of a slightly complex form as the changes are multiplicative.


A requirement for synchronisation is that cells must at least
be adjacent in time, though one might be in the step ahead. So
we must have one of :

  t_i + dt_i >= t_{i+1}
  t_i        <= t_{i+1} + dt_{i+1}

Given the constraints on dt and t, this results in one of:

  if dt_i == dt_{i+1}
  t_i = t_{i+1} + dt_i  // Neighbour is behind	
  t_i = t_{i+1}         // Both in same time step
  t_i = t_{i+1} - dt_i  // Neighbour is ahead
  
  if dt_i = dt_{i+1}*2  // we are coarser than neighbour
  t_i = t_{i+1} + dt_{i+1} // Neighbour is behind
  t_i = t_{i+1}            // Neighbour is in the same timestep, but the first half
  t_i = t_{i+1} - dt_{i+1} // Neighbour is in the same timestep, but the second half
  t_i = t_{i+1} - dt_i     // Neighbour is ahead

  if dt_i = dt_{i+1}/2  // We are finer than neighbour
  t_i = t_{i+1} + dt_{i+1} // Neighbour is behind
  t_i = t_{i+1}            // Neighbour is in same timestep, we are in the first half
  t_i = t_{i+1} + dt_i     // Neighbour is in same timestep, we are in the second half
  t_i = t_{i+1} - dt_i


Wherever we have a coarse-fine boundary, the nodes on either side know
exactly what the other is doing. One the fine side, it needs to step
twice, but if the coarse side is saying that it is fairly stable through
the coarse time step, the fine side can simply assume that it receives
the same input in both fine time-steps (or splits it equally between the
two).

Based on that, only the node on the fine side of a fine-coarse boundary
needs to care about the transition. So the variants are:

  if Flat or CoarseEdge or Coarsest:
    ( wait left || wait right)
    step dt
    (send left || send right)
    
  if FineEdge or Gradient:
    (wait fine || wait coarse)
    step dt
    (send fine || wait fine)
    step dt
    (send fine || send coarse)

  if Finest:
    (wait left || wait right)
    step dt
    step dt
    (send left || send right)
    
In a system with statically determined time-steps this should work
fine. But for dynamic time deltas we are stuck with a propagation
problem - how do we enforce the rule that each cell is either half
or twice the dt? If we allow people to change on a step by step
basis, then we could enforce that a cell could only go finer if
it knows the current neighbourhood is flat. But this eventually
implies global knowledge of the system.

Decent spiral conductivity:

a(x,y):= if (x=0 and y=0) then 0 else ( mod(atan2(y,x)+2*%pi,2*%pi)/(2*%pi) );
r(x,y):=sqrt(x^2+y^2);
dist(x,y) := ( cos( mod( r(x,y)+a(x,y), 1 )*2*%pi  ) + 1 )^2;

