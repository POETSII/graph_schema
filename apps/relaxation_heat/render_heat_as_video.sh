#!/usr/bin/env bash

GRAPH=$1
SNAPSHOTS=$2

mkdir -p .tmp
TMP=$(mktemp -d .tmp/XXXXXXXX)

tools/render_graph_as_dot.py --device-type-filter cell --snapshots $SNAPSHOTS \
    --bind-dev '*' state currHeat fillcolor 'heat(-127,127,value)' \
    --bind-dev '*' state currVersion color 'cycle(value,4)' \
    --bind-dev '*' state currVersion penwidth '4.0' \
    --output $TMP/graph $GRAPH

for i in $TMP/*.dot ; do
    neato -Tpng -O $i
done

ffmpeg -y -r 10 -i $TMP/graph_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 $1.mp4
