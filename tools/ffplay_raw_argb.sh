#!/bin/bash

if [[ $# -ne 2 ]] ; then
    >&2 echo "ffplay_raw_argb.sh : You must specify the frame size"
    exit 1
fi

W=$1
H=$2
shift 2

FFCMD="ffplay -f rawvideo -pixel_format argb -s ${W}x${H}"

SCALE=1

while true ; do
    PIXELS=$(( ($W * $SCALE) * ($H * $SCALE) )) 
    if [[ $PIXELS -gt 65536 ]] ; then
        break 
    fi
    SCALE=$(($SCALE * 2 ))
done

if [[ $SCALE -ne 1 ]] ; then
    FFCMD+=" -vf scale=$(($W*$SCALE)):$(($H*$SCALE)):flags=neighbor"
fi

FFCMD+=" -i -"

>&2 echo "${FFCMD}"

${FFCMD}