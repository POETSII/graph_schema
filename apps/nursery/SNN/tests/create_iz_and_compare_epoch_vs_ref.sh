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

bin/generate_izhikevich_sparse 400 100 200 0.0005 1000 |
    tee >(gzip - > ${WD}/net.txt.gz ) |
    tee >(bin/create_graph_instance_v2 GALSExact | gzip - > net.GALSExact.xml.gz) |
    tee >(bin/create_graph_instance_v2 HwIdle | gzip - > net.HwIdle.xml.gz) |
    cat > /dev/null

SORT="sort -t, -k1n,1 -k2n,2 -k3,3 -k4,4 -k5n,5  -k5n,5 "

############
## Reference

gunzip -k ${WD}/net.txt.gz -c | bin/reference_simulator | ${SORT}  > ${WD}/out_ref.txt

##############
## GALSExact

for i in $(seq 0 9) ; do
    >&2 echo "Running with seed $i"
    OUT="${WD}/out_sim.GalsExact.${i}.txt"
    ${GS}/bin/epoch_sim --max-contiguous-idle-steps 1000000 --log-level 1 --stats-delta 1000 \
        --prob-send 0.8 --prob-delay 0.2 --rng-seed ${i} \
        ${WD}/net.GALSExact.xml.gz --external PROVIDER | ${SORT} > ${OUT}

    diff ${WD}/out_ref.txt ${OUT}
done

##############
## HwIdle

for i in $(seq 1 10) ; do
    >&2 echo "Running with seed $i"
    OUT="${WD}/out_sim.HwIdle.${i}.txt"
    ${GS}/bin/epoch_sim --max-contiguous-idle-steps 1000000 --log-level 1 --stats-delta 1000 \
        --prob-send 0.8 --prob-delay 0.2 --rng-seed ${i} \
        ${WD}/net.HwIdle.xml.gz --external PROVIDER | ${SORT} > ${OUT}

    diff ${WD}/out_ref.txt ${OUT}
done