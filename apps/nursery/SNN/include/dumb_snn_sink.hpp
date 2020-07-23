#ifndef dumb_snn_hpp
#define dumb_snn_hpp

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <variant>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>

//#include "robin_hood.hpp"

/*
    There are two primary goals to this:
    - Explicitly specify all details of a network, leaving no randomness
    - Support bit-wise exact simulation of a network

    A natural consequence is that it is going to be big when stored
    as a file, and slow to read/write, but that is not considered
    important here.
*/

struct config_item
{
    std::string name;
    std::string unit;
    std::variant<double,std::string> value;
};

struct param_info
{
    std::string name;
    std::string unit; // ms, mV, ...
    double defaultVal;
};

struct prototype
{
    unsigned index; // Index within prototypes of the same class
    std::string name;
    std::string model;
    std::vector<config_item> config;
    std::vector<param_info> params;

    int try_find_param_index(std::string_view name) const
    {
        for(unsigned i=0; i<params.size(); i++){
            if(params[i].name==name){
                return i;
            }
        }
        return -1;
    }

    int find_param_index(std::string_view name) const
    {
        int r=try_find_param_index(name);
        if(r>-1){
            return r;
        }
        throw std::runtime_error("Couldn't find param called "+std::string{name}+" in prototype name="+std::string{this->name}+" model="+std::string{model});
    }

    void set_param_if_present(float &dst, int index, int nParams, const double *pParams) const
    {
        if(index>-1){
            assert(index<nParams);
            double dv=pParams[index];
            float fv=(float)dv;
            if(fv!=dv){
                throw std::runtime_error("Parameter is not representable as float.");
            }
            dst=fv;
        }
    }

    void set_param_if_present(unsigned &dst, int index, int nParams, const double *pParams) const
    {
        if(index>-1){
            assert(index<nParams);
            double dv=pParams[index];
            unsigned uv=(unsigned)dv;
            if(dv < 0 || uv!=dv){
                throw std::runtime_error("Parameter is not representable as unsigned.");
            }
            dst=uv;
        }
    }

    std::vector<double> param_defaults() const
    {
        std::vector<double> res;
        for(auto kv : params){
            res.push_back(kv.defaultVal);
        }
        return res;
    }

    void dump(FILE *dst, unsigned nParams, const double *pParams) const
    {
        if(nParams!=params.size()){
            throw std::runtime_error("Wrong number of params.");
        }
        fprintf(dst,"name=%s,model=%s;", name.c_str(), model.c_str());
        for(unsigned i=0; i<nParams; i++){
            const auto &pi = params[i];
            fprintf(dst, ",%s=%f", pi.name.c_str(), pParams[i]);
        }
        fprintf(dst, "\n");
    }
};

class DumbSNNSink
{
public:
    virtual ~DumbSNNSink()
    {}

    virtual void on_begin_network(
        const std::vector<config_item> &config
    ) =0;

    virtual void on_begin_prototypes()=0;

    virtual void on_neuron_prototype(
        const prototype &prototype
    ) =0;

    virtual void on_synapse_prototype(
        const prototype &prototype
    ) =0;

    virtual void on_end_prototypes() =0;

    virtual void on_begin_neurons() =0;
    virtual void on_neuron(
        const prototype &neuron_prototype,
        std::string_view id,
        unsigned nParams, const double *pParams
    )=0;
    virtual void on_end_neurons() =0;

    virtual void on_begin_synapses() =0;

    virtual void on_synapse(
        const prototype &synapse_prototype,
        std::string_view dest_id,
        std::string_view source_id,
        unsigned nParams, const double *pParams
    ) =0;

    virtual void on_end_synapses() =0;

    virtual void on_end_network() =0;

protected:
    double get_config_real(const std::vector<config_item> &config, const std::string &name, const std::string &unit)
    {
        for(const auto &ci : config){
            if(ci.name==name){
                if(ci.unit!=unit){
                     throw std::runtime_error("Config item "+name+" is in the wrong unit. Expected '"+unit+"' but got '"+ci.unit+"'");
                }
                if(auto p=std::get_if<double>(&ci.value)){
                    return *p;
                }else{
                    throw std::runtime_error("Config itme "+name+" has the wrong value type.");
                }
            }
        }
        throw std::runtime_error("Missing config item "+name);
    }

    int64_t get_config_int(const std::vector<config_item> &config, const std::string &name, const std::string &unit)
    {
        double d=get_config_real(config,name,unit);
        if(round(d)!=d){
            throw std::runtime_error("Expected int, but got real.");
        }
        return (int64_t)round(d);
    }

    std::string get_config_string(const std::vector<config_item> &config, const std::string &name, const std::string &unit)
    {
        for(const auto &ci : config){
            if(ci.name==name){
                if(ci.unit!=unit){
                    throw std::runtime_error("Config item "+name+" is in the wrong unit. Expected '"+unit+"' but got '"+ci.unit+"'");
                }
                if(auto p=std::get_if<std::string>(&ci.value)){
                    return *p;
                }else{
                    throw std::runtime_error("Config item "+name+" has the wrong value type.");
                }
            }
        }
        throw std::runtime_error("No config item called "+name);
    }
};

#endif
