from typing import *

class ConfigItem(NamedTuple):
    name:str 
    unit:str
    value:Union[float,int,str]

class ParamInfo(NamedTuple):
    name:str
    unit:str
    defaultVal:Union[int,float]

class Prototype(NamedTuple):
    index : int # Index within prototypes of the same class
    name : str
    model : str
    config : List[ConfigItem]
    params : List[ParamInfo]


class DumbSNNSink:
    def on_begin_network(self, config:List[ConfigItem])):
        pass

    def on_begin_prototypes(self):
        pass

    def on_neuron_prototype(self, prototype:Prototype):
        pass

    def on_synapse_prototype(self, prototype:Prototype):
        pass

    def on_end_prototypes(self):
        pass

    def on_begin_neurons(self):
        pass

    def on_neuron(self,
        neuron_prototype : Prototype,
        id:str
        params:List[float]
    ):
        pass
    
    def on_end_neurons(self):
        pass

    def on_begin_synapses(self):
        pass

    def on_synapse(self,
        synapse_prototype:Prototype,
        dest_id : str,
        source_id : str,
        params:List[float]
    ) =0;

    def on_end_synapses(self):
        pass

    def on_end_network():
        pass