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

bin/generate_izhikevich_sparse 800 200 400 0.0005 1000 |
    tee >(gzip - > ${WD}/net.txt.gz ) |
    bin/create_graph_instance_v2 |
    gzip - > ${WD}/net.xml.gz

gunzip -k ${WD}/net.txt.gz -c | bin/reference_simulator > ${WD}/out_ref.txt

${GS}/bin/epoch_sim --max-contiguous-idle-steps 1000000 --log-level 1 --stats-delta 1000 \
    ${WD}/net.xml.gz --external PROVIDER > ${WD}/out_sim.txt

diff <(sort ${WD}/out_ref.txt) <(sort ${WD}/out_sim.txt)