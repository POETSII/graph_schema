#ifndef generate_CUBA_hpp
#define generate_CUBA_hpp

#include "dumb_snn_sink.hpp"

#include <random>
#include <algorithm>

#include "generate_utils.hpp"

void generate_CUBA(
    unsigned N,
    unsigned numSteps,
    std::mt19937_64 &rng,
    DumbSNNSink &sink
){
    std::uniform_real_distribution<> udist;

    float dt=0.0001; // second. This is the default dt, and none is specified in CUBA example

    auto mv_to_uv_round=[](double x) -> double
    {
        return round(x*1000);
    };

    float taum = 0.020; // second
    float taue = 0.005; // second
    float taui = 0.010; // second
    float Vt = -0.050; // volt
    float Vr = -0.060; // volt
    float El = -0.049; // volt
    unsigned refLen = 50; // 5ms / 0.1ms
    float Vo = 0;

    double pE=(3200-800.0)/3200.0;

    unsigned Ne=unsigned(pE*N);
    unsigned Ni=N-Ne;

    prototype Neu{
        0,
        "Neu",
        "CUBA",
        {},
        {
            //{"taum", "second", taum},
            //{"taue", "second", taue},
            //{"taui", "second", taui},
            //{"Vt", "volt", Vt},
            //{"Vr", "volt", Vr},
            //{"El", "volt", El},
            //{"refLen", "steps", refLen}, // 5ms / 0.1ms
            {"nid", "1", 0},
            {"Vo", "volt", Vo}
        }
    };

    float we = (60*0.27/10); //*mV # excitatory synaptic weight (voltage)
    float wi = (-20*4.5/10); //*mV # inhibitory synaptic weight

    prototype SynE{
        0,
        "SE",
        "SynapseZeroDelay",
        {},
        {
            {"weight", "uV", mv_to_uv_round(we)}
        }
    };
    prototype SynI{
        1,
        "SI",
        "SynapseZeroDelay",
        {},
        {
            {"weight", "uV", mv_to_uv_round(wi)}
        }
    };

    std::vector<config_item> items({
        {"dt" , "second", (float)dt},
        {"numSteps" ,    "steps", (double)numSteps},
        {"calc_type",    "type", "float_ftz_daz"}, // Calculations should be done in this form
        {"globalSeed",   "1",    double(rng()&0xFFFFFFFFFFFFull)},
        {"sendHashOnSpike", "1", 1.0}
    });
    sink.on_begin_network(items);

    sink.on_begin_prototypes();
    sink.on_neuron_prototype(Neu);
    sink.on_synapse_prototype(SynE);
    sink.on_synapse_prototype(SynI);
    sink.on_end_prototypes();

    std::vector<std::string> neuron_ids;

    uint32_t nid=0;

    sink.on_begin_neurons();
    std::vector<double> params = Neu.param_defaults();
    for(unsigned i=0; i<N; i++){
        std::string id(10, 0);
        int n=sprintf(id.data(), "%c%u", (i<Ne)?'e':'i', i);
        id.resize(n);
        double r=udist(rng);
        // Vo = Vr + rand() * (Vt - Vr)
        params[0]=nid++;
        params[1]=float(Vr + r * (Vt-Vr));
        sink.on_neuron(Neu, id, params.size(), &params[0]);
        neuron_ids.push_back(id);
    }
    sink.on_end_neurons();

    sink.on_begin_synapses();

    double pConn=std::max(1.0, (4000*0.02)/N);

    robin_hood::unordered_flat_set<unsigned> working;
    working.reserve(N*pConn*1.1);
    auto paramsE=SynE.param_defaults();
    auto paramsI=SynI.param_defaults();
    for(const auto &dst : neuron_ids){
        sample_by_prob(
            pConn, neuron_ids,
            [&](const std::string &src){
                if(src[0]=='e'){
                    sink.on_synapse(SynE, dst, src, paramsE.size(), &paramsE[0]);
                }else{
                    sink.on_synapse(SynI, dst, src, paramsI.size(), &paramsI[0]);
                }
            },
            rng, working
        );
    }
    sink.on_end_synapses();
    sink.on_end_network();
}

#endif