import typing
from typing import Any, Dict, List

import sys
import collections

from contextlib import contextmanager

from mini_op2.framework.core import *
from mini_op2.framework.control_flow import Statement, ParFor
from mini_op2.framework.system import SystemSpecification, SystemInstance

from graph.core import GraphInstance, DeviceInstance
from graph.save_xml_stream import save_graph

import mini_op2.framework.sync_compiler
from mini_op2.framework.sync_compiler import sync_compiler, find_kernels_in_code, InvocationContext

scalar_uint32=DataType(shape=(),dtype=numpy.uint32)

def connect_invocation_indirect_reads(
    graph:GraphInstance,
    inst:SystemInstance,
    instances_by_set:Dict[Set,List[DeviceInstance]],
    gi:DeviceInstance, 
    ctxt:InvocationContext
):
    graph_type=graph.graph_type
    invocation=ctxt.stat.id
    
    for (index,arg) in ctxt.indirect_reads:
        dst_type=graph_type.device_types["set_{}".format(arg.iter_set.id)]
        dst_pin=dst_type.pins["{invocation}_arg{index}_read_recv".format(invocation=invocation,index=index)]
        src_type=graph_type.device_types["set_{}".format(arg.to_set.id)]
        src_pin_name="{invocation}_dat_{dat}_read_send".format(invocation=invocation,dat=arg.dat.id)
        if src_pin_name not in src_type.pins:
            raise RuntimeError("Couldn't find pin {} in {}. Pins are {}".format(
                src_pin_name, src_type.id,
                ",".join(src_type.pins)
            ))
        src_pin=src_type.pins[src_pin_name]
      
        iter_instances=instances_by_set[arg.iter_set]
        to_instances=instances_by_set[arg.to_set]
        mapping=inst.maps[arg.map]
        
        assert arg.index>=0
        for (dst_index,dst_instance) in enumerate(iter_instances):
            src_index=mapping[dst_index][arg.index]
            src_instance=to_instances[src_index]
            graph.create_edge_instance(dst_instance,dst_pin,src_instance,src_pin)


def connect_invocation_indirect_writes(
    graph:GraphInstance,
    inst:SystemInstance,
    instances_by_set:Dict[Set,List[DeviceInstance]],
    gi:DeviceInstance, 
    ctxt:InvocationContext
):
    graph_type=graph.graph_type
    invocation=ctxt.stat.id
    
    write_recv_counts=collections.Counter()
    for di in instances_by_set[ctxt.stat.iter_set]:
        write_recv_counts[di]+=1 # Everything on iteration set must execute, which is tracked as virtual indirect write
    
    for (index,arg) in ctxt.indirect_writes:
        dst_type=graph_type.device_types["set_{}".format(arg.to_set.id)]
        dst_pin=dst_type.pins["{invocation}_arg{index}_write_recv".format(invocation=invocation,index=index)]
        src_type=graph_type.device_types["set_{}".format(arg.iter_set.id)]
        src_pin_name="{invocation}_arg{index}_write_send".format(invocation=invocation,index=index)
        src_pin=src_type.pins[src_pin_name]
      
        iter_instances=instances_by_set[arg.iter_set]
        to_instances=instances_by_set[arg.to_set]
        mapping=inst.maps[arg.map]
        assert arg.index>=0
        for (src_index,src_instance) in enumerate(iter_instances):
            dst_index=mapping[src_index][arg.index]
            dst_instance=to_instances[dst_index]
            write_recv_counts[dst_instance] += 1
            graph.create_edge_instance(dst_instance,dst_pin,src_instance,src_pin)
    
    write_recv_total_name="{invocation}_write_recv_total".format(invocation=invocation)
    for (di,count) in write_recv_counts.items():
        di.set_property(write_recv_total_name, count)


def connect_invocation(
    graph:GraphInstance,
    inst:SystemInstance,
    instances_by_set:Dict[Set,List[DeviceInstance]],
    gi:DeviceInstance, 
    ctxt:InvocationContext
):
    graph_type=graph.graph_type
    
    invocation=ctxt.stat.id
    all_involved=ctxt.get_all_involved_sets()
    total_responding_devices=0
    for s in all_involved:
        begin="{}_begin".format(invocation)
        end="{}_end".format(invocation)
        for di in instances_by_set[s]:
            graph.create_edge_instance(di,begin,gi,begin)
            graph.create_edge_instance(gi,end,di,end)
            total_responding_devices+=1
    
    graph.set_property("{invocation}_total_responding_devices".format(invocation=invocation), total_responding_devices)

    connect_invocation_indirect_reads(graph, inst, instances_by_set, gi, ctxt)
    connect_invocation_indirect_writes(graph, inst, instances_by_set, gi, ctxt)
    


def build_graph(globalDeviceType:str, spec:SystemSpecification, inst:SystemInstance, code:Statement): 
    graphTypeBuilder=sync_compiler(spec, code)
    graphType=graphTypeBuilder.build()
    
    graph=GraphInstance("op2", graphType)
    
    gi=graph.create_device_instance("global", globalDeviceType)
    
    instancesBySet={} # type: Dict[Set,List[DeviceInstance]]
    instancesByName={} # type: Dict[str,DeviceInstance]
    for set in spec.sets.values():
        instances=instancesBySet.setdefault(set,[])
        dt=graphType.device_types["set_{}".format(set.id)]
        for i in range(inst.sets[set]):
            name="{}_{}".format(set.id,i)
            di=graph.create_device_instance(name, dt)
            instances.append(di)
            instancesByName[name]=di
    
    for (dat,vals) in inst.dats.items():
        init_dat_name="init_dat_{}".format(dat.id)
        for (index,di) in enumerate(instancesBySet[dat.set]):
            sys.stderr.write("{}, {}, {}\n".format(dat.set.id, dat.id, index))
            di.set_property(init_dat_name, vals[index].tolist())
            
    
    kernels=find_kernels_in_code(code)    
    for (i,stat) in enumerate(kernels):
        ctxt=InvocationContext(spec, stat)
        connect_invocation(graph,inst,instancesBySet,gi,ctxt)
    
    return graph


if __name__=="__main__":
    logging.basicConfig(level=4)
    
    import mini_op2.apps.airfoil
    import mini_op2.apps.aero
    
    (spec,inst,code)=mini_op2.framework.sync_compiler.load_model(sys.argv)
    
    graph=build_graph("tester", spec, inst, code)
    
    save_graph(graph, sys.stdout)
