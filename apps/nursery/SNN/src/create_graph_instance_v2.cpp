#include "../include/neuron.hpp"
#include "../include/dumb_snn_source_from_file.hpp"
#include "../include/network.hpp"

#include "graph_persist_dom_reader_impl.hpp"
#include "graph_persist_sax_writer_impl.hpp"

#include <regex>
#include <fstream>
#include <iostream>

#include "robin_hood.hpp"

void render_network_as_graph_instance(
    Network &net,
    std::string algorithm_name,
    GraphLoadEvents &writer
){
    bool connect_external_hash=true;
    bool connect_external_spikes=true;

    auto &config=net.config;
    
    if(auto c=config.at("calc_type").get_value_string("type"); c!="float_ftz_daz"){
        throw std::runtime_error("calc_type doesn't match. Want='float_ftz_daz', got="+c);
    }

    double dtd=config.at("dt").get_value_real("second");
    float dt=dtd;
    if(dtd!=dt){   
        throw std::runtime_error("dt not representable as single precision.");
    }

    fprintf(stderr, "dt=%g\n", dt);

    int32_t max_steps=config.at("numSteps").get_value_int( "steps");

    uint64_t globalSeed=config.at("globalSeed").get_value_int("1");

    std::string model_name;
    for(const auto &n : net.nodes){
        if(model_name.empty()){
            model_name=n.model->name();
        }else{
            if(model_name!=n.model->name()){
                throw std::runtime_error("More than one model.");
            }
        }
    }

    std::string graph_type_path="providers/SNN_"+algorithm_name+"_"+model_name+"_graph_type.xml";

    auto graphType=loadGraphType(graph_type_path);

    auto neuronDeviceType=graphType->getDeviceType("neuron");
    auto neuronSpikeInPin=neuronDeviceType->getInput("spike_in");
    auto neuronSpikeOutPin=neuronDeviceType->getOutput("spike_out");
    auto neuronHashOutPin=neuronDeviceType->getOutput("hash_out");

    auto externalDeviceType=graphType->getDeviceType("external_output");
    auto externalHashInPin=externalDeviceType->getInput("hash_in");
    auto externalSpikeInPin=externalDeviceType->getInput("spike_in");
    auto externalStopOutPin=externalDeviceType->getOutput("stop");

    auto haltDeviceType=graphType->getDeviceType("__halt_device_type__");
    auto haltStopInPin=haltDeviceType->getInput("stop");

    auto synapseProperties=neuronSpikeInPin->getPropertiesSpec()->create();
    if(synapseProperties->payloadSize()!=4){
        throw std::runtime_error("Expecting 4-byte edge property with a single integer weight."); 
    }

    auto graphPropertiesType=graphType->getPropertiesSpec();
    auto graphProperties=graphPropertiesType->create();
        
    graphPropertiesType->setScalarSubElement("dt", graphProperties, dt);
    graphPropertiesType->setScalarSubElement("global_rng_seed", graphProperties, globalSeed);
    graphPropertiesType->setScalarSubElement("max_steps", graphProperties, max_steps);
    graphPropertiesType->setScalarSubElement("total_neurons", graphProperties, uint32_t(net.nodes.size()));
    //  export once at the end of the run
    graphPropertiesType->setScalarSubElement("stats_export_interval", graphProperties, uint32_t(max_steps));

    writer.onGraphType(graphType);
    auto gid=writer.onBeginGraphInstance(graphType, "snn", graphProperties, rapidjson::Document());

    ///////////////////////////////////////////
    // Devices

    writer.onBeginDeviceInstances(gid);

    uint64_t external_sink_index=-1;
    if(connect_external_hash || connect_external_spikes){
        external_sink_index=writer.onDeviceInstance(gid, externalDeviceType, "ext_out", {}, nullptr);
    }

    uint64_t halt_sink_index=writer.onDeviceInstance(gid, haltDeviceType, "__halt__", TypedDataPtr{}, TypedDataPtr{});

    for(auto &n : net.nodes){
        auto dst_props=neuronDeviceType->getPropertiesSpec()->create();
        auto src_props=n.neuron->get_properties();

        // PROPS consist of just neuron properties (other members now removed)
        if(dst_props->payloadSize() != src_props.second){
            std::stringstream tmp;
            tmp<<"Neuron properties sizes dont match. expected from graph type ="<<dst_props->payloadSize()<<", got from neuron model ="<<src_props.second<<"\n";
            throw std::runtime_error(tmp.str());
        }
        
        memcpy(dst_props.payloadPtr(), src_props.first, src_props.second);

        uint64_t nid = writer.onDeviceInstance(
            gid,
            neuronDeviceType, std::string{n.id},
            dst_props, nullptr
        );

        n.user = nid;
    }

    writer.onEndDeviceInstances(gid);

    ///////////////////////////////////////////
    // Edges

    writer.onBeginEdgeInstances(gid);

    writer.onEdgeInstance(gid,
        halt_sink_index, haltDeviceType, haltStopInPin,
        external_sink_index, externalDeviceType, externalStopOutPin,
        -1, TypedDataPtr{}, TypedDataPtr{}
    );

    for(auto &src : net.nodes){
        auto src_sink_index=std::any_cast<uint64_t>(src.user);
        for(auto &edge : src.outgoing){
            auto &dst = net.nodes[edge.dest];

            auto localProperties=synapseProperties.clone();

            if(edge.delay!=0){
                throw std::runtime_error("Edges delays are not supported here.");
            }

            *(int32_t*)localProperties->payloadPtr() = edge.weight;

            auto dst_sink_index=std::any_cast<uint64_t>(dst.user);

            writer.onEdgeInstance(
                gid,
                dst_sink_index, neuronDeviceType, neuronSpikeInPin,
                src_sink_index, neuronDeviceType, neuronSpikeOutPin,
                -1, localProperties, TypedDataPtr{}
            );
        }

        if(connect_external_hash){
            writer.onEdgeInstance(gid,
                external_sink_index, externalDeviceType, externalHashInPin,
                src_sink_index, neuronDeviceType, neuronHashOutPin,
                -1, TypedDataPtr{}, TypedDataPtr{}
            );
        }
        if(connect_external_spikes){
            writer.onEdgeInstance(gid,
                external_sink_index, externalDeviceType, externalSpikeInPin,
                src_sink_index, neuronDeviceType, neuronSpikeOutPin,
                -1, TypedDataPtr{}, TypedDataPtr{}
            );
        }

        
    }

    writer.onEndEdgeInstances(gid);
    writer.onEndGraphInstance(gid);
};

int main(int argc, char *argv[])
{
    std::string algorithm_name="HwIdle";

    NetworkBuilder net_builder;
    DumbSNNSourceFromFile(stdin, &net_builder);

    sax_writer_options options;
    auto writer=createSAXWriterOnFile("/dev/stdout", options);

    render_network_as_graph_instance(
        net_builder.net, algorithm_name, *writer
    );
} 

