#ifndef poisson_neuron_hpp
#define poisson_neuron_hpp

#include "neuron.hpp"

#include "shared_utils.hpp"

#include "neuron_models/poisson_neuron_model.hpp"

#include <iostream>

class PoissonNeuron
    : public Neuron
{
public:
    using model = poisson;
    using properties_t = typename model::properties_t;
    using state_t = typename model::state_t;

    properties_t p;
    state_t s;
    
    void reset(uint64_t seed) override
    {
        model::reset(handler_log, p,s,seed);
    }

    bool step(float dt, int32_t pos_stim, int32_t neg_stim) override 
    {
        return model::step(handler_log, p, s, dt, pos_stim, neg_stim);
    }

    float project() const
    {
        return s.rng;
    }

    std::pair<const void*,size_t> get_properties() const override
    {
        //std::cerr<<"sizeof(p)=="<<sizeof(p)<<"\n";
        return {&p, sizeof(p)};
    }

    uint32_t hash() const override
    {
        return model::hash(p,s);
    }

    uint32_t nid() const override
    { return p.nid; }

    virtual void dump() const
    {
        model::dump(handler_log, p, s);
    }
};

class PoissonNeuronModel
    : public NeuronModel
{
protected:
    using model = poisson;
public:
    PoissonNeuronModel()
    {
        add_standard_substitutions<model::properties_t,model::state_t>("poisson");
    }

    virtual neuron_factory_functor_t create_factory(const prototype &p) const
    {
        auto indices=build_param_indices<model::properties_t>(p);

        auto res=[=](const prototype &p, std::string_view id, unsigned nParams, const double *pParams) -> std::shared_ptr<Neuron>
        {
            if(p.model!="poisson"){
                throw std::runtime_error("Wrong model in prototype.");
            }
            auto n=std::make_shared<PoissonNeuron>();

            apply_param_indices<model::properties_t>(indices, n->p, nParams, pParams);

            n->reset(0);

            //fprintf(stderr, "id=%s,", std::string{id}.c_str());
            
            return std::shared_ptr<Neuron>(n);
        };
        return res;
    }
};


#endif