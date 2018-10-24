from typing import *
import json
from json_rpc_helpers import *
from client_connection import *
import sys
import logging

def client_echo1(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0"])
    logging.info("Client: run")
    connection.run()
    logging.info("Client: halt(0)")
    connection.halt(0)
    got=[]
    while len(got)==0:
        logging.info("Client: poll")
        got=connection.poll()
    logging.info("Client: done")
    connection.close()
    logging.info("Client: channel closed")

if __name__=="__main__":
    logging.basicConfig(level=logging.INFO)

    channel=JSONRawChannelOnStreams(sys.stdin, sys.stdout)
    proxy=JSONClientProxy(channel)
    dc=UpwardConnection(proxy)
    client_echo1(dc)
