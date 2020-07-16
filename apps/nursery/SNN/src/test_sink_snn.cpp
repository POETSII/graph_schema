#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_izhikevich.hpp"

#include <unistd.h>

class DumbSNNSinkCount
    : public DumbSNNSink
{
public:
    uint64_t m_neurons=0;
    uint64_t m_synapses=0;

    virtual void on_begin_network(
        const std::vector<config_item> &config
    ){}

    virtual void on_begin_prototypes(){}

    virtual void on_neuron_prototype(
        const prototype &prototype
    ){}

    virtual void on_synapse_prototype(
        const prototype &prototype
    ){}

    virtual void on_end_prototypes(){}

    virtual void on_begin_neurons(){}
    virtual void on_neuron(
        const prototype &neuron_prototype,
        std::string_view id,
        unsigned nParams, const double *pParams
    ){
        m_neurons++;
    }
    virtual void on_end_neurons(){}

    virtual void on_begin_synapses(){}

    virtual void on_synapse(
        const prototype &synapse_prototype,
        std::string_view dest_id,
        std::string_view source_id,
        unsigned nParams, const double *pParams
    ){
        m_synapses++;
    }

    virtual void on_end_synapses()
    {}

    virtual void on_end_network()
    {}
};


int main()
{
    DumbSNNSinkCount sink;
    DumbSNNSourceFromFile(stdin, &sink);
}
