#ifndef dumb_snn_source_from_file_hpp
#define dumb_snn_source_from_file_hpp

#include "dumb_snn_sink.hpp"

#include <sstream>
#include <unordered_map>
#include <cassert>

// Not supported in g++ 7.4, despite c++17 support
//#include <charconv>

void DumbSNNSourceFromFile(FILE *src, DumbSNNSink *sink)
{
    std::vector<prototype> neuron_prototypes;
    std::vector<prototype> synapse_prototypes;

    int line_num=0;
    size_t line_buffer_size=1024;
    char *line_buffer=(char*)malloc(line_buffer_size);
    char *line=nullptr;

    std::vector<std::string_view> _parts;

    auto peek_line=[&]() -> const char *
    {
        if(line==nullptr){
            ssize_t s=getline(&line_buffer, &line_buffer_size, src);
            if(s==-1){
                throw std::runtime_error("End of stream or read error.");
            }
            line=line_buffer;
            line_num++;
            while(s>0 && isspace(line[s-1])){
                line[s-1]=0;
                s--;
            }
            while(s>0 && isspace(line[0])){
                line++;
                s--;
            }
        }
        return line;
    };

    auto read_line=[&]() -> const char *
    {
        peek_line();
        auto res=line;
        line=0;
        return res;
    };

    auto expect=[&](const char *str) -> void
    {
        if(strcmp(peek_line(), str)){
            std::stringstream tmp;
            tmp<<"On line "<<line_num<<", expected "<<str<<", but got "<<peek_line();
            throw std::runtime_error(tmp.str());
        }
        read_line();
    };

    auto try_expect=[&](const char *str) -> bool
    {
        if(strcmp(peek_line(), str)){
            return false;
        }
        read_line();
        return true;
    };

    auto split_commas = [&](const char *curr) -> std::vector<std::string_view> &
    {
        _parts.clear();
        const char *begin=curr;
        while(1){
            if(*curr==',' || *curr==0){
                _parts.push_back({begin,(size_t)(curr-begin)});
                if(*curr==0){
                    break;
                }
                assert(*curr==',');
                begin=curr+1;
            }
            curr++;
        }
        return _parts;
    };

    auto parse_float=[&](const std::string_view &s) -> double
    {
        assert(!s.empty());
        double res;
        // No support in g++
        /*auto e=std::from_chars(&s[0], &s[s.size()], res);
        if(e.ptr!=&s[s.size()]){
            throw std::runtime_error("Couldn't parse float.");
        }*/
        std::string ss(s);
        char *endptr=nullptr;
        res=strtod(ss.c_str(), &endptr);
        if(endptr!=&ss[ss.size()]){
            throw std::runtime_error("Couldn't parse float '"+std::string{s}+"'");
        }
        return res;
    };

    auto find_prototype=[&](const std::vector<prototype> &prototypes, const std::string_view &name) -> const prototype &
    {
        assert(!name.empty());
        assert(prototypes.size() <= 32); // Anything more than this is outside design intent.
        for(unsigned i=0; i<prototypes.size(); i++){
            if(name==prototypes[i].name){
                return prototypes[i];
            }
        }
        throw std::runtime_error("Prtototype not found.");
    };

    auto read_config=[&]() -> std::vector<config_item>
    {
        std::vector<config_item> config;
        if(try_expect("BeginConfig")){
            while(!try_expect("EndConfig")){
                const char *l=read_line();
                const auto &parts=split_commas(l);

                if(parts.size()!=4){
                    throw std::runtime_error("Invalid config line.");
                }

                config_item ci;
                ci.name=parts[0];
                ci.unit=parts[1];
                std::string type{parts[2]};
                const auto &value=parts[3];

                if(type=="str"){
                    ci.value=std::string{value};
                }else if(type=="num"){
                    if(value.empty()){
                        throw std::runtime_error("Can't parse empty value.");
                    }
                    ci.value=parse_float(value);
                }else{
                    throw std::runtime_error("Unknown config type : '"+type+"'");
                }

                config.push_back(ci);
            }
        }
        return config;
    };


    expect("BeginNetworkV0");
    auto network_config=read_config();
    // EndConfig
    sink->on_begin_network(network_config);

    expect("BeginPrototypes");
    sink->on_begin_prototypes();
    while(!try_expect("EndPrototypes")){
        const auto &parts=split_commas(read_line());
        if(parts.size()!=3){
            throw std::runtime_error("Prototype too short.");
        }

        prototype p;

        std::string type{parts[0]};
        p.name=std::string{parts[1]};
        p.model=std::string{parts[2]};

        if(type.size()<5 || type.substr(0,5)!="Begin"){
            throw std::runtime_error("Corrupt prototype.");
        }
        std::string end_type="End"+type.substr(5);

        p.config=read_config();

        while(!try_expect(end_type.c_str())){
            const auto &cols=split_commas(read_line());
            if(parts.size()!=4){
                throw std::runtime_error("Corrupt parts list in prototype.");
            }
            if(parts[0]!="Param"){
                throw std::runtime_error("Corrupt param in prototype.");
            }
            param_info pi;
            pi.name=parts[1];
            pi.unit=parts[2];
            pi.defaultVal=parse_float(parts[3]);
            p.params.push_back(pi);
        }

        if(type=="BeginNeuronPrototype"){
            p.index=neuron_prototypes.size();
            neuron_prototypes.push_back(p);
            sink->on_neuron_prototype(p);
        }else if(type=="BeginSynapsePrototype"){
            p.index=synapse_prototypes.size();
            synapse_prototypes.push_back(p);
            sink->on_synapse_prototype(p);
        }else{
            throw std::runtime_error("Unknown type.");
        }
    }
    // EndPrototypes;
    sink->on_end_prototypes();

    std::string prev_prototype;

    std::vector<double> values;
    expect("BeginNeurons");
    sink->on_begin_neurons();
    while(!try_expect("EndNeurons")){
        const auto &parts=split_commas(read_line());
        fprintf(stderr, "%s\n", line_buffer);
        if(parts.size()<2){
            throw std::runtime_error("Not enough parts to neuron");
        }
        std::string_view prototype_name=parts[0];
        if(prototype_name.empty()){
            prototype_name=prev_prototype;
        }else{
            prev_prototype=prototype_name;
        }

        const auto &prototype=find_prototype(neuron_prototypes, prototype_name);
        const auto &id=parts[1];

        if(parts.size()!=prototype.params.size()+2){
            throw std::runtime_error("Wrong number of parts for neuron.");
        }
        values.resize(prototype.params.size());
        for(unsigned i=2; i<parts.size(); i++){
            if(parts[i].empty()){
                values[i-2]=prototype.params[i-2].defaultVal;
            }else{
                values[i-2]=parse_float(parts[i]);
            }
        }

        //prototype.dump(stderr, values.size(), &values[0]);

        sink->on_neuron(
            prototype,
            id,
            values.size(), &values[0]
        );
    }
    //EndNeurons
    sink->on_end_neurons();

    prev_prototype.clear();
    std::string prev_source, prev_dest;

    expect("BeginSynapses");
    sink->on_begin_synapses();
    while(!try_expect("EndSynapses")){
        const auto &parts=split_commas(read_line());
        if(parts.size()<3){
            throw std::runtime_error("Not enough parts to synapse");
        }
        std::string_view prototype_name=parts[0];
        if(prototype_name.empty()){
            prototype_name=prev_prototype;
        }else{
            prev_prototype=prototype_name;
        }
        const auto &prototype=find_prototype(synapse_prototypes, prototype_name);
        
        std::string_view dst_id=parts[1];
        if(dst_id.empty()){
            dst_id=prev_dest;
        }else{
            prev_dest=dst_id;
        }
        std::string_view src_id=parts[2];
        if(src_id.empty()){
            src_id=prev_source;
        }else{
            prev_source=src_id;
        }

        if(parts.size()!=prototype.params.size()+3){
            throw std::runtime_error("Wrong number of parts for synapse.");
        }
        values.resize(prototype.params.size());
        for(unsigned i=3; i<parts.size(); i++){
            if(parts[i].empty()){
                values[i-3]=prototype.params[i-3].defaultVal;
            }else{
                values[i-3]=parse_float(parts[i]);
            }
        }

        sink->on_synapse(
            prototype,
            dst_id, src_id,
            values.size(), &values[0]
        );
    }
    //EndNeurons
    sink->on_end_synapses();

    sink->on_end_network();

    free(line_buffer);
};

#endif
