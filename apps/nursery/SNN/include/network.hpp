#ifndef network_hpp
#define network_hpp

#include "neuron.hpp"
#include "dumb_snn_sink.hpp"

#include "robin_hood.hpp"

#include <any>

struct Network
{
    struct edge_t
    {
        uint32_t dest;
        int32_t weight;
        uint32_t delay;
    };

    struct node_t
    {
        std::string id;
        uint32_t index; // Arbitrary contiguous linear index starting at 0. Unrelated to id 
        std::shared_ptr<NeuronModel> model;
        std::shared_ptr<Neuron> neuron;
        std::vector<edge_t> outgoing;
        std::vector<uint32_t> incoming;
        std::any user;
    };

    std::unordered_map<std::string,config_item> config;
    std::vector<node_t> nodes;
};

struct NetworkBuilder
    : DumbSNNSink
{
    Network net;
    robin_hood::unordered_map<std::string,uint32_t> id_to_index;

    std::vector<std::pair<std::shared_ptr<NeuronModel>,neuron_factory_functor_t>> neuron_factories;

    using synapse_factory_functor_t = std::function<int32_t(const prototype &p, unsigned nParams, const double *pParams)>;

    std::vector<synapse_factory_functor_t> synapse_factories;

    void on_begin_network(
        const std::vector<config_item> &_config
    ) override {
        for(const auto &co : _config){
            net.config[co.name]=co;
        }
    }

    void on_begin_prototypes() override
    {}

    void on_neuron_prototype(
        const prototype &prototype
    ) override
    {
        if(prototype.index!=neuron_factories.size()){
            throw std::runtime_error("Corrupt synapse indices.");
        }

        auto neuron_model=create_neuron_model(prototype.model);
        neuron_factories.push_back({neuron_model,neuron_model->create_factory(prototype)});
    }

    void on_synapse_prototype(
        const prototype &p
    ) override
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

    void on_end_prototypes() override
    {}

    void on_begin_neurons() override
    {}

    void on_neuron(
        const prototype &neuron_prototype,
        std::string_view id,
        unsigned nParams, const double *pParams
    ) override
    {
        auto factory=neuron_factories.at(neuron_prototype.index);

        Network::node_t node;
        node.id=id;
        node.index=net.nodes.size();
        node.model=factory.first;
        node.neuron=factory.second(neuron_prototype, id, nParams, pParams);

        auto iti=id_to_index.emplace(node.id, node.index);
        if(!iti.second){
            throw std::runtime_error("Duplicate node id.");
        }

        net.nodes.push_back(std::move(node));
    }

    void on_end_neurons() override
    {}

    void on_begin_synapses() override
    {}

    void on_synapse(
        const prototype &synapse_prototype,
        std::string_view dest_id,
        std::string_view source_id,
        unsigned nParams, const double *pParams
    ) override
    {
        auto dest_index=id_to_index.at(std::string{dest_id});
        auto source_index=id_to_index.at(std::string{source_id});

        int32_t weight=synapse_factories.at(synapse_prototype.index)(synapse_prototype, nParams, pParams);
        uint32_t delay=0;

        net.nodes[source_index].outgoing.push_back({dest_index,weight,delay});
        net.nodes[dest_index].incoming.push_back(source_index);
    }

    void on_end_synapses() override
    {}

    void on_end_network() override
    {}
};

#endif