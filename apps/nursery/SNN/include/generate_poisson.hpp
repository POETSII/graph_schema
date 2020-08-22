#ifndef generate_poisson_hpp
#define generate_poisson_hpp

#include "dumb_snn_sink.hpp"

#include <random>
#include <algorithm>

#include "generate_utils.hpp"

void generate_poisson(
    unsigned N,
    unsigned K,
    double firingRate,
    unsigned numSteps,
    std::mt19937_64 &rng,
    DumbSNNSink &sink
){
    std::uniform_real_distribution<> udist;

    float dt=1; // Time means nothin for this graph

    firingRate=std::max(0.0, std::min(1.0, firingRate));
    uint32_t threshold=(uint32_t)floor(ldexp(firingRate, 32));

    prototype Neu{
        0,
        "Neu",
        "poisson",
        {},
        {
            {"nid", "1", 0},
            {"rngO", "1", 0},
            {"threshold", "1", threshold}
        }
    };

    prototype Syn{
        0,
        "SE",
        "SynapseZeroDelay",
        {},
        {
            {"weight", "uV", 1}
        }
    };

    sink.on_begin_network({
        {"dt" , "second", (float)dt},
        {"numSteps" ,    "steps", numSteps},
        {"calc_type",    "type", "float_ftz_daz"}, // Calculations should be done in this form
        {"globalSeed",   "1",    rng()&0xFFFFFFFFFFFFull},
        {"sendHashOnSpike", "1", 1}
    });

    sink.on_begin_prototypes();
    sink.on_neuron_prototype(Neu);
    sink.on_synapse_prototype(Syn);
    sink.on_end_prototypes();

    std::vector<std::string> neuron_ids;

    uint32_t nid=0;

    sink.on_begin_neurons();
    std::vector<double> params = Neu.param_defaults();
    for(unsigned i=0; i<N; i++){
        std::string id(10, 0);
        int n=sprintf(id.data(), "n%u", i);
        id.resize(n);
        params[0]=nid++;
        params[1]=rng()&0xFFFFFFFFFFull;
        sink.on_neuron(Neu, id, params.size(), &params[0]);
        neuron_ids.push_back(id);
    }
    sink.on_end_neurons();

    sink.on_begin_synapses();

    double pConn=std::max(0.0, std::min(1.0, K/(double)N));

    robin_hood::unordered_flat_set<unsigned> working;
    working.reserve(N*pConn*1.1);
    auto paramsE=Syn.param_defaults();
    for(const auto &dst : neuron_ids){
        paramsE[0]=rng()%256;
        sample_by_prob(
            pConn, neuron_ids,
            [&](const std::string &src){
                sink.on_synapse(Syn, dst, src, paramsE.size(), &paramsE[0]);
            },
            rng, working
        );
    }
    sink.on_end_synapses();
    sink.on_end_network();
}

#endif