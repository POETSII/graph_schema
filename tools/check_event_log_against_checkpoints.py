#!/usr/bin/env python3

import graph.core
import graph.checkpoints
import sys

graphInstPath=sys.argv[1]
checkpointPath=sys.argv[2]
eventLogPath=sys.argv[3]

graph.checkpoints.apply_checkpoints(graphInstPath,checkpointPath,eventLogPath)
