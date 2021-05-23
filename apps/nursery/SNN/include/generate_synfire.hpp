#ifndef generate_synfire_hpp
#define generate_synfire_hpp

#include "dumb_snn_sink.hpp"
#include "network.hpp"

#include <random>
#include <algorithm>

#include "generate_utils.hpp"

struct synfire_neuron
{
    std::string id;
    std::vector<std::string> inputs;
};



std::vector<std::vector<synfire_neuron>> generate_synfire_ring(
    unsigned L,
    unsigned W,
    unsigned K,
    std::string prefix,
    std::mt19937_64 &rng,
    Network &net
){
    std::uniform_real_distribution<> udist;

    std::vector<std::vector<synfire_neuron>> ns;
    ns.resize(L, std::vector<synfire_neuron>(W));

    for(unsigned l=0; l<L; l++){
        std::string lprefix=prefix+"_"+std::to_string(l);
        for(unsigned w=0; w<W; w++){
            ns[l][w].id=lprefix+"_"+std::to_string(w);
        } 
    }

    for(unsigned l=0; l<L; l++){
        const auto &prev=ns.at((l+L-1)%L);
        for(unsigned w=0; w<W; w++){
            sample_exactly_k(
                K,
                ,
                [&](unsigned i){
                    ns.at(l)
                }
            )
        }
    }


}

#endif