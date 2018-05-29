Consider a 1d chain of cells.

Each cell i has:
- current time t
- current value v
- next time nt

Each cell can step from time t to nt when it
knows it's neighbour values at time t.

All cell's values are assumed to linearly change in value
from t to nt

Each message sent is a bracket, containing:
- nt, t,v, tp,vp
where nt < t < pt, and the message represents
the interval [t,tp)
That means that for any given t, there will be
exactly one message from each neighbour such that
t \in [t,tp)

If messages come out of order, then later brackets may
come before earlier brackets. This could cause a problem
if one neighbour is waiting for something at time t, but
the message for d+dt comes first. It can't hold on to both
unless we have unbounded memory.

Hrmm, need to prove that this can't happen...


All the neighbours round a point have a time nt that they
want to step to, which we'll assume is known. Let us somehow
ensure that the times are _always_ distinct, e.g. by colouring
the graph, and embedding it in the LSBs of the times.

Then there must exist a lowest (earliest) time within each
neighbourhood, and the node with the earliest time always
knows it. This is the same as the async ising spin approach.
When that node fires, either it may no longer be the earliest
time, or it could still be the earliest time. If it isn't
the earliest, then control passs to someone else, and it
can't fire again. If it remains the earliest, then potentially
it could send two messages, which could arrive out of order.
The second message could trigger another cell. However, the
sender must still have passed through those states, so it
doesn't care.

... I think it all works...?


