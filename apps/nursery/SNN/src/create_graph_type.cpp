#include "../include/neuron.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "graph_persist_dom_reader.hpp"
#include "graph_persist_sax_writer.hpp"

#include <regex>
#include <fstream>
#include <iostream>

#include "robin_hood.hpp"


int main(int argc, char *argv[])
{
    std::string template_name="HwIdle";
    std::string neuron_model="CUBA";

    if(argc>1){
        template_name=argv[1];
    }
    if(argc>2){
        neuron_model=argv[2];
    }

    std::string neuron_template_path="graphs/SNN_"+template_name+"_template.xml";

    auto model=create_neuron_model(neuron_model);

    std::map<std::string,std::string> replacements;
    for(const auto &kv : model->get_substitutions() ){
        replacements["${"+kv.first+"}"] = kv.second;
    }

    std::string template_source;
    {
        std::ifstream src(neuron_template_path);
        if(!src.is_open()){
            throw std::runtime_error("Couldnt find file "+neuron_template_path);
        }
        std::string line;
        while(std::getline(src, line)){
            for(const auto &kv : replacements){
                auto pos=line.find(kv.first);
                while(pos!=std::string::npos){
                    line.replace(pos, kv.first.size(), kv.second);
                    pos=line.find(kv.first);
                }
            }
            template_source=template_source+line+"\n";
        }
    }

    std::cout<<template_source<<std::endl;
} 

