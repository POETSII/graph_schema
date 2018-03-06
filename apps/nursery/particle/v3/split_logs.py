#!/usr/bin/python3

import sys
import collections
import re

leftMsgs=collections.OrderedDict()
rightMsgs=collections.OrderedDict()
times=collections.OrderedDict()

for line in sys.stdin.readlines():
    
    g=re.match("SOFT \[([0-9a-zA-Z]+)\] ([0-9.]+) : (.+)", line)
    if not g:
        print(line)
    else:
        thread=int(g.group(1))
        time=float(g.group(2))
        msg=g.group(3)
    
        if thread==0:
            leftMsgs.setdefault(time,[]).append(msg)
        else:
            rightMsgs.setdefault(time,[]).append(msg)
        times[time]=None
 
for t in times:
    left=leftMsgs.get(t,[])
    right=rightMsgs.get(t,[])
    while len(left)>0 or len(right)>0:
        sys.stdout.write("{:.6f} |".format(t))
        
        val=left.pop(0) if len(left)>0 else ""
        sys.stdout.write(" {:70} |".format(val))
        
        val=right.pop(0) if len(right)>0 else ""
        sys.stdout.write(" {:70} |\n".format(val))
            


