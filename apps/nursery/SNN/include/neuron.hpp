#ifndef neuron_hpp
#define neuron_hpp

#include <cstdint>
#include <memory>
#include <functional>
#include <sstream>
#include <set>

#include "dumb_snn_sink.hpp"

using stimulus_type = int32_t;

class Neuron
{
public:
    virtual ~Neuron()
    {}

    virtual void reset(uint64_t seed) =0;

    virtual bool step(float dt, int32_t pos_stim, int32_t neg_stim) =0;

    virtual std::pair<const void*,size_t> get_properties() const =0;

    virtual float project() const =0;
};

using neuron_factory_functor_t = std::function<std::shared_ptr<Neuron>(const prototype &p, std::string_view id, unsigned nParams, const double *pParams)>;

class NeuronModel
{
protected:
    std::string m_name;
    std::map<std::string,std::string> m_substitutions;

    struct collect_members
    {
        std::stringstream dest;

        void operator()(const char *name, float, float &value)
        {
            dest<<"float "<<name<<";\n";
        }
        void operator()(const char *name, uint32_t, uint32_t &value)
        {
            dest<<"uint32_t "<<name<<";\n";
        }
        void operator()(const char *name, uint64_t, uint64_t &value)
        {
            dest<<"uint64_t "<<name<<";\n";
        }
    };

    template<class TP, class TS>
    void add_standard_substitutions(const char *name)
    {
        m_name=name;

        auto &subs=m_substitutions;

        subs["ModelType"]=m_name;

        subs["ModelType_IncludeFile"]=m_name+"_neuron_model.hpp";

        TP p;
        collect_members cp;
        p.walk(cp);
        subs["ModelType_DevicePropertyMembers"]=cp.dest.str();

        TS s;
        collect_members cs;
        s.walk(cs);
        subs["ModelType_DeviceStateMembers"]=cs.dest.str();
    }

    enum param_type
    {
        Int32,
        UInt32,
        UInt64,
        Float
    };

    struct param_index
    {
        param_type type;
        int index;
        size_t offset;
    };

    template<class T>
    struct collect_indices
    {
        const prototype &p;
        T *base;
        std::vector<param_index> indices;
        std::set<std::string> all_params;

        template<class V>
        void add(const char *name, param_type type, V &value)
        {
            int index=p.try_find_param_index(name);
            if(index!=-1){
                ssize_t offset=((char*)&value)-((char*)base); // Not really defined behaviour...
                assert(0<=offset && offset+sizeof(V)<=sizeof(T));
                param_index pi{type,index,size_t(offset)};
                indices.push_back(pi);
            }
            if(!all_params.insert(name).second){
                throw std::runtime_error("Duplicate name.");
            }
        }

        void operator()(const char *name, float, float &value)
        {
            add(name, Float, value);
        }
        void operator()(const char *name, uint32_t, uint32_t &value)
        {
            add(name, UInt32, value);
        }
        void operator()(const char *name, int32_t, int32_t &value)
        {
            add(name, Int32, value);
        }
        void operator()(const char *name, uint64_t, uint64_t &value)
        {
            add(name, UInt64, value);
        }
    };

    template<class T>
    std::vector<param_index> build_param_indices(const prototype &p) const
    {
        T def; // default construct to get members with defaults
        collect_indices<T> collector{p, &def, {}, {}};
        def.walk(collector);
        for(const auto &pi : p.params){
            if(collector.all_params.find(pi.name)==collector.all_params.end()){
                throw std::runtime_error("Unexpected param "+pi.name+" for model "+m_name);
            }
        }
        return collector.indices;
    };

    template<class T>
    static void apply_param_indices(const std::vector<param_index> &pis, T &dst, unsigned nParams, const double *pParams)
    {
        char *base=(char*)&dst;
        for(const auto &pi : pis){
            switch(pi.type){
            case UInt32:{
                double v=pParams[pi.index];
                uint32_t i=v;
                if(v<0 || i!=v){
                    throw std::runtime_error("Value is not compatible with uint32_t.");
                }
                *(uint32_t*)(base+pi.offset) = i;
                break;
            }
            case Int32: {
                double v=pParams[pi.index];
                int32_t i=v;
                if( i!=v){
                    throw std::runtime_error("Value is not compatible with int32_t.");
                }
                *(int32_t*)(base+pi.offset) = i;
                break;
            }
            case UInt64:{
                double v=pParams[pi.index];
                uint64_t i=v;
                if(v<0 || i!=v){
                    throw std::runtime_error("Value is not compatible with uint64_t.");
                }
                *(uint64_t*)(base+pi.offset) = i;
                break;
            }
            case Float:{
                double v=pParams[pi.index];
                float i=v;
                if( i!=v){
                    throw std::runtime_error("Value is not compatible with float.");
                }
                *(float*)(base+pi.offset) = i;
                break;
            }
            default:
                assert(0);
                throw std::logic_error("Case not reachable.");
            }
        }
    }
public:
    virtual ~NeuronModel()
    {}

    virtual neuron_factory_functor_t create_factory(const prototype &p) const =0;

    const std::string &name() const
    { return m_name; }

    const std::map<std::string,std::string> &get_substitutions() const
    {
        return m_substitutions;
    }
};


std::shared_ptr<NeuronModel> create_neuron_model(std::string_view model_name);

#endif
