#!/usr/bin/env python3

from graph.load_xml import load_graph
from graph.save_xml_stream import save_graph
from graph.core import *
import sys
import os
import random

n=1
if len(sys.argv)>1:
    n=int(sys.argv[1])

nMessageTypes=n
nDeviceTypes=n
nDeviceInstances=n*10
nEdgeInstances=n*40



urng=random.random

def make_random_string(prefix):
    return "{}_{}".format(prefix,random.randint(0,2**32))

def make_void_spec(name=None):
    return None

def make_random_int32_spec(name=None,pEmpty=0.5):
    if name==None:
        name=make_random_string("v")
    
    value=None
    if urng()<pEmpty:
        value=random.randint(-2**31,2**31-1)
        
    return ScalarTypedDataSpec(name, "int32_t", value)

def make_random_float32_spec(name=None,pEmpty=0.5):
    if name==None:
        name=make_random_string("v")
    
    value=None
    if urng()<pEmpty:
        value=random.random()*2-1
        
    return ScalarTypedDataSpec(name, "float", value)


def make_random_tuple_spec(name=None,pEmpty=0.5):
    if urng() < 0.25:
        return None
    
    if name==None:
        name=make_random_string("v")
    
    elts=[]
    while urng()<pEmpty or len(elts)<1:
        e=make_random_data_spec()
        if e is not None:
            elts.append(e)
    return TupleTypedDataSpec(name, elts)

def make_random_data_spec(name=None):
    creators=[make_void_spec,make_random_int32_spec,make_random_float32_spec,make_random_tuple_spec]
    return random.choice(creators)(name)



def make_random_instance(proto):
    if proto is None:
        return None
    if urng()<0.3:
        return None
    if isinstance(proto,ScalarTypedDataSpec):
        value=None
        if proto.type=="int32_t":
            value=random.randint(-2**31,2**31-1)
        elif proto.type=="float":
            value=random.random()*2-1
        elif proto.type=="bool":
            value=bool(random.randint(0,1))
        else:
            assert False, "Unknown type"
        return value
    elif isinstance(proto,TupleTypedDataSpec):
        elts={}
        for e in proto.elements_by_index:
            if urng()<0.6:
                ee=make_random_instance(e)
                if ee:
                    elts[e.name]=ee
        return elts
    elif isinstance(proto,ArrayTypedDataSpec):
        return [make_random_instance(proto.type) for i in range(proto.length)]
    else:
        raise RuntimeError(f"Unknown data type {type(proto)}.")

graphType=GraphType("random", make_random_tuple_spec(), None)
graphProperties=make_random_instance(graphType.properties)
graphInstance=GraphInstance("random", graphType)

message_types=[]
for i in range(nMessageTypes):
    message=make_random_tuple_spec()
    et=MessageType(graphType,"et{}".format(i),message)
    graphType.add_message_type(et)
    message_types.append(et)

device_types=[]
for i in range(nDeviceTypes):
    state=make_random_tuple_spec()
    properties=make_random_tuple_spec()
    dt=DeviceType(graphType,"dt{}".format(i),properties, state )
    device_types.append(dt)
    
    while urng()<0.7:
        name=make_random_string("p")
        edge_type=random.choice(list( graphType.message_types.values()  ))
        properties=make_random_tuple_spec()
        state=make_random_tuple_spec()
        handler="assert(0);"
        dt.add_input(name,edge_type, False, properties, state, None, handler)

    while urng()<0.7:
        name=make_random_string("p")
        edge_type=random.choice(list( graphType.message_types.values()  ))
        handler="assert(0);"
        dt.add_output(name,edge_type,False, None, handler)
    
    graphType.add_device_type(dt)

device_instances=[]
for i in range(nDeviceInstances):
    dt=random.choice(device_types)
    properties=make_random_instance(dt.properties)
    state=make_random_instance(dt.state)
    #sys.stderr.write("proto={}, properties={}".format(dt.properties,properties))
    di=DeviceInstance(graphInstance, make_random_string("di"), dt, properties, state)
    device_instances.append(di)

    graphInstance.add_device_instance(di)

edge_instances=[]
for i in range(nEdgeInstances):
    src_device=random.choice(device_instances)
    if len(src_device.device_type.outputs)==0:
        continue
    
    src_pin=random.choice(list(src_device.device_type.outputs.values()))

    random.shuffle(device_instances)
    
    dst_device=None
    dst_pin=None
    for dst_device in device_instances:
        pp=list(dst_device.device_type.inputs.values())
        random.shuffle(pp)
        for p in pp:
            if p.message_type == src_pin.message_type:
                dst_pin=p
                break
        if dst_pin:
            break

    if not dst_pin:
        continue

    if f"{dst_device.id}:{dst_pin.name}-{src_device.id}:{src_pin.name}" in graphInstance.edge_instances:
        continue

    properties=make_random_instance(dst_pin.properties)
    ei=EdgeInstance(graphInstance, dst_device, dst_pin.name, src_device, src_pin.name,properties)
    graphInstance.add_edge_instance(ei)
        
    
    

save_graph(graphInstance,sys.stdout)
