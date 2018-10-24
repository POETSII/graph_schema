#!/bin/bash

CONFIG=$1
TEST=$2

coproc python3 

FIFO=$(mktemp -u)

mkfifo $FIFO

python3 echo_client.py $CONFIG $TEST < $FIFO | python3 echo_server.py $CONFIG > $FIFO

rm $FIFO
