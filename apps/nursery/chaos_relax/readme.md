We have a standard relaxation problem, with an implicit-style
set of equations:

    eq : a[-1] * x[i-1,t] + a[0] * x[i,t] + a[+1] * x[i+1,t] = x[i,t-1];
    
So we have a standard update equation based on {x[i+1,t],x[i,t-1],x[i-1,t]}

    update : solve(eq, x[i,t])[1];
    > x[i,t]=-(a[1]*x[i+1,t]-x[i,t-1]+a[-1]*x[i-1,t])/a[0]

We can start the relaxation by assuming that for all interior
nodes we have:

   x[:,t] = x[:,t-1]

and any boundary nodes will be updated independently. Probably some
kind of velocity estimate would be better, e.g. a forwards difference
in time:

   x[i,t] = (x[i,t-1] - x[i,t-2]) + x[i,t-1]
          = 2*x[i,t-2] - x[i,t-2]

However, we do it, we know x[:,t-1], and have a candidate x[:,t].
The current error is then:

    err : lhs(eq - x[i,t-1]);
    a[1]*x[i+1,t]+a[0]*x[i,t]-x[i,t-1]+a[-1]*x[i-1,t]
    
So we can just keep looping:

   while(1){
      calculate error
      if error > eps:
        update x[i,t]
        notify neighbours of new x[i,t]
      wait for new neighbour value
   }
   
