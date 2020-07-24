import scipy.stats as stats
import numpy as np
import math
import random

fb=14
scale=2**-fb

reps=4
target_std=math.sqrt(1.0/reps)
modify_std=1
target_std*=modify_std

def gen_table64():
    a1=1.113170523
    a3=-0.119175853
    a5=0.020313091

    tab=[]
    for i in range(0,64):
        u=(i+0.5)/64
        Li=stats.norm.ppf(u)
        Li2=Li*Li
        Li4=Li2*Li2
        #Lip=a1*Li+a3*Li*Li*Li+a5*Li*Li*Li*Li*Li
        Lip = Li*(Li4*a5+Li2*a3+a1) * target_std
        print(f"u={u}, y={Li}, y'={Lip}")
        fx=int(round(Lip/scale))
        tab.append(fx)

    return tab

table=gen_table64()
for t in table:
    print(f"{t}, // {t*scale}")

t=np.array([x*scale for x in table])
print(f"tab mean={np.mean(t)}")
print(f"tab std={np.std(t)}")
print(f"tab max(scaled) = {max(t)/scale}")


t2=np.add.outer(t,t).flatten()
print(t2.shape)

t4=np.add.outer(t2,t2).flatten()
t4.sort()
print(t4.shape)
print(f"conv4 std={np.std(t4)}")
print(f"conv4 kurt={stats.kurtosis(t4)}")

got_p=np.arange(0,len(t4))/len(t4)
exp_p=stats.norm.cdf(t4)
err_p=got_p-exp_p

for i in range(0,len(t4),100000):
    print(f"{t4[i]},{got_p[i]},{exp_p[i]},{err_p[i]}")

s=10
while s < 4000000:
    x=random.choices(t4, k=s)

    result=stats.anderson(x)
    print(result)

    s=int(s*1.2)