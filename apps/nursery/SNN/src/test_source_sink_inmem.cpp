#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_izhikevich.hpp"
#include "../include/generate_fast_simple.hpp"

class DumbSNNSinkCount
    : public DumbSNNSink
{
public:
    unsigned m_neurons=0;
    uint64_t m_synapses=0;
    double acc=0;

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
        if(0==(m_synapses%(100000000))){
            fprintf(stderr, "synapses=%llu=2^%f=10^%f\n", m_synapses, std::log2((double)m_synapses), std::log10((double)m_synapses));
        }
        acc += pParams[0];
    }

    virtual void on_end_synapses()
    {}

    virtual void on_end_network()
    {}
};

int main()
{
    DumbSNNSinkCount sink;

    std::mt19937_64 rng;
    generate_fast_simple(
        1000000,
        1000, // K
        rng,
        sink
    );

    fprintf(stderr, "N=%u, M=%u, check=%f\n", sink.m_neurons, sink.m_synapses, sink.acc);
}
