#!/bin/bash


FFCMD="ffplay -framerate 25 -probesize 4096"
FFCMD+=" -vf scale='max(512,iw)':'max(512,ih)':flags=neighbor"

FFCMD+=" -i -"

>&2 echo "${FFCMD}"

${FFCMD}