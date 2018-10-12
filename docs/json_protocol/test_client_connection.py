from typing import *
from io import BytesIO, StringIO
import json
import multiprocessing

from json_rpc_helpers import *

from server_connection import *
from client_connection import *

import unittest

class TestClient(unittest.TestCase):
    ref_graph_type="gt0"
    ref_graph_instance="gi0"
    ref_incoming_edges={"int0:out":["ext0:in"]}

    class EventsDump(DownwardConnectionEvents):
        def __init__(self):
            self.received=[]
            self.to_send=[MulticastMessage("int0:out")]

        def on_connect(self, connection, owner, owner_cookie, graph_type, graph_instance, owned_devices) :
            print(owned_devices)
            assert owned_devices==set(["ext0","ext1"]), "Got = {}".format(owned_devices)
            return ConnectResults(TestClient.ref_graph_type, TestClient.ref_graph_instance, TestClient.ref_incoming_edges)

        def on_run(self, connection) :
            print("on_run")
            return True

        def on_send(self, connection, messages:List[MulticastMessage]):
            self.received += messages

        def on_poll(self, connection, max_messages):
            if len(self.to_send)>0:
                todo=min(max_messages,len(self.to_send))
                res=self.to_send[:todo]
                self.to_send=self.to_send[todo:]
                return res
            else:
                return []

        def on_halt(self, connection, code, message):
            self.to_send.append(Halt(code,message))

    def server_proc(self, channel:JSONRawChannel, events:DownwardConnectionEvents):
        import time
        pull=JSONServerPull(channel)
        dc=DownwardConnection(pull)
        try:
            while True:
                dc.do_events(events)
                time.sleep(1e-6)
        except EOFError:
            return

    def test_wibble(self):
        (c2s,s2c)=multiprocessing.connection.Pipe()

        server_channel=JSONRawChannelOnPipe(s2c,s2c)
        ev=TestClient.EventsDump()
        try:
            server=multiprocessing.Process(target=TestClient.server_proc, args=(self,server_channel, ev))
            server.start()

            client_channel=JSONRawChannelOnPipe(c2s,c2s)
            proxy=JSONClientProxy(client_channel)
            client=UpwardConnection(proxy)

            client.bind(owned_devices=["ext0","ext1"])

            client.run()

            client.send([ MulticastMessage("ext0:out") ])

            msgs=client.poll()
            self.assertEqual(len(msgs), 1)

        finally:
            client.close()
            server.join()


if __name__ == '__main__':
    unittest.main()