from graph.load_xml import load_graph
from graph.save_xml import save_graph
from graph.core import *
import sys
import os
import random

nEdgeTypes=4
nDeviceTypes=4
nDeviceInstances=50
nEdgeInstances=200

urng=random.random

def make_random_string(prefix):
    return "{}_{}".format(prefix,random.randint(0,2**32))

def make_void(name=None):
    return None

def make_random_int32(name=None,pEmpty=0.5):
    if name==None:
        name=make_random_string("v")
    
    value=None
    if urng()<pEmpty:
        value=random.randint(-2**31,2**31-1)
        
    return Int32Data(name, value)

def make_random_float32(name=None,pEmpty=0.5):
    if name==None:
        name=make_random_string("v")
    
    value=None
    if urng()<pEmpty:
        value=random.random()*2-1
        
    return Float32Data(name, value)

def make_random_bool(name=None,pEmpty=0.5):
    if name==None:
        name=make_random_string("v")
    
    value=None
    if urng()<pEmpty:
        value=bool(random.randint(0,1))
        
    return BoolData(name, value)

def make_random_tuple(name=None,pEmpty=0.5):
    if name==None:
        name=make_random_string("v")
    
    elts=[]
    while urng()<pEmpty or len(elts)<1:
        e=make_random_data()
        if e is not None:
            elts.append(e)
    return TupleData(name, elts)

def make_random_data(name=None):
    creators=[make_void,make_random_int32,make_random_float32,make_random_bool,make_random_tuple]
    return random.choice(creators)(name)



def make_random_instance(proto):
    if proto is None:
        return None
    if proto.is_complete and urng()<0.3:
        return None
    if isinstance(proto,ScalarData):
        _map={Int32Data:make_random_int32,Float32Data:make_random_float32,BoolData:make_random_bool}
        return _map[type(proto)](proto.name)
    elif isinstance(proto,TupleData):
        elts=[]
        for e in proto.elements_by_index:
            if urng()<0.6:
                ee=make_random_instance(e)
                if ee:
                    elts.append(ee)
        return TupleData(proto.name, elts)
    else:
        raise RuntimeError("Unknown data type.")

graph=Graph("random")

edge_types=[]
for i in range(nEdgeTypes):
    message=make_random_data()
    state=make_random_data()
    properties=make_random_data()
    et=EdgeType(graph,"et{}".format(i),message,state,properties )
    graph.add_edge_type(et)
    edge_types.append(et)

device_types=[]
for i in range(nDeviceTypes):
    state=make_random_data()
    properties=make_random_data()
    dt=DeviceType(graph,"dt{}".format(i),state,properties )
    device_types.append(dt)
    
    while urng()<0.7:
        name=make_random_string("p")
        edge_type=random.choice(list( graph.edge_types.values()  ))
        handler="assert(0);"
        dt.add_input(name,edge_type,handler)

    while urng()<0.7:
        name=make_random_string("p")
        edge_type=random.choice(list( graph.edge_types.values()  ))
        handler="assert(0);"
        dt.add_output(name,edge_type,handler)
    
    graph.add_device_type(dt)

device_instances=[]
for i in range(nDeviceInstances):
    dt=random.choice(device_types)
    properties=make_random_instance(dt.properties)
    #sys.stderr.write("proto={}, properties={}".format(dt.properties,properties))
    di=DeviceInstance(graph, make_random_string("di"), dt, properties)
    device_instances.append(di)

    graph.add_device_instance(di)

edge_instances=[]
for i in range(nEdgeInstances):
    src_device=random.choice(device_instances)
    if len(src_device.device_type.outputs)==0:
        continue
    
    src_port=random.choice(list(src_device.device_type.outputs.values()))

    random.shuffle(device_instances)
    
    dst_device=None
    dst_port=None
    for dst_device in device_instances:
        pp=list(dst_device.device_type.inputs.values())
        random.shuffle(pp)
        for p in pp:
            if p.edge_type == src_port.edge_type:
                dst_port=p
                break
        if dst_port:
            break

    if not dst_port:
        continue

    if ( dst_device.id, dst_port.name, src_device.id, src_port.name) in graph.edge_instances:
        continue

    properties=make_random_instance(dst_port.edge_type.properties)
    ei=EdgeInstance(graph, dst_device, dst_port.name, src_device, src_port.name,properties)
    graph.add_edge_instance(ei)
        
    
    

save_graph(graph,sys.stdout)
