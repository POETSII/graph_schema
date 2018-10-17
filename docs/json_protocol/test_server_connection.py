from typing import *
from io import BytesIO, StringIO
import json
import multiprocessing

from json_rpc_helpers import *

from server_connection import *

import unittest

class TestStates(unittest.TestCase):
    ref_graph_type="gt0"
    ref_graph_instance="gi0"
    ref_incoming_edges={"int0:out":["ext0:in"]}

    class EventsDump(DownwardConnectionEvents):
        def __init__(self):
            self.received=[]
            self.to_send=[]

        def on_connect(self, connection, owner, owner_cookie, graph_type, graph_instance, owned_devices) :
            print(owned_devices)
            assert owned_devices==set(["ext0"])
            return ConnectResults(TestStates.ref_graph_type, TestStates.ref_graph_instance, TestStates.ref_incoming_edges)

        def on_run(self, connection) :
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

    def test_connect(self):
        msg1=MulticastMessage("ext0:out")
        msg2=MulticastMessage("ext0:out", [10])
        msg3=MulticastMessage("ext0:out", [11])
        input=[
            {"jsonrpc":"2.0","id":"id0","method":"bind", "params":{ "owned_devices":["ext0"] }},
            {"jsonrpc":"2.0","id":"id1","method":"run"},
            {"jsonrpc":"2.0","id":"id2","method":"send", "params":{ "messages":[ msg1.toJSON() ]  }},
            {"jsonrpc":"2.0","id":"id3","method":"send", "params":{ "messages":[ msg2.toJSON(), msg3.toJSON() ]  }},
            {"jsonrpc":"2.0","id":"id4","method":"poll", "params":{ "max_messages":1} },
            {"jsonrpc":"2.0","id":"id5","method":"poll" },
            {"jsonrpc":"2.0","id":"id6","method":"poll" },
            {"jsonrpc":"2.0","id":"id7","method":"halt", "params":{"code":10} },
            {"jsonrpc":"2.0","id":"id8","method":"poll"},
            {"jsonrpc":"2.0","id":"id9","method":"poll"}
        ]
        channel=JSONRawChannelOnArrays(input)
        connection=JSONServerPull(channel)

        ev=TestStates.EventsDump()
        ev.to_send += [msg1,msg2,msg3]
        dc=DownwardConnection(connection)
        try:
            while dc.do_events(ev):
                pass
        except EOFError:
            pass
        
        self.assertEqual(len(channel.output), 10)

        bind_response=channel.output[0]
        print(bind_response)
        self.assertEqual(bind_response["result"]["graph_type"], self.ref_graph_type)
        self.assertEqual(bind_response["result"]["graph_instance"], self.ref_graph_instance)
        self.assertEqual(bind_response["result"]["incoming_edges"], self.ref_incoming_edges)
            
        run_response=channel.output[1]

        send_response=channel.output[2]
        self.assertEqual(ev.received[0],msg1)

        send_response=channel.output[3]
        self.assertEqual(ev.received[1],msg2)
        self.assertEqual(ev.received[2],msg3)

        poll_result=channel.output[4]["result"]
        self.assertEqual(len(poll_result["events"]),1)

        poll_result=channel.output[5]["result"]
        self.assertEqual(len(poll_result["events"]),2)

        poll_result=channel.output[6]["result"]
        self.assertEqual(len(poll_result["events"]),0)

        halt_result=channel.output[7]["result"]

        poll_result=channel.output[8]["result"]
        self.assertEqual(len(poll_result["events"]),1)
        self.assertTrue( contains_halt(json_objects_to_events(poll_result["events"]) ) )

        poll_error=channel.output[9]["error"]
        self.assertEqual(poll_error["code"], ProtocolError.ConnectionFinished )


class TestStates2(unittest.TestCase):
    ref_graph_type="gt0"
    ref_graph_instance="gi0"
    ref_incoming_edges={"int0:out":["ext0:in"]}

    class EventsDump(DownwardConnectionEvents):
        def __init__(self):
            self.received=[]
            self.to_send=[]
            self.enable_run=False

        def on_connect(self, connection, owner, owner_cookie, graph_type, graph_instance, owned_devices) :
            print(owned_devices)
            assert owned_devices==set(["ext0","ext1"])
            return ConnectResults(TestStates.ref_graph_type, TestStates.ref_graph_instance, TestStates.ref_incoming_edges)

        def on_run(self, connection) :
            return self.enable_run

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

    def test_connect(self):
        msg1=MulticastMessage("ext0:out")
        msg2=MulticastMessage("ext0:out", [10])
        msg3=MulticastMessage("ext0:out", [11])

        (c2s,s2c)=multiprocessing.connection.Pipe()

        channel=JSONRawChannelOnPipe(s2c,s2c)
        connection=JSONServerPull(channel)

        ev=TestStates2.EventsDump()
        dc=DownwardConnection(connection)

        def run():
            while dc.do_events(ev):
                pass

        run()

        # Standard bind
        c2s.send({"jsonrpc":"2.0","id":"id0","method":"bind", "params":{ "owned_devices":["ext0","ext1"] }})
        run()
        bind_response=c2s.recv()
        print(bind_response)
        self.assertEqual(bind_response["result"]["graph_type"], self.ref_graph_type)
        self.assertEqual(bind_response["result"]["graph_instance"], self.ref_graph_instance)
        self.assertEqual(bind_response["result"]["incoming_edges"], self.ref_incoming_edges)

        # Run
        c2s.send({"jsonrpc":"2.0","id":"id1","method":"run"})
        run()
        self.assertFalse(c2s.poll())
        ev.enable_run=True
        run()
        self.assertTrue(c2s.poll())
        run_result=c2s.recv()["result"]

        # Send one message
        c2s.send({"jsonrpc":"2.0","id":"id2","method":"send", "params":{ "messages":[ msg1.toJSON() ]  }})
        run()
        self.assertTrue(c2s.poll())
        run_result=c2s.recv()["result"]
        self.assertEqual(ev.received[0], msg1)

        # Poll for messages, expect none
        c2s.send({"jsonrpc":"2.0","id":"id4","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 0)

        # Inject a message, poll for messages, expect one
        ev.to_send.append(msg2)
        c2s.send({"jsonrpc":"2.0","id":"id5","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 1)
        self.assertEqual( poll_result["events"][0], msg2.toJSON())

        # Poll for messages, expect none
        c2s.send({"jsonrpc":"2.0","id":"id10","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 0)

        # Poll for messages async, expect no return yet
        c2s.send({"jsonrpc":"2.0","id":"idAsync","method":"poll", "params":{ "async":True, "max_messages":1} })
        run()
        self.assertFalse(c2s.poll())

        # Overlap send with outstanding poll
        c2s.send({"jsonrpc":"2.0","id":"id20","method":"send", "params":{ "messages":[ msg3.toJSON() ]  }})
        run()
        self.assertTrue(c2s.poll())
        run_result=c2s.recv()["result"]
        self.assertEqual(ev.received[1], msg3)
        
        # Synchronous poll, expect response with zero messages
        c2s.send({"jsonrpc":"2.0","id":"id21","method":"poll", "params":{ "max_messages":1} })
        run()
        self.assertTrue(c2s.poll())
        poll_result=c2s.recv()["result"]
        self.assertEqual(len(poll_result["events"]), 0)

        # Inject message, expect outstanding response to complete
        ev.to_send.append(msg2)
        run()
        self.assertTrue(c2s.poll())
        poll_response=c2s.recv()
        self.assertEqual(poll_response["id"], "idAsync")

        # Poll for messages async, expect no return yet
        c2s.send({"jsonrpc":"2.0","id":"idAsync2","method":"poll", "params":{ "async":True, "max_messages":1} })
        run()
        self.assertFalse(c2s.poll())

        # Queue up another async
        c2s.send({"jsonrpc":"2.0","id":"idAsync3","method":"poll", "params":{ "async":True, "max_messages":1} })
        run()
        self.assertFalse(c2s.poll())

        # Inject halt, expect halt to complete first
        c2s.send({"jsonrpc":"2.0","id":"id100","method":"halt", "params":{ "code":0} })
        run()
        halt_response=c2s.recv()
        self.assertEqual(halt_response["id"],"id100")

        # First async poll should complete with halt message
        self.assertTrue(c2s.poll())
        poll_response=c2s.recv()
        self.assertEqual(poll_response["id"], "idAsync2")
        self.assertEqual(len(poll_response["result"]["events"]), 1)
        self.assertEqual(poll_response["result"]["events"][0]["type"], "halt")
        self.assertEqual(poll_response["result"]["events"][0]["code"], 0)

        # Second async poll completes with zero messages
        self.assertTrue(c2s.poll())
        poll_response=c2s.recv()
        self.assertEqual(poll_response["id"], "idAsync3")
        self.assertEqual(len(poll_response["result"]["events"]), 0)

if __name__ == '__main__':
    unittest.main()