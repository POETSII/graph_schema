#!/bin/bash

CONFIG=$1

coproc python3 

FIFO=$(mktemp)

mkfifo $FIFO

python3 echo_client.py $CONFIG < $FIFO | python3 echo_server.py $CONFIG > $FIFO

rm $FIFO
