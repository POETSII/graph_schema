
Time is discretised into integer points.

Space is discretised into a set of cells C,
of an unspecified dimension and topology.

There exists a function weight(i,j), which specifies
the amount that cell j contributes in the calculation
of i. If weight(i,j) is zero, then the cells are
not adjacent. It is required that adjacency is transitive
in order to make the spatial mapping make sense:

    \forall i,j \in C: weight(i,j) !=0 <-> weight(j,i) != 0

Each cell i has a neighbourhood of adjacent cells:

   \forall i \in C: nhood(i) = { j : j \in C ^ adjacent(i,j) > 0 }

Each cell i has an intrinsic time-step dt_{i},
where dt_{i} is an integer power of two:

   \forall i \in C: dt_{i} = 2^j, j>=0

For any cell i, it is required that all cells in a neighbourhood
have a time-step that is within a factor of two of the center cell:

   \forall i: \forall j \in nhood(i) -> dt_{j}/2 <= dt_{i} <= dt_{j}*2
   
To make things more manageable, we'll tighten this constraint
such that all cells in the neighbourhood are either:

- Flat: all cells in neighbourhood are of the same granularity:

  \forall i: \forall j \in nhood(i) -> dt_{i} = dt_{j}

- Fine-edge: all of the same (fine) granularity as the center cell or coarser:

  \forall i: \forall j \in nhood(i) -> dt_{i} = dt_{j} \/ dt_{i}*2 = dt_{j}  

- Coarse-edge: all of the same (coarse) granularity as the center cell or finer:

  \forall i: \forall j \in nhood(i) -> dt_{i} = dt_{j} \/ dt_{i}/2 <= dt_{j}

The intuitive explanation is that flat cells are in the middle of some
area with roughly equal curvature, fine-edges are the interior boundary
of a region of higher curvature, and coarse edges are interiod boundary
of a region with flatter curvature. A coarse-edge will naturally
line up against a fine-edge.

The requirement that all neighbourhoods only contain two distinct 
deltas means we avoid the situation where a cell has a finer
time-step on one side, and a coarser time-step on the other. This
stops us having very rapid changes in time-step, but makes it
it easier to manage.



Each cell steps through time at it's natural granularity (dt_{i}),
producing a new value v_{i,t} for each multiple of time-step, and
effectively remaining constant in-between.

    \forall i, t: ! (dt_i | t) -> v_{t,i} == v_{t,floor(t/dt_i)}

For each time-step on the cell's intrinsic granularity, the
new value is calculated based on the most recent value
of the neighbours:

    \forall i, t, j:  (dt_i | t)  ->  v_{t,i} = \sum_{j \in nhood(i)} weight(i,j) * v_{j,t-1}
    
Because the neighbour cells remain constant from the last update,
this means that cell value v_{i,t} can rely on values from
at most three time steps:

    t-dt_{i}*2    // Further back in time than i's last time step
    t-dt_{i}      // Same as i's last time-step
    t-dt_{i}/2    // Closer in time than i's last time step (only possible if dt_{i}>1)

For a one dimensional regular grid, this leads to the
following sensible scenarios (ignoring symmetries):

    -       +             +             +
    |      /|\           /|\           /|\
  dt_{i}  / | \         + | \         / | \
    |    /  |  \          |  \       /  |  \
    -   +   +   +         +   +     +   +   \
                                             \
                                              \
                                               +
    
Unlikely scenarios (in 1D) would be:

        +             +
       /|\           /|\
      + | +         / | \
        |          /  |  \
        +         /   +   \
                 /         \ 
                /           \
               +             +

In regular scenario we can then use finite-difference to
work out a discretised version of the PDE for each update,
which is used to give weight(i,j).

When moving between time-deltas we end up with four scenarios
that looks like this (in the 1D case):


    Finer time-step:
    - Receive two values per update (even or odd)
         
         +   +    +      +           even
        /|\  |    |      |
       / | \ |    |      |
      /  |  \|    |      |
     +   +   +    |      |           odd
        /|\  |    |      |
       / | \ |    |      |
      /  |  \|    |      |
     +   +   +    +      +       +   even
                 
    Boundary, fine-side:
    - Receive two values for odd update
    - Receive one value for even update
                 
         +   +    +      +          even
         |  /|\   |      |
         | / | |  |      |
         |/  | |  |      |
     +   +   +  \ |      |          odd
         |  /|\  ||      |
         | / | \ ||      |
         |/  |   \|      |
     +   +   +    +      +       +  even
                  
    Boundary, coarse-side:
    - Receive three updates for even update
                  
         +   +    +      +          even
         |   |   /|\     |
         |   |  / | \    |
         |   | /  |  \   |
     +   +   +    |  |   |          odd (no update)
         |   |    |   \  |
         |   |    |    \ |
         |   |    |     ||
     +   +   +    +      +       +  even
                  
    Fully coarse side:
    - This is equivalent to a full fine step
                  
         +   +    +      +          odd (or even)
         |   |    |     /|\
         |   |    |    / | \
         |   |    |   |  |  \
     +   +   +    |   |  |   \      n/a (no update)
         |   |    |  /   |    \
         |   |    | /    |     \
         |   |    |/     |      \
     +   +   +    +      +       +  even (or odd)
                  
Nodes broadcast their latest value to all neighbours
unconditionally at each time-step. For flat cells
this is not too much of a problem - they need to
maintain a "current" context for values needed for
the next time-step, and a "next" context.


The overall state machine for calculating cells
then becomes:

cell[flat]{
    dp:{nhoodSize:int,wSelf:fixed,dt:int}
    ds:{t:int,cAcc:fixed,cSeen:int,nAcc:fixed,nSeen:int}

    input[input]{
        ep{w:fixed}
        m{t:int,v:fixed}
        on_recv{
            if(m.t==ds.t){
                ds.cSeen++;
                ds.cAcc += ep.w * m.v;
            }else if(mt.t==ds.t+dp.dt){
                ds.nSeen++;
                ds.cAcc += ep.w * m.v;
            }else{
                fail();
            }
        }
    }
      
    output/OnSend: m{t,v}
        m.t = ds.t;
        m.v = ds.cAcc;
        ds.t += dp.dt;
        ds.cAcc = ds.cAcc * dp.wSelf + ds.nAcc;
        ds.cSeen = ds.nSeen;
        ds.nAcc = 0;
        ds.nSeen = 0;

    output/Ready:
        return ds.cSeen==dp.nhoodSize
