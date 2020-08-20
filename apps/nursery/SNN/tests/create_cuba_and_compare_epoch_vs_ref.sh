#!/bin/bash

WD=$1
GS=$2

if [[ "$WD" == "" ]] ; then
    WD="."
fi
if [[ "$GS" == "" ]] ; then
     GS="../../.."
fi

set -eou pipefail

bin/generate_CUBA 1000 |
    tee >(gzip - > ${WD}/net.txt.gz ) |
    bin/create_graph_instance_v2 |
    gzip - > ${WD}/net.xml.gz

SORT="sort -t, -k1n,1 -k2n,2 -k3,3 -k4,4 -k5n,5  -k5n,5 "

gunzip -k ${WD}/net.txt.gz -c | bin/reference_simulator | ${SORT}  > ${WD}/out_ref.txt

for i in $(seq 1 10) ; do
    >&2 echo "Running with seed $i"
    ${GS}/bin/epoch_sim --max-contiguous-idle-steps 1000000 --log-level 1 --stats-delta 1000 \
        --prob-send 0.4 --rng-seed ${i} \
        ${WD}/net.xml.gz --external PROVIDER | ${SORT} > ${WD}/out_sim.txt

    diff ${WD}/out_ref.txt ${WD}/out_sim.txt
done