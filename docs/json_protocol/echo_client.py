from typing import *
import json
from json_rpc_helpers import *
from client_connection import *
import sys
import logging

def client_echo1_test1(connection:UpwardConnection):
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

def client_echo1_test2(connection:UpwardConnection):
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
    logging.info("Client: poll (expecting error)")
    try:
        got=connection.poll()
        logging.info("Client: poll (FAIL: NO ERROR)")
        sys.exit(1)
    except:
        logging.info("Client: poll (got error)")
    logging.info("Client: done")
    connection.close()
    logging.info("Client: channel closed")

def client_echo1_test3(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0"])
    logging.info("Client: run")    
    connection.run()
    logging.info("Client: send")
    connection.send([
        MulticastMessage(Endpoint("ext0:out"))
    ])
    got=[]
    while len(got)==0:
        logging.info("Client: poll")
        got=connection.poll(max_events=1)
    assert len(got)==1
    assert isinstance(got[0],MulticastMessage)
    assert got[0].src==Endpoint("int0:out")

    logging.info("Client: halt(0)")
    connection.halt(0)
    got=[]
    while len(got)==0:
        logging.info("Client: poll")
        got=connection.poll(max_events=1)
    assert isinstance(got[0], Halt)
    connection.close()

def client_echo2parallel_test1(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0","ext1"])
    logging.info("Client: run")    
    connection.run()
    logging.info("Client: halt(0)")
    connection.halt(0)
    got=[]
    while len(got)==0:
        logging.info("Client: poll")
        got=connection.poll(max_events=1)
    assert isinstance(got[0], Halt)
    connection.close()

def client_echo2parallel_test3(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0","ext1"])
    logging.info("Client: run")    
    connection.run()
    logging.info("Client: send")
    connection.send([
        MulticastMessage(Endpoint("ext0:out"), {"scalar1":2})
    ])
    logging.info("Client: halt(1)")
    connection.halt(1)
    got=[]
    while len(got)<2:
        logging.info("Client: poll")
        got+=connection.poll()
    for e in got:
        if isinstance(e,Halt):
            assert e.code==1
        if isinstance(e,MulticastMessage):
            assert e.data["scalar1"]==2
    connection.close()

def client_echo2parallel_test4(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0","ext1"])
    logging.info("Client: run")    
    connection.run()
    logging.info("Client: send")
    connection.send([
        MulticastMessage(Endpoint("ext0:out"), {"scalar1":2}),
        MulticastMessage(Endpoint("ext1:out"), {"scalar2":3})
    ])
    logging.info("Client: halt(2)")
    connection.halt(2)
    got=[]
    while len(got)<2:
        logging.info("Client: poll")
        got+=connection.poll()
    for e in got:
        if isinstance(e,Halt):
            assert e.code==2
        if isinstance(e,MulticastMessage):
            if e.src.full=="int0:out":
                assert e.data["scalar1"]==2
            if e.src.full=="int1:out":
                assert e.data["scalar2"]==3
    connection.close()


def client_echo3fork_test1(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0","extA","extB"])
    logging.info("Client: checking incoming edges")
    assert len(connection.incoming_edges)==1
    assert len(connection.incoming_edges[Endpoint("int0:out")])==2
    assert Endpoint("extA:in") in connection.incoming_edges[Endpoint("int0:out")]
    assert Endpoint("extB:in") in connection.incoming_edges[Endpoint("int0:out")]
    connection.close()

def client_echo3fork_test2(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0","extA","extB"])
    logging.info("Client: run")
    connection.run()
    logging.info("Client: send")
    connection.send([
        MulticastMessage("ext0:out")
    ])
    logging.info("Client: poll")
    got=[]
    while len(got)==0:
        got += connection.poll()
    assert len(got)==1
    assert got[0].src.full=="int0:out"

    logging.info("Client: halt(0)")
    connection.halt(0)

    logging.info("Client: poll (expecting only halt)")
    got=[]
    while len(got)==0:
        got += connection.poll()
    assert len(got)==1
    assert isinstance(got[0],Halt)
    
    connection.close()

def client_echo4join_test1(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["extA","extB"])
    logging.info("Client: checking incoming edges")
    assert len(connection.incoming_edges)==1
    assert len(connection.incoming_edges[Endpoint("int0:out")])==2
    assert Endpoint("extA:in") in connection.incoming_edges[Endpoint("int0:out")]
    assert Endpoint("extB:in") in connection.incoming_edges[Endpoint("int0:out")]
    connection.close()

def client_echo4join_test2(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["extA","extB"])
    logging.info("Client: run")
    connection.run()
    logging.info("Client: send")
    connection.send([
        MulticastMessage("extA:out")
    ])
    logging.info("Client: poll")
    got=[]
    while len(got)==0:
        got += connection.poll()
    assert len(got)==1
    assert got[0].src.full=="int0:out"

    logging.info("Client: halt(0)")
    connection.halt(0)

    logging.info("Client: poll (expecting only halt)")
    got=[]
    while len(got)==0:
        got += connection.poll()
    assert len(got)==1
    assert isinstance(got[0],Halt)
    
    connection.close()

def client_echo4join_test3(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["extA","extB"])
    logging.info("Client: run")
    connection.run()
    logging.info("Client: send")
    connection.send([
        MulticastMessage("extA:out"),
        MulticastMessage("extB:out")
    ])
    logging.info("Client: poll")
    got=[]
    while len(got)<2:
        got += connection.poll()
    assert len(got)==2
    sources=set([msg.src.full for msg in got])
    assert  sources== set(["int0:out"])

    logging.info("Client: halt(0)")
    connection.halt(0)

    logging.info("Client: poll (expecting only halt)")
    got=[]
    while len(got)==0:
        got += connection.poll()
    assert len(got)==1
    assert isinstance(got[0],Halt)
    
    connection.close()

def client_echo5source_test1(connection:UpwardConnection):
    logging.info("Client: bind")
    connection.bind(["ext0"])
    logging.info("Client: run")
    connection.run()
    logging.info("Client: poll")

    got=[]
    while len(got)<1:
        got += connection.poll()
    assert got[0].src.full=="int0:out"
    
    connection.close()


_tests={
    "echo1" : ["test1", "test2","test3"],
    "echo2parallel" : ["test1", "test3", "test4"],
    "echo3fork" : ["test1","test2"],
    "echo4join" : ["test1", "test2", "test3"],
    "echo5source" : ["test1"]
}

_tests={
    config : {
        test : globals()["client_{}_{}".format(config,test)]
        for test in tests
    }
    for (config,tests) in _tests.items()
}

if __name__=="__main__":
    logging.basicConfig(level=logging.INFO)

    config="echo1"
    test="test1"

    if len(sys.argv)>1:
        config=sys.argv[1]
    if len(sys.argv)>2:
        test=sys.argv[2]

    channel=JSONRawChannelOnStreams(sys.stdin, sys.stdout)
    proxy=JSONClientProxy(channel)
    dc=UpwardConnection(proxy)

    _tests[config][test](dc)


