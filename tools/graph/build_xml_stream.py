from graph.core import *
from graph.save_xml import save_graph_type,toNS
from graph.load_xml import load_graph

from lxml import etree

import os
import io
import sys
import shutil
import tempfile
import time
from typing import *

try:
    import ujson as json
    is_ujson=True
except:
    import json
    is_ujson=False


JSONValue = Dict[str,any]
Value = Union[None,str,JSONValue]

class GraphBuilder:
    """
    Represents a sink for building a single graph instance.
    
    """ 

    def __init__(self):
        pass

    @property
    def can_interleave(self):
        """Are clients allowed to interleave edges and devices? If False then 
            they must add all devices before any edges."""
        return False

    def hint_total_devices(self, n:int):
        """
        Used to hint at the number of devices that will be added. This does not need
        to be exact, and _may_ be used to give info about progress. It is implementation
        defined how this is used.
        """
        pass

    def hint_total_edges(self, n:int):
        """
        Used to hint at the number of edges that will be added. This does not need
        to be exact, and _may_ be used to give info about progress. It is implementation
        defined how this is used.
        """
        pass

    def begin_graph_instance(self, id:str, type:GraphType, *, properties:Value, metadata:Value):
        """Sets the top-level graph instance info. Must only be called once."""
        raise NotImplementedError()

    def add_device_instance(self,
            id:str,
            type:DeviceType,
            *,
            properties : Value = None,
            state : Value = None,
            metadata : Value = None
        ):
        raise NotImplementedError()

    def end_device_instances(self):
        """
        Signal that there are no more device instances to come.
        
        This must be called explicitly before starting the edge instances, as it
        may allow implementations to be slightly faster in each call to
        add_edge_instance (which is probably on the critical path).

        If (and only if) can_interleave is True then it is legal to add
        edge instances before calling end_device_instances, but it is still
        likely to be more efficient to call end_device_instances() as soon as
        possible to avoid additional memory consumption or IO.

        For an interleaved capable builder you are not even required to add
        device instances before edges, but again it is probably more efficient
        if you do.
        """
        raise NotImplementedError()

    def add_edge_instance(self,
            dst_dev_inst:str, dst_dev_pin:str,
            src_dev_inst:str, src_dev_pin:str,
            *,
            properties : Value = None,
            state : Value = None,
            metadata : Value = None,
            send_index:int=None
        ):
        
        raise NotImplementedError()

    def end_graph_instance(self):
        """Marks the graph as finished, should write through to any underlying file or stream."""
        raise NotImplementedError()

class InterleavedGraphBuilderWrapper(GraphBuilder):
    """Creates an interleaved wrapper over graph builders that don't natively
    support interleaving."""

    class _EdgeForward(NamedTuple):
        id : str
        type : DeviceType
        properties : Value
        state : Value
        metadata : Value
        send_index : Optional[int]

    def __init__(self, sink:GraphBuilder):
        self._sink=sink
        self._edge_queue=[] # type: List[_EdgeForward]

    @property
    def can_interleave(self):
        return True

    def add_device_instance(self, id:str, type:DeviceType, *, properties : Value = None, state : Value = None, metadata : Value = None):
        self._sink.add_device_instance(id, type, properties=properties, state=state, metadata=metadata)

    def end_device_instances(self):
        assert self._edge_queue is not None
        for e in self._edge_queue:
            self._sink.add_edge_instance(
                e.dst_dev_inst,e.dst_dev_pin, e.src_dev_inst,e.src_dev_pin,
                properties=e.properties, state=e.state, metadata=e.metadata, send_index=e.send_index
            )
        self._edge_queue=None
        self._sink.end_device_instances()
        
    def add_edge_instance(self, dst_dev_inst:str, dst_dev_pin:str, src_dev_inst:str, src_dev_pin:str, *, properties : Value = None, state : Value = None, metadata : Value = None, send_index:int=None):
        if self._edge_queue is not None:
            self._edge_queue.append(InterleavedGraphBuilderWrapper._EdgeForward(dst_dev_inst, dst_dev_pin, src_dev_inst, src_dev_pin, properties, state, metadata, send_index))
        else:
            self._sink.add_edge_instance(dst_dev_inst, dst_dev_pin, src_dev_inst, src_dev_pin, properties=properties, state=state, metadata=metadata, send_index=send_index)

    def end_graph_instance(self):
        if self._edge_queue is not None:
            self.end_device_instances()
        self._sink.end_graph_instance()
        self._sink=None

def make_interleaved_graph_builder(sink:GraphBuilder) -> GraphBuilder:
    if sink.can_interleave:
        return sink
    return InterleavedGraphBuilderWrapper(sink)


class GraphInstanceBuilder(GraphBuilder):
    @staticmethod
    def _to_object(v:Value) -> Optional[JSONValue]:
        if isinstance(v,str):
            v=v.strip()
            if v=="":
                return None
            return json.loads("{"+v+"}")
        else:
            return v # Will be either JSON object or None

    def __init__(self):
        super().__init__()
        self.graph_type=None # type: Optional[GraphInstance]
        self.graph_instance=None # type: Optional[GraphInstance]
        self._devices_open=True

    def begin_graph_instance(self, id:str, type:GraphType, *, properties:Value = None, metadata:Value = None):
        assert self.graph_type is None
        self.graph_type=type
        self.graph_instance=GraphInstance(id,type,GraphInstanceBuilder._to_object(properties),GraphInstanceBuilder._to_object(metadata))

    def add_device_instance(self,
            id:str,
            type:DeviceType,
            *,
            properties : Value = None,
            state : Value = None,
            metadata : Value = None
        ):
        assert self._devices_open and self.graph_instance
        self.graph_instance.create_device_instance(id,type,properties,state,metadata)

    def end_device_instances(self):
        assert self._devices_open and self.graph_instance
        self._devices_open=False

    def add_edge_instance(self,
            dst_dev_inst:str, dst_dev_pin:str,
            src_dev_inst:str, src_dev_pin:str,
            *,
            properties : Value = None,
            state : Value = None,
            metadata : Value = None,
            send_index:int=None
        ):
        assert self._devices_open is False and self.graph_instance
        self.graph_instance.create_edge_instance(
            self.graph_instance.device_instances[dst_dev_inst], dst_dev_pin,
            self.graph_instance.device_instances[src_dev_inst], src_dev_pin,
            GraphInstanceBuilder._to_object(properties), GraphInstanceBuilder._to_object(metadata),
            send_index=send_index, state=GraphInstanceBuilder._to_object(state) 
        )

    def end_graph_instance(self):
        pass


class XmlV3StreamGraphBuilder:
    """This provides a reasonably fast streaming XML writer that should have O(1) memory
        use (if __debug__==0).

        It does no real checking of the data, even if __debug__!=1
    
        It allows interleaving of devices and edges with constant memory overhead,
        though it will need to perfom O(edges) extra disk IO to support this. It
        is much more efficient in disk IO to generate non-interleaved and/or call
        end_device_instances as soon as possible.
    """

    def _write_value(self, fmt, value:Value) :
        if value is not None:
            if not isinstance(value,str):
                value=json.dumps(value)
                value=value[1:-1]
            if value != "":
                self._dst.write(fmt.format(value))

    def _write_edge_value(self, fmt, value:Value) :
        if value is not None:
            if not isinstance(value,str):
                value=json.dumps(value)
                value=value[1:-1]
            if value != "":
                self._edge_dst.write(fmt.format(value))


    def __init__(self, dst, edge_spool_size=1<<24):
        self._graph_type=None
        self._edge_spool_size = edge_spool_size
        # The main output file
        self._dst=dst
        # will be used for spooling if nesc., and later may switch to pointing at self._dst
        self._edge_dst=tempfile.SpooledTemporaryFile(max_size=self._edge_spool_size, mode='w+t')
        self._devices_complete=False
        self._edge_queue=[] # type: List[Tuple]

        self._progress_check=0
        self._progress_start_time=time.time()
        self._progress_last_time=self._progress_start_time
        self._progress_time_step=1
        self._progress_last_devices=0
        self._progress_last_edges=0

        self._total_devices=0
        self._total_edges=0
        self._hint_total_devices=None
        self._hint_total_edges=None

    def _do_progress(self):
        now=time.time()
        if self._progress_last_time+self._progress_time_step < now:
            global_delta=now - self._progress_start_time
            local_delta=now-self._progress_last_time

            device_progress=""
            if self._hint_total_devices is not None:
                device_progress=f" out of {self._hint_total_devices} expected ({self._total_devices/self._hint_total_devices*100:.1f}%)"
            if self._progress_last_devices!=self._total_devices:
                device_progress+=f" {(self._total_devices - self._progress_last_devices)/local_delta/1000:.1f} KDevice/sec"
                self._progress_last_devices=self._total_devices

            edge_progress=""
            if self._hint_total_edges is not None:
                edge_progress=f" out of {self._hint_total_edges} expected ({self._total_edges/self._hint_total_edges*100:.1f}%)"
            if self._progress_last_edges!=self._total_edges:
                edge_progress+=f" {(self._total_edges - self._progress_last_edges)/local_delta/1000:.1f} KEdge/sec"
                self._progress_last_edges=self._total_edges
            
            sys.stderr.write(f"Gen time = {global_delta:.2f} seconds, {self._total_devices} devices{device_progress}, {self._total_edges} edges{edge_progress}\n")
            self._progress_time_step=min(60, self._progress_time_step*1.5)
            self._progress_last_time=now


    @property
    def can_interleave(self):
        return True

    def hint_total_devices(self, n:int):
        self._hint_total_devices=n

    def hint_total_edges(self, n:int):
        self._hint_total_edges=n

    def begin_graph_instance(self, id:str, type:GraphType, *, properties:Value = None, metadata:Value = None):
        assert self._graph_type is None

        ns="https://poets-project.org/schemas/virtual-graph-schema-v3"
        nsmap = { None : ns }
        root=etree.Element(toNS("p:Graphs"), nsmap=nsmap)
        graphTypeNode=save_graph_type(root,type)
        graphTypeText=etree.tostring(graphTypeNode,pretty_print=True).decode("utf8")

        self._dst.write("<?xml version='1.0'?>\n")
        self._dst.write(f'<Graphs xmlns="{ns}">\n')

        self._dst.write(graphTypeText)

        self._dst.write(f' <GraphInstance id="{id}" graphTypeId="{type.id}">\n')
        self._write_value("<Properties>{}</Properties>\n", properties)
        self._write_value("<MetaData>{}</MetaData>\n", metadata)

        self._dst.write('  <DeviceInstances>\n')
        self._graph_type=type


    def add_device_instance(self, id:str, type:DeviceType, *, properties : Value = None, state : Value = None, metadata : Value = None):
        assert not self._devices_complete
        if properties or state or metadata:
            self._dst.write(f'  <DevI id="{id}" type="{type.id}">\n')
            self._write_value('    <P>{}</P>\n', properties)
            self._write_value('    <S>{}</S>\n', state)
            self._write_value('    <M>{}</M>\n', metadata)
            self._dst.write("  </DevI>\n")
        else:
            self._dst.write(f'  <DevI id="{id}" type="{type.id}"/>\n')

        self._total_devices+=1
        if 0 == (self._total_devices & 0xFFF):
            self._do_progress()

        return id


    def end_device_instances(self):
        """Signal that there are no more device instances to come."""
        assert not self._devices_complete
        self._dst.write("""
  </DeviceInstances>
  <EdgeInstances>
""")
        self._edge_dst.seek(0)
        shutil.copyfileobj(self._edge_dst, self._dst)
        self._edge_dst.close()
        self._edge_dst=self._dst
        self._devices_complete=True

    def add_edge_instance(self, dst_dev_inst:str, dst_dev_pin:str, src_dev_inst:str, src_dev_pin:str, *, properties : Value = None, state : Value = None, metadata : Value = None, send_index:int=None):
        if properties or state or metadata:
            if send_index is not None:
                self._edge_dst.write(f' <EdgeI path="{dst_dev_inst}:{dst_dev_pin}-{src_dev_inst}:{src_dev_pin}" sendIndex="{send_index}">\n')
            else:
                self._edge_dst.write(f' <EdgeI path="{dst_dev_inst}:{dst_dev_pin}-{src_dev_inst}:{src_dev_pin}">\n')
            self._write_edge_value("    <P>{}</P>\n", properties)
            self._write_edge_value("    <S>{}</S>\n", state)
            self._write_edge_value("    <M>{}</M>\n", metadata)
            self._edge_dst.write("  </EdgeI>\n")
        else:
            if send_index is not None:
                self._edge_dst.write(f' <EdgeI path="{dst_dev_inst}:{dst_dev_pin}-{src_dev_inst}:{src_dev_pin}" sendIndex="{send_index}" />\n')
            else:
                self._edge_dst.write(f' <EdgeI path="{dst_dev_inst}:{dst_dev_pin}-{src_dev_inst}:{src_dev_pin}" />\n')

        self._total_edges+=1
        if 0 == (self._total_edges & 0xFFF):
            self._do_progress()
    
    def end_graph_instance(self):
        assert self._dst
        assert self._devices_complete, "User should still call end_devices_complete on interleaved builder"
        self._dst.write("</EdgeInstances>\n</GraphInstance></Graphs>\n")
        self._dst.flush()
        self._dst=None



##################################################################################
## Testing

import unittest

_test_graph_type=GraphType("t1",
    TupleTypedDataSpec("_", [
        ScalarTypedDataSpec("a", "uint32_t"),
        ScalarTypedDataSpec("b", "float"),
    ]),
    { "m1" : 10}
)
_test_mt1=_test_graph_type.add_message_type(MessageType(_test_graph_type, "mt1",
    TupleTypedDataSpec("_", [
        ScalarTypedDataSpec("x", "uint32_t")
    ])
))
_test_dt1=_test_graph_type.add_device_type(DeviceType(_test_graph_type, "dt1",
    TupleTypedDataSpec("_", [
        ScalarTypedDataSpec("x", "uint32_t"),
        ScalarTypedDataSpec("y", "uint32_t")
    ]),
    TupleTypedDataSpec("_", [
        ScalarTypedDataSpec("x", "uint32_t"),
        ScalarTypedDataSpec("y", "uint32_t")
    ])
))
_test_dt1.add_input("i1", _test_mt1,
    is_application=False,
    properties=TupleTypedDataSpec("_", [
        ScalarTypedDataSpec("x", "uint32_t")
    ]),
    state=TupleTypedDataSpec("_", [
        ScalarTypedDataSpec("y", "uint32_t")
    ]),
    metadata={"t":"f"},
    receive_handler=""
)
_test_dt1.add_output("o1", _test_mt1,is_application=False,send_handler="",metadata={"j":6})


def _build_test_graph_v1(sink:GraphBuilder):
    sink.begin_graph_instance("t1", _test_graph_type)
    sink.end_device_instances()
    sink.end_graph_instance()

def _check_test_graph_v1(test, inst:GraphInstance):
    test.assertEqual("t1", inst.id)


def _build_test_graph_v2(sink:GraphBuilder):
    sink.begin_graph_instance("t1", _test_graph_type)
    sink.add_device_instance("di0", _test_dt1)
    sink.add_device_instance("di1", _test_dt1)
    sink.end_device_instances()
    sink.end_graph_instance()

def _check_test_graph_v2(test, inst:GraphInstance):
    test.assertEqual("t1", inst.id)
    di0=inst.device_instances["di0"]
    di1=inst.device_instances["di1"]


def _build_test_graph_v3(sink:GraphBuilder):
    sink.begin_graph_instance("t1", _test_graph_type)
    sink.add_device_instance("di0", _test_dt1)
    sink.add_device_instance("di1", _test_dt1)
    sink.end_device_instances()
    sink.add_edge_instance("di0", "i1", "di1", "o1")
    sink.add_edge_instance("di1", "i1", "di0", "o1", properties={"x":2})
    sink.end_graph_instance()

def _check_test_graph_v3(test, inst:GraphInstance):
    test.assertEqual("t1", inst.id)
    di0=inst.device_instances["di0"]
    di1=inst.device_instances["di1"]
    ei0=inst.edge_instances["di0:i1-di1:o1"]
    ei1=inst.edge_instances["di1:i1-di0:o1"]

def _build_test_graph_v3_OoO(sink:GraphBuilder):
    sink.begin_graph_instance("t1", _test_graph_type)
    sink.add_edge_instance("di0", "i1", "di1", "o1")
    sink.add_edge_instance("di1", "i1", "di0", "o1", properties={"x":2})
    sink.add_device_instance("di0", _test_dt1)
    sink.add_device_instance("di1", _test_dt1)
    sink.end_device_instances()
    sink.end_graph_instance()

_test_v4_n=1<<8

def _build_test_graph_v4(sink:GraphBuilder):
    sink.begin_graph_instance("t1", _test_graph_type)
    for i in range(_test_v4_n):
        sink.add_device_instance(f"di{i}", _test_dt1)
    sink.end_device_instances()
    for i in range(_test_v4_n):
        sink.add_edge_instance(f"di{i}", "i1", f"di{(i+1)%_test_v4_n}", "o1")
    sink.end_graph_instance()

def _build_test_graph_v4_OoO(sink:GraphBuilder):
    sink.begin_graph_instance("t1", _test_graph_type)
    for i in range(_test_v4_n//2):
        sink.add_device_instance(f"di{i}", _test_dt1)
    for i in range(_test_v4_n//2):
        sink.add_edge_instance(f"di{i}", "i1", f"di{(i+1)%_test_v4_n}", "o1")
    for i in range(_test_v4_n//2, _test_v4_n):
        sink.add_device_instance(f"di{i}", _test_dt1)
    for i in range(_test_v4_n//2,_test_v4_n):
        sink.add_edge_instance(f"di{i}", "i1", f"di{(i+1)%_test_v4_n}", "o1")
    sink.end_device_instances()
    sink.end_graph_instance()

def _check_test_graph_v4(test, inst:GraphInstance):
    test.assertEqual("t1", inst.id)
    for i in range(_test_v4_n):
        test.assertIn(f"di{i}", inst.device_instances)
    for i in range(_test_v4_n):
        test.assertIn(f"di{i}:i1-di{(i+1)%_test_v4_n}:o1", inst.edge_instances)
    

class TestGraphInstanceBuilder(unittest.TestCase):
    def test_generate_v1(self):
        builder=GraphInstanceBuilder()
        _build_test_graph_v1(builder)
        _check_test_graph_v1(self, builder.graph_instance)

    def test_generate_v2(self):
        builder=GraphInstanceBuilder()
        _build_test_graph_v2(builder)
        _check_test_graph_v2(self, builder.graph_instance)

    def test_generate_v3(self):
        builder=GraphInstanceBuilder()
        _build_test_graph_v3(builder)
        _check_test_graph_v3(self, builder.graph_instance)

class TestXMLV3StreamGraphBuilder(unittest.TestCase):
    def test_generate_v1(self):
        dst=io.StringIO()
        builder=XmlV3StreamGraphBuilder(dst)
        _build_test_graph_v1(builder)
        dst.seek(0)
        (gt,gi)=load_graph(dst, "")
        _check_test_graph_v1(self, gi)

    def test_generate_v2(self):
        dst=io.StringIO()
        builder=XmlV3StreamGraphBuilder(dst)
        _build_test_graph_v2(builder)
        dst.seek(0)
        (gt,gi)=load_graph(dst, "")
        _check_test_graph_v2(self, gi)

    def test_generate_v3(self):
        dst=io.StringIO()
        builder=XmlV3StreamGraphBuilder(dst)
        _build_test_graph_v3(builder)
        dst.seek(0)
        (gt,gi)=load_graph(dst, "")
        _check_test_graph_v3(self, gi)

    def test_generate_v3_OoO(self):
        dst=io.StringIO()
        builder=XmlV3StreamGraphBuilder(dst)
        _build_test_graph_v3_OoO(builder)
        dst.seek(0)
        (gt,gi)=load_graph(dst, "")
        _check_test_graph_v3(self, gi)


    def test_generate_v4(self):
        dst=io.StringIO()
        builder=XmlV3StreamGraphBuilder(dst)
        _build_test_graph_v4(builder)
        dst.seek(0)
        (gt,gi)=load_graph(dst, "")
        _check_test_graph_v4(self, gi)

    def test_generate_v4_OoO(self):
        dst=io.StringIO()
        builder=XmlV3StreamGraphBuilder(dst)
        _build_test_graph_v4_OoO(builder)
        dst.seek(0)
        (gt,gi)=load_graph(dst, "")
        _check_test_graph_v4(self, gi)


if __name__ == '__main__':
    unittest.main()