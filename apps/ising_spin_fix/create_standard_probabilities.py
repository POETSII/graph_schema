#!/usr/bin/env python3

import math
import warnings
import csv
import os

def _calc_probs(T,J,H):
    probs=[0]*10
    for i in range(0,5):
        for j in range(0,2):
            index = i + 5*j; #'" index == 0,1,... ,9 "/
            my_spin = 2*j - 1;
            sum_nei = 2*i - 4;
            d_E = 2.*(J * my_spin * sum_nei + H * my_spin);
            x = math.exp(-d_E/T)
            p=x/(1.+x)
            probs[index]=min(2**32-1,math.floor(p * 2**32))
    return probs

def get_probs(T,J,H):
    appBase=os.path.dirname(os.path.realpath(__file__))
    try:
        with open(os.path.join(appBase,"standard_probabilities.csv")) as src:
            for line in src:
                parts=line.split(",")
                parts[0:3]=[float(p) for p in parts[0:3]]
                parts[3:13]=[int(p) for p in parts[3:13]]
                if parts[0:3]==[T,J,H]:
                    return parts[3:13]

        warnings.warn("(T,J,H) tuple not found in standard probabilities - results might not be repeatable.")
    except:
        warnings.warn("Couldn't load standard probabilities - results might not be repeatable.")
    
    return _calc_probs(T,J,H)


if __name__=="__main__":
    for T in [8.0,4.0,2.0, 1.0, 0.5, 0.25, 0.125]:
        for J in [1.0, 0.5, 0.25, 0.0]:
            for H in [1.0, 0.5, 0.25, 0.0]:
                probs=_calc_probs(T,J,H)

                print(f'{T},{J},{H},{",".join([str(s) for s in probs])}')
