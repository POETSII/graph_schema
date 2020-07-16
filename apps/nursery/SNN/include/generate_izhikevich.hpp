#ifndef generate_izhikevich_hpp
#define generate_izhikevich_hpp

#include "dumb_snn_sink.hpp"

#include <random>
#include <algorithm>

#include "generate_utils.hpp"

void generate_izhikevich(
    unsigned Ne,
    unsigned Ni,
    unsigned K,
    float dt,
    unsigned numSteps,
    std::mt19937_64 &rng,
    DumbSNNSink &sink
){
    std::uniform_real_distribution<> udist;

    auto f16=[](double x) -> double
    {
        if(x!=0){
            x=ldexp(round(ldexp(x,16)),-16);
        }
        return x;
    };

    prototype Ex{
        0,
        "Ex",
        "Izhikevich",
        {},
        {
            {"a","/ms", 0.02},
            {"b","/ms", 0.2},
            {"c","mV", -65}, //-65+15*re*re
            {"d","mV/ms", -8},   //d=8-6*re*re
            {"i_offset","nA", 0.0},
            {"v", "mV", -70},
            {"u", "mV/ms", -14},
            {"Ir","1", 5}
        }
    };
    prototype In{
        1,
        "In",
        "Izhikevich",
        {},
        {
            {"a", "/ms", 0.02}, //0.02+0.08*ri
            {"b", "/ms", 0.25}, // 0.25-0.05*ri
            {"c", "mV",-65}, 
            {"d", "mV/ms", 2},
            {"i_offset", "nA", 0.0},
            {"v", "mV", -70},
            {"u", "mV/ms", -14},
            {"Ir", "1", 2}
        }
    };

    prototype Syn{
        0,
        "Syn",
        "SynapseZeroDelay",
        {},
        {
            {"weight", "nA", 0.0}
        }
    };

    sink.on_begin_network({
        {"dt" , "mS", (float)dt},
        {"numSteps" ,    "steps", numSteps},
        {"calc_type",    "type", "float_ftz_daz"} // Calculations should be done in this form
    });

    sink.on_begin_prototypes();
    sink.on_neuron_prototype(Ex);
    sink.on_neuron_prototype(In);
    sink.on_synapse_prototype(Syn);
    sink.on_end_prototypes();

    std::vector<std::string> neuron_ids;

    sink.on_begin_neurons();
    std::vector<double> params = Ex.param_defaults();
    for(unsigned i=0; i<Ne; i++){
        std::string id(10, 0);
        int n=sprintf(id.data(), "e%u", i);
        id.resize(n);
        double re=udist(rng);
        params[2]=f16(-65+15*re*re);
        params[3]=f16(8-6*re*re);
        params[5]=-65;
        params[6]=params[1]*params[5];
        sink.on_neuron(Ex, id, params.size(), &params[0]);
        neuron_ids.push_back(id);
    }
    params = In.param_defaults();
    for(unsigned i=0; i<Ni; i++){
        std::string id(10, 0);
        int n=sprintf(id.data(), "i%u", i);
        id.resize(n);
        double ri=udist(rng);
        params[0]=f16(0.02+0.08*ri);
        params[1]=f16(0.25-0.05*ri);
        params[5]=-65;
        params[6]=params[1]*params[5];
        sink.on_neuron(In, id, params.size(), &params[0]);
        neuron_ids.push_back(id);
    }
    sink.on_end_neurons();

    sink.on_begin_synapses();

    double pConn=std::min(1.0, K/double(Ne+Ni));

    robin_hood::unordered_flat_set<unsigned> working;
    working.reserve((Ne+Ni)*pConn*1.1);
    params.resize(1);
    for(const auto &dst : neuron_ids){
        sample_by_prob(
            pConn, neuron_ids,
            [&](const std::string &src){
                params[0] = f16(src[0]=='e' ? 0.5*udist(rng) : -udist(rng));
                sink.on_synapse(Syn, dst, src, params.size(), &params[0]);
            },
            rng, working
        );
    }
    sink.on_end_synapses();
    sink.on_end_network();
}

#endif