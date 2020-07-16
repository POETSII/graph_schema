#ifndef generate_fast_simple_hpp
#define generate_fast_simple_hpp

#include "dumb_snn_sink.hpp"

#include <boost/function_output_iterator.hpp>

#include <random>
#include <algorithm>

#include "robin_hood.hpp"

#include "generate_utils.hpp"

void generate_fast_simple(
    unsigned N,
    unsigned K,
    std::mt19937_64 &rng,
    DumbSNNSink &sink
){
    std::uniform_real_distribution<> udist;

    prototype Ex{
        0,
        "Ex",
        "Nothing",
        {},
        {
            {"a","ms",0.02},
            {"b","ms",0.2},
            {"c","ms",-65},
            {"d","ms",-8}
        }
    };

    prototype Syn{
        0,
        "Syn",
        "ZeroDelaySynapse",
        {},
        {
            {"weight", "nA", 0.0}
        }
    };

    sink.on_begin_network({
        {"dt" , "second", 1e-4},
        {"T" , "second", 1}
    });

    sink.on_begin_prototypes();
    sink.on_neuron_prototype(Ex);
    sink.on_synapse_prototype(Syn);
    sink.on_end_prototypes();

    std::vector<std::string> neuron_ids;

    sink.on_begin_neurons();
    std::vector<double> params = Ex.param_defaults();
    for(unsigned i=0; i<N; i++){
        std::string id(10, 0);
        int n=sprintf(id.data(), "e%u", i);
        id.resize(n);
        params[2]=i%1024;
        params[3]=i*(1.0/1024);
        sink.on_neuron(Ex, id, params.size(), &params[0]);
        neuron_ids.push_back(id);
    }
    sink.on_end_neurons();

    sink.on_begin_synapses();

    double pConn=std::min(1.0, K/(double)N);

    robin_hood::unordered_flat_set<unsigned> working;

    params.resize(1);
    unsigned ii=0;
    for(const auto &dst : neuron_ids){
        sample_by_prob(pConn, neuron_ids,
            [&](const std::string &src)
            {
                sink.on_synapse(Syn, dst, src, params.size(), &params[0]);
            },
            rng, working
        );
    }
    sink.on_end_synapses();
    sink.on_end_network();
}

#endif