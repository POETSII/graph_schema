

n=64
ps=linspace(0.01,1,100);
msgs=[]
stps=[]
for p = ps
    [r,steps,messages]=relaxation_heat(n,p);
    msgs=[msgs ; messages];
    stps=[stps ; steps];
    [p,messages]
end

plotyy(ps, msgs,  ps, stps)
