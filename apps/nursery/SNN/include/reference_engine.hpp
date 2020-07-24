#ifndef zero_delay_engine_hpp
#define zero_delay_engine_hpp


#pragma STDC FENV_ACCESS ON

#include <xmmintrin.h>	// SSE instructions
#include <pmmintrin.h>	// SSE instructions

#ifndef __SSE2_MATH__
#error "Denormal control probably won't work."
#endif

inline void CheckDenormalsDisabled()
{
    if( _MM_GET_FLUSH_ZERO_MODE() != _MM_FLUSH_ZERO_ON ){
        throw std::runtime_error("Denormals appear enabled.");
    }
    if( _MM_GET_DENORMALS_ZERO_MODE() != _MM_DENORMALS_ZERO_ON ){
        throw std::runtime_error("Denormals appear enabled.");
    }

    volatile float x=2e-38f; // Just above sub-normal
    volatile float zero=0;

    x=x*0.5f;
    if(x!=zero){
        throw std::runtime_error("Denormals appear enabled.");
    }
}

#include "neuron.hpp"

#include "robin_hood.hpp"

#include <functional>
#include <memory>
#include <list>

class ReferenceEngine
    : public DumbSNNSink
{
public:
    

private:

    struct synapse_proto_t
    {
        int weight_index;
        int delay_index;
        int target_index;
    };

    struct synapse_info_t
    {
        unsigned dest;
        unsigned delay;
        stimulus_type weight;
    };

    struct neuron_info_t
    {
        std::string id;
        unsigned index;
        std::shared_ptr<Neuron> neuron;
        std::vector<synapse_info_t> fanout;
    };

    float m_dt;
    unsigned m_total_steps;

    std::vector<synapse_proto_t> m_synapse_protos;

    std::vector<neuron_factory_functor_t> m_neuron_factories;

    std::vector<neuron_info_t> m_neurons;
    robin_hood::unordered_map<std::string,unsigned> m_id_to_index;
public:
    virtual void on_begin_network(
        const std::vector<config_item> &config
    ){
        auto c=get_config_string(config, "calc_type","type");
        if(c!="float_ftz_daz"){
            throw std::runtime_error("calc_type doesn't match. Want='float_ftz_daz', got="+c);
        }
        double dt=get_config_real(config, "dt", "second");
        m_dt=dt;
        if(m_dt!=dt){   
            throw std::runtime_error("dt not representable as single precision.");
        }

        m_total_steps=get_config_int(config, "numSteps", "steps");
    }

    virtual void on_begin_prototypes()
    {}

    virtual void on_neuron_prototype(
        const prototype &prototype
    )
    {
        auto model=create_neuron_model(prototype.model);
        m_neuron_factories.push_back(model->create_factory(prototype));
    }


    virtual void on_synapse_prototype(
        const prototype &prototype
    )
    {
        synapse_proto_t p;
        p.delay_index=-1;
        p.target_index=-1;
        p.weight_index=-1;

        if(prototype.model=="SynapseZeroDelay"){
            if(prototype.params.size()!=1){
                throw std::runtime_error("Unexpected number of params.");
            }
            if(prototype.params[0].name!="weight"){
                throw std::runtime_error("Unsupported weight param.");
            }
            p.weight_index=0;
        }else if(prototype.model=="SynapseWithDelay"){
            if(prototype.params.size()!=2){
                throw std::runtime_error("Unexpected number of params.");
            }
            if(prototype.params[0].name=="weight"){
                p.weight_index=0;
                p.delay_index=1;
            }else{
                p.weight_index=1;
                p.delay_index=0;
            }

            if(prototype.params[p.weight_index].name!="weight"){
                throw std::runtime_error("Unsupported weight param '"+prototype.params[p.weight_index].name+'"');
            }
            if(prototype.params[p.delay_index].name!="delay" || prototype.params[p.delay_index].unit!="steps"){
                throw std::runtime_error("Unsupported delay param.");
            }
        }else if(prototype.model=="SynapseZeroDelayWithTarget"){
            if(prototype.params.size()!=2){
                throw std::runtime_error("Unexpected number of params.");
            }
            if(prototype.params[0].name=="weight"){
                p.weight_index=0;
                p.target_index=1;
            }else{
                p.weight_index=1;
                p.target_index=0;
            }

            if(prototype.params[p.weight_index].name!="weight"){
                throw std::runtime_error("Unsupported weight param.");
            }
            if(prototype.params[p.target_index].name!="target" || prototype.params[p.target_index].unit!="1"){
                throw std::runtime_error("Unsupported target param.");
            }
        }else{
            throw std::runtime_error("Unknown synapse type '"+prototype.model+"'");
        }
        m_synapse_protos.push_back(p);
    }

    virtual void on_end_prototypes()
    {}

    virtual void on_begin_neurons()
    {}

    virtual void on_neuron(
        const prototype &neuron_prototype,
        std::string_view id,
        unsigned nParams, const double *pParams
    ) {
        auto neuron=m_neuron_factories[neuron_prototype.index](neuron_prototype, id, nParams, pParams);
        unsigned index=m_neurons.size();
        m_neurons.push_back({
            std::string{id},
            index,
            neuron,
            {}
        });
        if(!m_id_to_index.insert({std::string{id}, index}).second){
            throw std::runtime_error("Duplicate neuron id.");
        }
    }

    virtual void on_end_neurons()
    {}

    virtual void on_begin_synapses()
    {}

    virtual void on_synapse(
        const prototype &synapse_prototype,
        std::string_view dest_id,
        std::string_view source_id,
        unsigned nParams, const double *pParams
    )
    {
        unsigned dst_index=m_id_to_index[std::string{dest_id}];
        unsigned src_index=m_id_to_index[std::string{source_id}];

        const auto &p=m_synapse_protos[synapse_prototype.index];

        double w=pParams[p.weight_index];
        if(round(w)!=w){
            std::stringstream tmp;
            tmp<<"Invalid weight (not scaled/rounded to integer), w="<<w;
            throw std::runtime_error(tmp.str());
        }

        unsigned delay=0;
        if(p.delay_index!=-1){
            double d=pParams[p.delay_index];
            if(round(d)!=d || d<0){
                throw std::runtime_error("Invalid delay");
            }
            delay=unsigned(d);
        }

        m_neurons.at(src_index).fanout.push_back({dst_index,delay, (int)w });
    }

    virtual void on_end_synapses()
    {}

    virtual void on_end_network()
    {}

    void run(uint64_t seed, std::function<void(unsigned,unsigned)> on_firing)
    {
        CheckDenormalsDisabled();

        for(const auto &ni : m_neurons){
            ni.neuron->reset(seed);
        }

        std::list<std::vector<stimulus_type>> stimulus_backing;
        std::vector<std::vector<stimulus_type>*> stimulus_window;

        auto add_stim_vec=[&]()
        {
            stimulus_backing.emplace_back(m_neurons.size()*2, 0);
            stimulus_window.push_back(&stimulus_backing.back());
        };

        auto write_stimulus=[&](unsigned delay, unsigned dst, stimulus_type stim)
        {
            while(delay>=stimulus_window.size()){
                add_stim_vec();
            }
            int neg=stim<0;
            (*stimulus_window[delay])[dst*2+neg] += stim;
        };

        add_stim_vec();

        for(unsigned t=0; t<m_total_steps; t++){
            auto stim_now=stimulus_window.front();
            stimulus_window.erase(stimulus_window.begin(), stimulus_window.begin()+1);

            const auto *stim_vec=&stim_now->at(0);
            for(unsigned i=0; i<m_neurons.size(); i++){
                const auto &ni=m_neurons[i];
                bool f=ni.neuron->step(m_dt, stim_vec[i*2+0], stim_vec[i*2+1]);
                //fprintf(stderr, "  t=%u, n=%u, st[0]=%d, st[1]=%d\n", t, i, stim_now->at(i*stim_stride), stim_now->at(i*stim_stride+1));
                if(f){
                    //on_firing(t,i);
                    for(const auto &s : ni.fanout){
                        write_stimulus(s.delay, s.dest, s.weight);
                    }
                }
                if(i!=0){
                    fprintf(stdout, ",");
                }
                fprintf(stdout, "%g", ni.neuron->project());
            }
            fprintf(stdout, "\n");

            stim_now->assign(stim_now->size(), 0);
            stimulus_window.push_back(stim_now);
        }
    }
};

#endif
