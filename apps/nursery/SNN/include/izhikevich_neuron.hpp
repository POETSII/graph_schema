#ifndef izhikevich_neuron_hpp
#define izhikevich_neuron_hpp

#include "neuron.hpp"

#include "shared_utils.hpp"

#include "neuron_models/izhikevich_neuron_model.hpp"

class IzhikevichNeuron
    : public Neuron
{
public:
    using model = Izhikevich;
    using properties_t = typename model::properties_t;
    using state_t = typename model::state_t;

    properties_t p;
    state_t s;
    
    void reset(uint64_t seed)
    {
        model::reset(handler_log, p, s, seed);
    }

    bool step(float dt, int32_t pos_stim, int32_t neg_stim)
    {
        return model::step(handler_log, p, s, dt, pos_stim, neg_stim);
    }

    float project() const
    {
        throw std::runtime_error("Not implemented.");
    }

    std::pair<const void*,size_t> get_properties() const override
    {
        return {&p, sizeof(p)};
    }

    /*void dump(FILE *dst)
    {
        fprintf(dst, "a=%f,b=%f,c=%f,d=%f,i_offset=%f,Ir=%f,v=%f,u=%f,rng=%llu\n",
            a,b,c,d,i_offset,Ir,v,u,(unsigned long long)rng
        );
    }*/
};



class IzhikevichNeuronModel
    : public NeuronModel
{
protected:

public:
    IzhikevichNeuronModel()
    {
        add_standard_substitutions<Izhikevich::properties_t,Izhikevich::state_t>("Izhikevich");
    }

    virtual neuron_factory_functor_t create_factory(const prototype &p) const
    {
        auto indices=build_param_indices<Izhikevich::properties_t>(p);

        auto res=[=](const prototype &p, std::string_view id, unsigned nParams, const double *pParams) -> std::shared_ptr<Neuron>
        {
            if(p.model!="Izhikevich"){
                throw std::runtime_error("Wrong model in prototype.");
            }

            auto n=std::make_shared<IzhikevichNeuron>();

            apply_param_indices<Izhikevich::properties_t>(indices, n->p, nParams, pParams);

            n->reset(0);

            //fprintf(stderr, "id=%s,", std::string{id}.c_str());
            
            return std::shared_ptr<Neuron>(n);
        };
        return res;
    }
};

#endif
