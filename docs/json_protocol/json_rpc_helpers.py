from typing import *
from io import BytesIO
import json
import multiprocessing
import multiprocessing.connection
import threading
import queue
import random

# No recursive type support, this should be enough
JSONObject = Dict[str,any]
JSONArray = List[any]
JSON = Union[JSONObject,JSONArray]


class JSONRawChannel:
    def send(self, value:JSON):
        raise NotImplementedError

    def try_recv(self) -> Optional[JSON]:
        """
        Try to receive an object. If there is currently no message return None.
        If there are no more messages, raise EOFError.
        """
        raise NotImplementedError

    def recv(self) -> JSON:
        """
        Return a object, blocking until one is available. If there are no more
        messages, raise EOFError.
        """
        raise NotImplementedError

    def flush(self):
        raise NotImplementedError

    def close():
        raise NotImplementedError

class JSONRawChannelOnPipe(JSONRawChannel):
    def __init__(self, source:multiprocessing.connection.Connection, sink:multiprocessing.connection.Connection):
        self._source=source
        self._sink=sink

    def send(self, value:JSON):
        self._sink.send(value)

    def try_recv(self) -> Optional[JSON]:
        if self._source.poll(): # This will also return true if other end is closed
            return self._source.recv() # Raise EOFError if the pipe is closed
        else:
            return None
    
    def recv(self) -> JSON:
        return self._source.recv() # Will raise EOFError at the end of stream

    def flush(self):
        pass

    def close(self):
        self._source.close()
        self._sink.close()

class JSONRawChannelOnArrays(JSONRawChannel):
    def __init__(self, input:Union[str,List[JSON]], prob_recv:float=1.0):
        if isinstance(input,str):
            self._input=json.loads(input)
            assert isinstance(self._input, list)
        else:
            self._input=input
        self._output=[]
        self._prob_recv=prob_recv

    def send(self, value:JSON):
        self._output.append(value)

    def try_recv(self) -> Optional[JSON]:
        if len(self._input)==0:
            raise EOFError()
        elif random.random() <= self._prob_recv:
            return self._input.pop(0)
        else:
            return None
    
    def recv(self) -> JSON:
        if len(self._input)==0:
            raise EOFError()
        else:
            return self._input.pop(0)

    def flush(self):
        pass

    def close(self):
        pass

    @property
    def output(self):
        return self._output

class JSONRPCError(Exception):
    def __init__(self, description:str, code:Optional[int]=None, payload=None):
        super().__init__()
        self.description=description
        self.payload=payload
        self.code=code

    def __str__(self):
        return "JSONRPCError( {}, code={}, payload={} ) ".format(self.description, self.code, self.payload)

class JSONClientProxy:
    def __init__(self, channel:JSONRawChannel):
        self.channel=channel
        self.unq=0

    def _next_id(self):
        self.unq+=1
        return "id"+str(self.unq)

    def call(self, method:str, parameters:JSON) -> JSON:
        id=self._next_id()
        msg={
            "jsonrpc":"2.0",
            "method":method,
            "id":id,
            "parameters":parameters
        }
        self.channel.send(msg)
        res=self.channel.recv()
        if not isinstance(res,dict):
            raise JSONRPCError("Response was not an object.", payload=res)
        if res.get("jsonrpc")!="2.0":
            raise JSONRPCError("Response did not have correct jsonrpc field.", payload=res)
        if res.get("id")!=id:
            raise JSONRPCError("Response id {} did not match request id {}.".format(res.get("id"),id), payload=res)
        if "error" in res:
            raise JSONRPCError("Server returned error: "+res.get("message","<No error message>"), code=res["error"] , payload=res)
        if "result" not in res:
            raise JSONRPCError("Response did not have error or result.", res)
        return res["result"]

class JSONServerStub:
    def __init__(self, channel:JSONRawChannel):
        self._channel=channel

    def on_call(self, method:str, parameters:Optional[JSON]) -> Optional[JSON]:
        raise NotImplementedError

    def on_notify(self, method:str, params:Optional[JSON]):
        raise NotImplementedError

    def _error(self, id:str, code:int, message:str):
        return {"jsonrpc":"2.0", "id":id, "error":{ "code":code, "message":message } }
    
    def _dispatch(self, msg:JSON) -> JSON:
        id=isinstance(msg,dict) and msg.get("id") # Try to get the id if at all possible

        if not isinstance(msg,dict):
            return _error(id, -32600, "Request is not an object (batches not supported yet).")
        if msg.get("jsonrpc")!="2.0":
            return _error(id, -32600, "Request is missing jsonrpc header or has wrong value/type.")
        if "method" not in msg:
            return _error(id, -32600, "Request is missing a method field.")
        method=msg["method"]
        if not isinstance(method,str):
            return _error(id, -32600, "Request method is not a string.")
        
        params=msg.get("params")
        res=None
        try:
            if id:
                res=self.on_call(method, params)
            else:
                self.on_notify(method, params)
        except JSONRPCError as error:
            if id is not None:
                res=_error(id, error.code, error.message)
        except Exception as error:
            if id is not None:
                res=_error(id, -32000, str(error))
        
        return res

    def run(self):
        while True:
            try:
                msg=self._channel.recv()
            except EOFError:
                break
            res=self._dispatch(msg)
            if res:
                self._channel.send(res)


class JSONServerPull:
    def __init__(self, channel:JSONRawChannel):
        self._channel=channel
        self._in_progress=set() # type: Set[str]

    def _error(self, id:str, code:int, message:str):
        self._channel.send({"jsonrpc":"2.0", "id":id, "error":{ "code":code, "message":message } })

    def begin(self, valid_methods:Optional[List[str]]=None) -> Tuple[str,Optional[id],Optional[JSON]]:
        return self.try_begin(block=True, valid_methods=valid_methods)

    def try_begin(self, valid_methods:Optional[List[str]]=None, block:bool=False) -> Tuple[Optional[str],Optional[id],Optional[JSON]]:
        while True:
            if block:
                msg=self._channel.recv()
            else:
                msg=self._channel.try_recv()
            if msg==None:
                return (None,None,None)
            print(msg)

            if not isinstance(msg,dict):
                self._error(id, -32600, "Request is not an object (batches not supported yet).")
                continue
            if msg.get("jsonrpc")!="2.0":
                self._error(id, -32600, "Request is missing jsonrpc header or has wrong value/type.")
                continue

            id=isinstance(msg,dict) and msg.get("id") # Try to get the id if at all possible
            if id and id in self._in_progress:
                self._error(id, -32600, "Id is a duplicate for in-progress call.")
                continue

            if "method" not in msg:
                self._error(id, -32600, "Request is missing a method field.")
                continue
            method=msg["method"]
            if not isinstance(method,str):
                self._error(id, -32600, "Request method is not a string.")
                continue
            
            if valid_methods:
                if method not in valid_methods:
                    self._error(id, -32000, "Method {} is not valid, or not currently available.".format(method))

            if id!=None:
                self._in_progress.add(id)
            
            params=msg.get("params", {})
            return (method, id, params)


    def complete(self, id:str, result:Optional[JSON]=None):
        assert id in self._in_progress
        res={"jsonrpc":"2.0","id":id, "result" : result }
        self._channel.send(res)
        self._in_progress.remove(id)

    def error(self, id:str, code:int, message:Optional[str]=None):
        assert id in self._in_progress
        self._error(id, code, message)
        self._in_progress.remove(id)



""" class JSONServerThread:
    class _Server(JSONServerStub):
        def __init__(self, parent:JSONServerThread):
            self.parent=parent

        def on_call(self, method:str, params:Optional[JSON]) -> Optional[JSON]:
            self.parent._request_queue.put( (method, params, False) )
            return self.parent._response_queue.put(  )


    def _server_proc(self):
                

    def __init__(self, channel:JSONRawChannel):
        self._channel=channel
        self._thread=threading.Thread(target=_server_proc, args=(self))
        self._thread.start()

        self._request_queue=queue.Queue()
        self._response_queue=queue.Queue()

        self.method=None       # type: Optional[str]
        self.params=None        # type: Optional[JSON]
        self.is_notification=False # type: bool
        self.is_active=False       # type: bool

    def try_begin(self) -> bool
        assert not self.is_active
        try:
            (self.method,self.params,self.is_notification)=self._request_queue.get_notwait()
            self.is_active=True
            return True
        except Empty:
            return False

    def complete(self, result:Optional[JSON]):
        assert self.is_active
        if self.is_notification:
            self._response_queue.put( result )
        else:
            assert not result
        self.method=None
        self.params=None
        self.is_notification=False
        self.is_active=False """


import unittest

class TestJSONChannelRawChannelOnStreams(unittest.TestCase):
    def test_read0(self):
        chan=JSONRawChannelOnStreams(BytesIO(b''), BytesIO())
        try:
            msg=chan.recv()
            self.assertTrue(False)
        except EOFError:
            pass

    def test_read1(self):
        chan=JSONRawChannelOnStreams(BytesIO(b'{"x":4}'), BytesIO())
        msg=chan.recv()
        self.assertIsInstance(msg,dict)
        self.assertEqual(msg.get("x"), 4)
        try:
            msg=chan.recv()
            self.assertTrue(False)
        except EOFError:
            pass

    def test_read2(self):
        chan=JSONRawChannelOnStreams(BytesIO(b'{"x":4}{}'), BytesIO())
        msg=chan.recv()
        self.assertIsInstance(msg,dict)
        self.assertEqual(msg.get("x"), 4)
        msg=chan.recv()
        self.assertIsInstance(msg,dict)
        self.assertEqual(len(msg), 0)
        try:
            msg=chan.recv()
            self.assertTrue(False)
        except EOFError:
            pass

def server_echo_proc(c2s,s2c):
    channel=JSONRawChannelOnPipe(c2s,s2c)
    while True:
        print("Server waiting")
        msg=channel.recv()
        if msg==None:
            break
        print("Server recevied {}".format(msg))
        channel.send(msg)
    channel.close()#

def _json_equal(a:Union[str,JSON], b:Union[str,JSON]) -> bool:
    if isinstance(a,(str,bytes)):
        a=json.loads(a)
    if isinstance(b,(str,bytes)):
        b=json.loads(b)
    return a==b

def assertJSONEqual(test, a:Union[str,JSON], b:Union[str,JSON]):
    if isinstance(a,(str,bytes)):
        a=json.loads(a)
    if isinstance(b,(str,bytes)):
        b=json.loads(b)
    test.assertEqual(a,b)

    
class TestJSONChannelRawChannelOnPipes(unittest.TestCase):

    def test_call0(self):
        (c2s,s2c)=multiprocessing.Pipe()
        
        server=multiprocessing.Process(target=server_echo_proc, args=(c2s,c2s))
        server.start()
        
        chan=JSONRawChannelOnPipe(s2c, s2c)
        chan.send({})
        got=chan.recv()
        self.assertEqual(got,{})
        chan.send({"x":10})
        got=chan.recv()
        self.assertEqual(got,{"x":10})
        chan.close()

        server.join()

class TestJSONServerStub(unittest.TestCase):
    class XServer(JSONServerStub):
        def __init__(self, channel:JSONRawChannel):
            super().__init__(channel)

        def on_call(self, method:str, msg:JSON):
            if method=="x":
                return {"y":1.0}
            else:
                raise NotImplementedError

    def test_read1(self):
        out=BytesIO()
        chan=JSONRawChannelOnStreams(BytesIO(b'{"jsonrpc":"2.0","method":"x","id":"f"}'), out)
        server=TestJSONServerStub.XServer(chan)
        server.run()
        self.assertEqual(json.loads(out.getvalue()), {"y":1.0})

    def test_read2(self):
        out=BytesIO()
        chan=JSONRawChannelOnStreams(BytesIO(b'{"jsonrpc":"2.0","method":"x","id":"f"}{"jsonrpc":"2.0","method":"x","id":"f"}'), out)
        server=TestJSONServerStub.XServer(chan)
        server.run()
        aa=b'['+out.getvalue().replace(b'}\n{',b'},{')+b']'
        bb=[{"y":1.0},{"y":1.0}]
        print(aa)
        print(bb)
        self.assertTrue(_json_equal(aa,bb))

class TestJSONClientProxy(unittest.TestCase):
    def test_read1(self):
        request=BytesIO()
        response=BytesIO(b'{"jsonrpc":"2.0","id":"id1","result":{"x":10}}')
        chan=JSONRawChannelOnStreams(response, request)
        
        proxy=JSONClientProxy(chan)
        result=proxy.call("f1", {"p1":10})
        self.assertTrue(_json_equal(result, '{"x":10}'))

    def test_read2(self):
        request=BytesIO()
        response=BytesIO(b'{"jsonrpc":"2.0","id":"id1","result":{"x":10}}{"jsonrpc":"2.0","id":"id2","result":{"g":10}}')
        chan=JSONRawChannelOnStreams(response, request)
        
        proxy=JSONClientProxy(chan)
        result=proxy.call("f1", {"p1":10})
        self.assertTrue(_json_equal(result, '{"x":10}'))

        result=proxy.call("f2", {"p3":10})
        self.assertTrue(_json_equal(result, '{"g":10}'))

    def test_error1(self):
        request=BytesIO()
        response=BytesIO(b'{"jsonrpc":"2.0","id":"id1","error":{"code":-32000,"message":"wibble"}}')
        chan=JSONRawChannelOnStreams(response, request)
        
        proxy=JSONClientProxy(chan)
        try:
            result=proxy.call("f1", {"p1":10})
            self.assertTrue(False)
        except JSONRPCError as error:
            pass
        except Exception:
            self.assertTrue(False)
        

class TestJSONServerPull(unittest.TestCase):
    def test_read1(self):
        out=BytesIO()
        chan=JSONRawChannelOnStreams(BytesIO(b'{"jsonrpc":"2.0","method":"x","id":"f"}'), out)
        server=JSONServerPull(chan)
        (method,id,params)=server.begin()
        self.assertEqual(method,"x")
        self.assertEqual(id,"f")
        self.assertEqual(params,None)
        server.complete(id, {"y":1.0})
        result=json.loads(out.getvalue()).get("result")
        assertJSONEqual(self,result, {"y":1.0})

if __name__ == '__main__':
    unittest.main()