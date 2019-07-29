#!/bin/bash

g++ ising_spin_fix_ref.cpp -o ising_spin_fix_ref.opt -DNDEBUG=1 -O3

for n in $(seq 16 8 1024) ; do
    /usr/bin/time -f %e -o .timing.txt ./ising_spin_fix_ref.opt $n 1 1000 > /dev/null
    T=$(cat .timing.txt)
    echo "$n, $T"
done
