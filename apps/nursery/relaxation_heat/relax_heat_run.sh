#!/bin/bash

N="$1"

python3 apps/nursery/relaxation_heat/create_relaxation_heat_instance.py $1 > heat_$1.xml
pts-xmlc heat_$1.xml
/usr/bin/time -f %e --output heat_$1.time pts-serve --code code.v --data data.v --elf tinsel.elf --headless true 2>&1 | tee heat_$1.log

