#include "../include/neuron.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "graph_persist_dom_reader.hpp"
#include "graph_persist_sax_writer.hpp"

#include <regex>
#include <fstream>
#include <iostream>

#include "robin_hood.hpp"

struct HardwareIdleSink
    : DumbSNNSink
{
    std::string algorithm_name;

    float dt;
    uint32_t max_steps;

    GraphTypePtr graphType;
    std::shared_ptr<GraphLoadEvents> writer;

    std::shared_ptr<NeuronModel> neuron_model;
    std::vector<neuron_factory_functor_t> neuron_factories;

    using synapse_factory_functor_t = std::function<int32_t(const prototype &p, unsigned nParams, const double *pParams)>;

    std::vector<synapse_factory_functor_t> synapse_factories;

    DeviceTypePtr neuronDeviceType;
    InputPinPtr neuronSpikeInPin;
    OutputPinPtr neuronSpikeOutPin;
    OutputPinPtr neuronHashOutPin;

    TypedDataPtr synapseProperties;

    bool connect_external_hash=true;
    DeviceTypePtr externalDeviceType;
    InputPinPtr externalHashInPin;
    uint64_t external_sink_index;

    struct neuron_info
    {
        std::string id;
        uint32_t linear_index;
        uint64_t sink_index;
        std::shared_ptr<Neuron> neuron;
    };
    using neuron_map_t = robin_hood::unordered_node_map<std::string,neuron_info> ;
    neuron_map_t m_neurons;

    uint64_t gid=-1;

    void on_begin_network(
        const std::vector<config_item> &config
    ) {
        auto c=get_config_string(config, "calc_type","type");
        if(c!="float_ftz_daz"){
            throw std::runtime_error("calc_type doesn't match. Want='float_ftz_daz', got="+c);
        }

        double dtd=get_config_real(config, "dt", "second");
        dt=dtd;
        if(dtd!=dt){   
            throw std::runtime_error("dt not representable as single precision.");
        }

        max_steps=get_config_int(config, "numSteps", "steps");
    }

    virtual void on_begin_prototypes()
    {}

    virtual void on_neuron_prototype(
        const prototype &prototype
    )
    {
        if(neuron_model){
            assert(prototype.model==neuron_model->name());
        }else{
            neuron_model=create_neuron_model(prototype.model);
        }

        if(prototype.index!=neuron_factories.size()){
            throw std::runtime_error("Corrupt synapse indices.");
        }

        neuron_factories.push_back(neuron_model->create_factory(prototype));
    }

    virtual void on_synapse_prototype(
        const prototype &p
    )
    {
        if(p.index!=synapse_factories.size()){
            throw std::runtime_error("Corrupt synapse indices.");
        }
        if(p.params.size()!=1){
            throw std::runtime_error("Expected exactly one synapse param in protitype.");
        }

        int index=p.find_param_index("weight");
        if(index!=0){
            throw std::runtime_error("Expected to find weight param at only index.");
        }

        synapse_factories.push_back([=](const prototype &p, unsigned nParams, const double *pParams) -> int32_t {
            double x=pParams[index];
            int32_t i=int32_t(x);
            if(x!=i){
                throw std::runtime_error("Synapse weight is not an integer.");
            }
            return i;
        });
    }

    virtual void on_end_prototypes()
    {
        auto neuron_model_name=neuron_model->name();

        std::string graph_type_path="providers/SNN_"+algorithm_name+"_"+neuron_model_name+"_graph_type.xml";

        graphType=loadGraphType(graph_type_path);

        neuronDeviceType=graphType->getDeviceType("neuron");
        neuronSpikeInPin=neuronDeviceType->getInput("spike_in");
        neuronSpikeOutPin=neuronDeviceType->getOutput("spike_out");
        neuronHashOutPin=neuronDeviceType->getOutput("hash_out");

        externalDeviceType=graphType->getDeviceType("external_output");
        externalHashInPin=externalDeviceType->getInput("hash_in");

        synapseProperties=neuronSpikeInPin->getPropertiesSpec()->create();
        if(synapseProperties->payloadSize()!=4){
            throw std::runtime_error("Expecting 4-byte edge property with a single integer weight."); 
        }

       

        auto graphPropertiesType=graphType->getPropertiesSpec();

        auto graphProperties=graphPropertiesType->create();
        
        graphPropertiesType->setScalarSubElement("dt", graphProperties, dt);
        graphPropertiesType->setScalarSubElement("global_rng_seed", graphProperties, uint64_t(1));
        graphPropertiesType->setScalarSubElement("max_steps", graphProperties, max_steps);
        // This means "never export within run", well unless it does 4B time steps...
        graphPropertiesType->setScalarSubElement("hash_export_timer_gap", graphProperties, uint32_t(0xFFFFFFFFul));
        graphPropertiesType->setScalarSubElement("hash_export_on_max_steps", graphProperties, uint32_t(1));

        writer->onGraphType(graphType);
        gid=writer->onBeginGraphInstance(graphType, "snn", graphProperties, rapidjson::Document());

    }

    virtual void on_begin_neurons()
    {
        writer->onBeginDeviceInstances(gid);

        if(connect_external_hash){
            external_sink_index=writer->onDeviceInstance(gid, externalDeviceType, "ext_out", {}, nullptr);
        }
    }

    virtual void on_neuron(
        const prototype &neuron_prototype,
        std::string_view id,
        unsigned nParams, const double *pParams
    )
    {
        neuron_info ni;
        ni.id=id;
        ni.linear_index=m_neurons.size();
        ni.neuron=neuron_factories.at(neuron_prototype.index)(neuron_prototype, id, nParams, pParams);

        auto dst_props=neuronDeviceType->getPropertiesSpec()->create();
        auto src_props=ni.neuron->get_properties();

        // PROPS consist of neuron properties followed by a 4-byte neuron id and a 4-byte hash offset.
        if(dst_props->payloadSize() != src_props.second+4+4){
            throw std::runtime_error("Neuron properties sizes dont match.");
        }
        
        memcpy(dst_props.payloadPtr(), src_props.first, src_props.second);
        *(dst_props.payloadPtr()+src_props.second) = ni.linear_index;
        *(dst_props.payloadPtr()+src_props.second+4) = 0; // no hash offset

        ni.sink_index = writer->onDeviceInstance(
            gid,
            neuronDeviceType, std::string{id},
            dst_props, nullptr
        );

        auto iti=m_neurons.insert(neuron_map_t::value_type{ni.id, ni});
        if(!iti.second){
            throw std::runtime_error("Duplicate neruond id.");
        }
    }

    virtual void on_end_neurons()
    {
        writer->onEndDeviceInstances(gid);
    }

    virtual void on_begin_synapses()
    {
        writer->onBeginEdgeInstances(gid);
    }

    virtual void on_synapse(
        const prototype &synapse_prototype,
        std::string_view dest_id,
        std::string_view source_id,
        unsigned nParams, const double *pParams
    )
    {
        neuron_info &dst=m_neurons.at(std::string{dest_id});
        neuron_info &src=m_neurons.at(std::string{source_id});

        auto localProperties=synapseProperties.clone();

        int32_t weight=synapse_factories.at(synapse_prototype.index)(synapse_prototype, nParams, pParams);

        *(int32_t*)localProperties->payloadPtr() = weight;

        writer->onEdgeInstance(
            gid,
            dst.sink_index, neuronDeviceType, neuronSpikeInPin,
            src.sink_index, neuronDeviceType, neuronSpikeOutPin,
            -1, localProperties, TypedDataPtr{}
        );

    }

    virtual void on_end_synapses()
    {
        if(connect_external_hash){
            for(auto &ni : m_neurons){
                writer->onEdgeInstance(gid,
                    external_sink_index, externalDeviceType, externalHashInPin,
                    ni.second.sink_index, neuronDeviceType, neuronHashOutPin,
                    -1, TypedDataPtr{}, TypedDataPtr{}
                );
            }
        }

        writer->onEndEdgeInstances(gid);
    }

    virtual void on_end_network()
    {
        writer->onEndGraphInstance(gid);
    }
};

int main(int argc, char *argv[])
{
    std::string algorithm_name="HwIdle";

    HardwareIdleSink sink;
    sink.algorithm_name=algorithm_name;

    sax_writer_options options;
    sink.writer=createSAXWriterOnFile("/dev/stdout", options);

    DumbSNNSourceFromFile(stdin, &sink);
} 

