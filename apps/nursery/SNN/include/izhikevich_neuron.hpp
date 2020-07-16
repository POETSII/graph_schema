#ifndef izhikevich_neuron_hpp
#define izhikevich_neuron_hpp

#include "neuron.hpp"

#include "shared_utils.hpp"

class IzhikevichNeuron
    : public Neuron
{
public:
    float a, b, c, d, i_offset, Ir;
    float u, v;
    uint64_t rng;

    float uOrig, vOrig;
    uint64_t rngOrig;
    
    void reset(uint64_t seed)
    {
        u=uOrig;
        v=vOrig;
        rng=rngOrig+seed;
    }

    bool step(float dt, unsigned nStim, const stimulus_type *pStim)
    {
        throw std::runtime_error("TODO");   

        assert(nStim==1);
        return izhikevich_update(
            dt,
            a, b, c, d, i_offset, Ir,
            rng,
            u, v,
            pStim[0]
        );
    }

    float project() const
    {
        throw std::runtime_error("Not implemented.");
    }

    void dump(FILE *dst)
    {
        fprintf(dst, "a=%f,b=%f,c=%f,d=%f,i_offset=%f,Ir=%f,v=%f,u=%f,rng=%llu\n",
            a,b,c,d,i_offset,Ir,v,u,(unsigned long long)rng
        );
    }
};

neuron_factory_t create_izhikevich_neuron_factory(
    const prototype &p
){
    std::vector<int> indices;
    indices.push_back(p.find_param_index("a"));
    indices.push_back(p.find_param_index("b"));
    indices.push_back(p.find_param_index("c"));
    indices.push_back(p.find_param_index("d"));
    indices.push_back(p.find_param_index("i_offset"));
    indices.push_back(p.find_param_index("Ir"));
    indices.push_back(p.find_param_index("v"));
    indices.push_back(p.find_param_index("u"));
    

    auto res=[=](const prototype &p, std::string_view id, unsigned nParams, const double *pParams) -> std::shared_ptr<Neuron>
    {
        auto n=std::make_shared<IzhikevichNeuron>();
        n->a=pParams[indices[0]];
        n->b=pParams[indices[1]];
        n->c=pParams[indices[2]];
        n->d=pParams[indices[3]];
        n->i_offset=pParams[indices[4]];
        n->Ir=pParams[indices[5]];

        n->vOrig=pParams[indices[6]];
        n->uOrig=pParams[indices[7]];
        
        n->rngOrig=id_to_seed(id, 0);

        n->reset(0);

        fprintf(stderr, "id=%s,", std::string{id}.c_str());
        n->dump(stderr);

        return std::shared_ptr<Neuron>(n);
    };
    return res;
}

#endif
