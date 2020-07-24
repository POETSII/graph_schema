import matplotlib.pyplot as plt
import numpy as np
import matplotlib
import sys

sys.stderr.write("Starting\n")

if 0:
    ts=[]
    ns=[]
    for line in sys.stdin:
        (t,n)=line.split(',')
        t=int(t)
        n=int(n)
        ts.append(t)
        ns.append(n)

    plt.scatter(ts,ns,s=.025)
    plt.show()
elif 1:
    sys.stderr.write("Loading\n")
    ns=[]
    for line in sys.stdin:
        (t,n)=line.split(',')

        (t,n)=(int(t),int(n))
        while n >= len(ns):
            ns.append([])
        ns[n].append(t)
    
    sys.stderr.write("Plotting\n")
    plt.eventplot(ns)
    sys.stderr.write("Showing\n")
    plt.show()

else:
    sys.stderr.write("Loading\n")
    events=[]
    max_t=0
    max_n=0
    for line in sys.stdin:
        (t,n)=line.split(',')
        (t,n)=(int(t),int(n))
        events.append((t,n))
        max_t=max(max_t,t)
        max_n=max(max_n,n)

    sys.stderr.write("Converting to bitmap")
    bm=np.zeros( (max_n+1,max_t+1), dtype=np.double )
    for (t,n) in events:
        bm[n,t]=1.0
    
    sys.stderr.write("Plotting\n")
    plt.imshow(bm, cmap='gray')
    sys.stderr.write("Showing\n")
    plt.show()