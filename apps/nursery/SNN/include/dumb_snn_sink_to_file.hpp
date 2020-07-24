#ifndef dumb_snn_sink_to_file_hpp
#define dumb_snn_sink_to_file_hpp

#include "dumb_snn_sink.hpp"

#include <cmath>

class DumbSNNSinkToFile
    : public DumbSNNSink
{
private:
    FILE *m_dst;

    std::vector<prototype> m_neuron_prototypes;
    std::vector<prototype> m_synapse_prototypes;

    std::string m_prev_prototype;    
    std::string m_prev_synapse;
    std::string m_prev_dst;
    std::string m_prev_src;

    void print_num(double x)
    {
        double rx=round(x);
        if(rx==x && (ldexp(-1,63) < rx && rx < ldexp(1,63))){
            fprintf(m_dst,",%lld", (long long)rx);
        }else{
            fprintf(m_dst, ",%a", x);
        }
    }

    void on_prototype(std::string type, std::vector<prototype> &list, const prototype &p)
    {
        if(p.index!=list.size()){
            throw std::runtime_error("Corrupt prototype order : index in prototype doesn't match number of prototypes emitted so far.");
        }
        fprintf(m_dst, "Begin%s,%s,%s\n", type.c_str(), p.name.c_str(), p.model.c_str());
        on_config(p.config);
        for(unsigned i=0; i<p.params.size();i++){
            fprintf(m_dst, "Param,%s,%s", p.params[i].name.c_str(), p.params[i].unit.c_str());
            print_num(p.params[i].defaultVal);
            fputc('\n', m_dst);
        }
        fprintf(m_dst, "End%s\n", type.c_str());
        list.push_back(p);
    }

    void on_values(const prototype &p, unsigned nValues, const double *pValues)
    {
        if(p.params.size()!=nValues){
            throw std::runtime_error("Values length doesn't match prototpe.");
        }
        for(unsigned i=0; i<p.params.size();i++){
            if(p.params[i].defaultVal==pValues[i]){
                fputc(',', m_dst);
            }else{
                print_num(pValues[i]);
            }
        }
        fputc('\n', m_dst);
    }

    void on_config(const std::vector<config_item> &config)
    {
        if(!config.empty()){
            fprintf(m_dst,"BeginConfig\n");
            for(auto &xv : config){
                fprintf(m_dst, "%s,%s", xv.name.c_str(), xv.unit.c_str());
                if(auto p=std::get_if<double>(&xv.value)){
                    fputs(",num", m_dst);
                    print_num(*p);
                }else if(auto p=std::get_if<std::string>(&xv.value)){
                    const auto *c=p->c_str();
                    if(strpbrk(c, ",\n\r\"\\")){
                        throw std::runtime_error("Invalid char in config string.");
                    }
                    fputs(",str,", m_dst);
                    fputs(c,m_dst);
                }
                fputc('\n',m_dst);
            }
            fprintf(m_dst,"EndConfig\n");
        }
    }
public:
    DumbSNNSinkToFile(FILE *dst)
        : m_dst(dst)
    {}

    virtual ~DumbSNNSinkToFile()
    {}

    void on_begin_network(
        const std::vector<config_item> &config
    ) {
        fprintf(m_dst,"BeginNetworkV0\n");
        on_config(config);
    }

    void on_begin_prototypes() override
    {
        fprintf(m_dst, "BeginPrototypes\n");
    }

    void on_neuron_prototype(
        const prototype &prototype
    ) override {
        on_prototype("NeuronPrototype", m_neuron_prototypes, prototype);
    }

    void on_synapse_prototype(
        const prototype &prototype
    ) override {
        on_prototype("SynapsePrototype", m_synapse_prototypes, prototype);
    }

    void on_end_prototypes() override 
    {
        fprintf(m_dst, "EndPrototypes\n");
    }

    void on_begin_neurons() override
    {
        fprintf(m_dst, "BeginNeurons\n");
    }

    void on_neuron(
        const prototype &neuron_prototype,
        std::string_view id,
        unsigned nParams, const double *pParams
    ) override
    {
        if(neuron_prototype.name!=m_prev_prototype){
            fputs(neuron_prototype.name.c_str(), m_dst);
            m_prev_prototype=neuron_prototype.name;
        }
        fputc(',', m_dst);
        fwrite(id.data(), id.size(), 1, m_dst);
        on_values(neuron_prototype, nParams, pParams);

        //neuron_prototype.dump(stderr, nParams, pParams);
    }

    void on_end_neurons() override
    {
        fprintf(m_dst, "EndNeurons\n");
        m_prev_prototype.clear();
    }

    void on_begin_synapses()
    {
        fprintf(m_dst, "BeginSynapses\n");
    }

    void on_synapse(
        const prototype &synapse_prototype,
        std::string_view dest_id,
        std::string_view source_id,
        unsigned nParams, const double *pParams
    ){
        if(synapse_prototype.name!=m_prev_prototype){
            fputs(synapse_prototype.name.c_str(), m_dst);
            m_prev_prototype=synapse_prototype.name;
        }
        fputc(',', m_dst);
        if(dest_id != m_prev_dst){
            fwrite(dest_id.data(), dest_id.size(), 1, m_dst);
            m_prev_dst=dest_id;
        }
        fputc(',', m_dst);
        if(source_id!=m_prev_src){
            fwrite(source_id.data(), source_id.size(), 1, m_dst);
            m_prev_src=source_id;
        }
        on_values(synapse_prototype, nParams, pParams);
    }

    void on_end_synapses()
    {
        fprintf(m_dst, "EndSynapses\n");
    }

    void on_end_network()
    {
        fprintf(m_dst,"EndNetworkV0\n");
    }
};


#endif
