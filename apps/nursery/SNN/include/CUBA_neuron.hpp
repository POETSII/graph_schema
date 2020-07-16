#ifndef CUBA_neuron_hpp
#define CUBA_neuron_hpp

#include "neuron.hpp"

#include "shared_utils.hpp"

/*
https://brian2.readthedocs.io/en/2.0rc/examples/CUBA.html
*/
class CUBANeuron
    : public Neuron
{
public:
    float taum;
    float taue;
    float taui;
    float Vt;
    float Vr;
    float El;
    unsigned refractorySteps;
    float Vo;

    float v;
    float ge;
    float gi;
    unsigned refractoryTimer;
    
    void reset(uint64_t seed)
    {
        v=Vo;
        ge=0;
        gi=0;
        refractoryTimer=0;
    }

    bool step(float dt, unsigned nStim, const stimulus_type *pStim) override 
    {
        // stimulus is an integer number of uV
        const float SCALE=1e-6; // Get stimulus in volts

        ge += pStim[0] * SCALE;
        gi += pStim[1] * SCALE;

        //dv/dt  = (ge+gi-(v-El))/taum : volt (unless refractory)
        float vNext=v; // volt
        if(refractoryTimer==0){
            vNext = v + dt * (ge + gi - (v-El))/taum;
        }
        //dge/dt = -ge/taue : volt
        float geNext= ge + dt * -ge/taue; // volt
        // dgi/dt = -gi/taui : volt
        float giNext = gi + dt * -gi/taui;

        v=vNext;
        ge=geNext;
        gi=giNext;

        bool fire=v>Vt && refractoryTimer==0;
        if(fire){
            v=Vr;
            refractoryTimer=refractorySteps;
        }else if(refractoryTimer>0){
            refractoryTimer--;
        }

        return fire;
    }

    float project() const
    { 
        return v;
    }
};

neuron_factory_t create_CUBA_neuron_factory(
    const prototype &p
){

    std::vector<int> indices;
    indices.push_back(p.find_param_index("taum"));
    indices.push_back(p.find_param_index("taue"));
    indices.push_back(p.find_param_index("taui"));
    indices.push_back(p.find_param_index("Vt"));
    indices.push_back(p.find_param_index("Vr"));
    indices.push_back(p.find_param_index("El"));
    indices.push_back(p.find_param_index("refractorySteps"));
    indices.push_back(p.find_param_index("Vo"));

    auto res=[=](const prototype &p, std::string_view id, unsigned nParams, const double *pParams) -> std::shared_ptr<Neuron>
    {
        auto n=std::make_shared<CUBANeuron>();
        n->taum=pParams[indices[0]];
        n->taue=pParams[indices[1]];
        n->taui=pParams[indices[2]];
        n->Vt=pParams[indices[3]];
        n->Vr=pParams[indices[4]];
        n->El=pParams[indices[5]];
        n->refractorySteps=pParams[indices[6]];
        n->Vo=pParams[indices[7]];

        n->reset(0);

        //fprintf(stderr, "id=%s,", std::string{id}.c_str());
        
        return std::shared_ptr<Neuron>(n);
    };
    return res;
}

#endif
