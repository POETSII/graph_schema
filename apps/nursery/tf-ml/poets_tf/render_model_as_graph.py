import sys
import os
import math

import tensorflow as tf
import numpy as np
import tensorflow.keras as keras

import graph
from graph.load_xml import load_graph_types_and_instances
from graph.build_xml_stream import make_xml_stream_builder

import os
xml_dir=os.path.dirname(os.path.realpath(__file__))

xml_ann_lazy_path=xml_dir+"/../ann_lazy.xml"
(ann_lazy_graph_type,_)=load_graph_types_and_instances(xml_ann_lazy_path,xml_ann_lazy_path)

graphType=ann_lazy_graph_type["ann_lazy"]
n_input=graphType.device_types["n_input"]
n_relu=graphType.device_types["n_relu"]
n_max_d4=graphType.device_types["n_max_d4"]


def render_model_as_graph(instName, layers,input_shape):
    sink=make_xml_stream_builder(sys.stdout,require_interleave=True)
    assert sink.can_interleave

    properties={"output_eps":0}

    sink.begin_graph_instance(instName, graphType, properties=properties)

    assert len(input_shape)==2
    curr=np.array([
        [
            sink.add_device_instance(f"i_{x}_{y}", n_input, properties={"x":x,"y":y})
            for x in range(input_shape[1])
        ] for y in range(input_shape[0])
    ], dtype=object)

    for layer_index in range(len(layers)):
        l=layers[layer_index]
        si=l.input_shape
        assert si[0]==None
        si=si[1:]
        so=l.output_shape
        assert so[0] is None
        so=so[1:]

        if isinstance(l,keras.layers.Reshape):
            curr=np.reshape(curr, so)

        elif isinstance(l,keras.layers.Conv2D):
            assert l.activation==keras.activations.relu, f"Activation={l.activation}"
            assert l.use_bias==True
            assert l.padding=="valid"
            assert l.data_format=="channels_last"
            assert l.strides==(1,1)
            assert l.dilation_rate==(1,1)
            assert l.groups==1
            kernel_size=l.kernel_size
            filters=l.filters
            weights=l.weights[0].numpy()
            print(weights.shape)
            assert weights.shape==kernel_size+(1,filters), f"Got={weights.shape}, Exp={kernel_size+(1,filters)}"
            bias=l.weights[1].numpy()
            assert bias.shape==(filters,)
            assert so==(si[0]-kernel_size[0]+1,si[1]-kernel_size[1]+1,filters)
            assert len(si)==3 and si[2]==1, f"Input assumed to be pure 2d mono-chrome, shape={si}"
            working=np.zeros(shape=so,dtype=object)
            for yo in range(0,so[1]):
                for xo in range(0,so[0]):
                    for fo in range(0,filters):
                        # TODO : should prune nodes which are purely constant or not used by any other node
                        props={"bias":float(bias[fo])}
                        dst=sink.add_device_instance(f"l{layer_index}_relu_{xo}_{yo}_{fo}", n_relu, properties=props )
                        working[yo,xo,fo]=dst
                        src=[]
                        for yi in range(0,kernel_size[1]):
                            for xi in range(0,kernel_size[0]):
                                w=weights[xi,yi,0,fo]
                                node=curr[xo+xi,yo+yi,0]
                                if w !=0 and node!=None :
                                    sink.add_edge_instance(dst,"vin", node, "vout", properties={"w":float(w)})

            curr=working
                        
        elif isinstance(l,keras.layers.MaxPooling2D):
            pool_size=l.pool_size
            assert so[0]==si[0] / pool_size[0]
            assert so[1]==si[1] / pool_size[1]
            assert so[2]==si[2]
            assert len(so)==3
            assert len(si)==3
            working=np.full(shape=so,fill_value=None,dtype=object)
            for yo in range(0,so[1]):
                for xo in range(0,so[0]):
                    for fo in range(0,so[2]):
                        # TODO : optimise out unused nodes
                        props={}
                        dst=sink.add_device_instance(f"l{layer_index}_max_d4_{xo}_{yo}_{fo}", n_max_d4, properties=props )
                        working[yo,xo,fo]=dst

                        src=[]
                        for yi in range(0,pool_size[1]):
                            for xi in range(0,pool_size[0]):
                                sink.add_edge_instance(dst,"vin", curr[xo*pool_size[0]+xi,yo*pool_size[1]+yi,fo], "vout" )
                        assert len(src) <= 4
            curr=working

        elif isinstance(l,keras.layers.Flatten):
            assert len(so)==1
            curr=np.reshape(curr, so)

        elif isinstance(l,keras.layers.Dense):
            assert l.activation in [keras.activations.relu,keras.activations.softmax], f"Activation={l.activation}"
            assert l.use_bias==True
            units=l.units

            assert len(si)==1
            n_in=si[0]
            assert len(so)==1
            assert units==so[0]

            weights=l.weights[0].numpy()
            bias=l.weights[1].numpy()
            assert weights.shape==(n_in, units), f"Weights.shape={weights.shape}, exp={(n_in, units)}"
            assert bias.shape==(units,)

            props_base={}
            if l.activation==keras.activations.relu:
                pass
            elif l.activation==keras.activations.softmax:
                props_base["act"]=1
            else:
                assert False
            
            working=np.full(shape=so,fill_value=None,dtype=object)
            for o in range(units):
                props={"bias":float(bias[o])}.update(props_base)
                dst=sink.add_device_instance(f"l{layer_index}_relu_{o}", n_relu, properties=props )
                working[o]=dst
                for i in range(n_in):
                    w=weights[i,o]
                    node=curr[i]
                    if w !=0 and node!=None :
                        sink.add_edge_instance(dst,"vin", node, "vout", properties={"w":float(w)})

            curr=working

    sink.end_device_instances()
    sink.end_graph_instance()
