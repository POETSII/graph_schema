#!/usr/bin/env bash

GRAPH=$1
SNAPSHOTS=$2

mkdir -p .tmp
TMP=$(mktemp -d .tmp/XXXXXXXX)

tools/render_graph_as_field.py \
    --bind-dev '*' state currHeat 'heat(-127,127,value)' \
    --output $TMP/graph \
    $GRAPH $SNAPSHOTS 

# The yuv420p is to allow standard windows players to play it
# keyframe every 25 frames makes it a bit more reasonable to seek
#   For frame-rate of 10, there is a seek point every two seconds
ffmpeg -y -r 10 -i $TMP/graph_%06d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -vf format=yuv420p -x264-params keyint=25 -crf 18 $1.field.mp4
