from typing import *
import string
import json
import queue
import threading

# No recursive type support, this should be enough
JSONObject = Dict[str,any]
JSONArray = List[any]
JSON = Union[JSONObject,JSONArray]


class JSONChannel:
    def send(self, value:JSON):
        raise NotImplementedError

    def recv(self) -> Optional[JSON]:
        raise NotImplementedError

    def flush(self):
        raise NotImplementedError

    def close():
        raise NotImplementedError

class JSONSplitter:
    """This is able to seperate JSON fragments in linear time, though with a high constant.
        "splitstream" would be better, but doesn't seem to allow for progressively adding chunks.
    """

    STATE_BETWEEN=0
    STATE_NORMAL=1
    STATE_STRING=2
    STATE_ESCAPE=3

    def __init__(self):
        self._level=0
        self._state=JSONSplitter.STATE_BETWEEN
        self._buffers=[] # type: List[str]

        self._whitespace=set(string.whitespace)

    def in_progress(self):
        return self._state != JSONSplitter.STATE_BETWEEN

    def add(self, buffer:str) -> Sequence[JSON]:
        print("add({})".format(buffer))

        end=self._split(buffer, 0) # Does the buffer contain or complete a fragment?
        if end==None:
            self._buffers.append(buffer)
            return []

        res=[] # type: List[JSON]

        if len(self._buffers)>0:
            plen=len(buffer)
            self._buffers.append(buffer)
            buffer="".join(self._buffers)
            self._buffers=[]
            end += len(buffer)-plen

        begin=0
        total=len(buffer) # Size of current buffer
        while end:
            print(buffer[begin:end])
            res.append( json.loads(buffer[begin:end]) )
            begin=end
            if begin >= total:
                break
            end=self._split(buffer, begin) # Look for next split point

        if begin < total:
            self._buffers=[ buffer[begin:] ]
        
        return res
                
    
    def _split(self, buffer:str, start:int) -> Optional[int] :
        STATE_BETWEEN=JSONSplitter.STATE_BETWEEN
        STATE_NORMAL=JSONSplitter.STATE_NORMAL
        STATE_STRING=JSONSplitter.STATE_STRING
        STATE_ESCAPE=JSONSplitter.STATE_ESCAPE

        res=None
        _state=self._state
        _level=self._level
        for i in range(start, len(buffer)):
            ch=buffer[i]
            print("  i={}, ch={}, state={}".format(i, ch, _state))
            if _state==STATE_ESCAPE:
                _state=STATE_STRING # We don't do error checkign on allowed chars here
            elif _state==STATE_STRING:
                if ch=='"':
                    _state=STATE_NORMAL
                elif ch=='\\':
                    _state=STATE_ESCAPE
                else:
                    pass
            elif _state==STATE_NORMAL:
                if ch=='[' or ch=='{':
                    _level+=1
                elif ch==']' or ch=='}':
                    _level-=1
                    if _level==0:
                        _state=STATE_BETWEEN
                        res=i+1  # The only exit point with an actual object
                        break
                elif ch=='"':
                    _state=STATE_STRING
                else:
                    pass
            elif _state==STATE_BETWEEN:
                assert _level == 0
                if ch=='{' or ch=='[':
                    _level+=1
                    _state=STATE_NORMAL
                elif ch in self._whitespace:
                    pass
                else:
                    raise RuntimeError("Unexpected character '{}' at top-level of JSON stream.".format((ch)))
        self._state=_state
        self._level=_level
        return res


class JSONSourceOnBufferedStream:
    def _worker(self):
        try:
            splitter=JSONSplitter()
            while True:
                # Stupid char by char process, as otherwise we will block
                # Can't work out a better method without all kinds of wierd polling stuff
                ch=self._source.read(1)
                if ch==None:
                    break
                for j in splitter.add(ch):
                    self._queue.put(j)
            if splitter.in_progress:
                raise RuntimeError("JSON stream finished mid-object.")
            self._queue.put(None)
        except Exception as e:
            self._queue.put(e)
        finally:
            self._running=False

    def __init__(self, source:TextIO):
        self._source=source
        self._queue=queue.Queue()
        self._thread=threading.Thread(target=_worker, args=(self,))
        self._running=True
        self._thread.start()

    def try_recv(self) -> Optional[JSON] :
        """Attempt to get a json object from the channel. Return None if no object ready. Raises EOFError when no more objects"""
        if self._eof:
            raise EOFError()
        try:
            v=self._queue.get_nowait()
            if isinstance(v,Exception):
                raise v
            if v is None:
                raise EOFError()
            return v
        except Empty:
            if not self._running:
                raise EOFError()
            return None



import unittest

class TestJSONSplitter(unittest.TestCase):
    def test_split1(self):
        splitter=JSONSplitter()
        res=splitter.add('{}')
        self.assertEqual(res, [{}])

    def test_split2(self):
        splitter=JSONSplitter()
        res=splitter.add('{')
        self.assertEqual(res, [])
        res=splitter.add('}')
        self.assertEqual(res, [{}])

    def test_split3(self):
        splitter=JSONSplitter()
        res=splitter.add('{')
        self.assertEqual(res, [])
        res=splitter.add('"x":10')
        self.assertEqual(res, [])
        res=splitter.add('}')
        self.assertEqual(res, [{"x":10}])

    def test_splitN(self):
        input='{ }[] {"x":10}["\\\""]{"x":{}}'
        output=[{},[],{"x":10},["\""],{"x":{}}]

        for i in range(len(input)):
            a=input[:i]
            b=input[i:]
            splitter=JSONSplitter()
            got=splitter.add(a)
            got+=splitter.add(b)
            self.assertEqual(got,output)

    def test_splitS(self):
        input='{ }[] {"x":10}["\\\""]{"x":{}, "Y":[10]}'
        output=[{},[],{"x":10},["\""],{"x":{},"Y":[10]}]

        splitter=JSONSplitter()

        got=[]
        for c in input:
            got+=splitter.add(c)
        
        self.assertEqual(got,output)

if __name__ == '__main__':
    unittest.main()

