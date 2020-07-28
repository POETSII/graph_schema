#include "../include/neuron.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "graph_persist_dom_reader.hpp"
#include "graph_persist_sax_writer.hpp"

#include "../include/shared_utils.hpp"

#include <regex>
#include <fstream>
#include <iostream>

#include "robin_hood.hpp"


int main(int argc, char *argv[])
{
    std::string neuron_model="CUBA";

    if(argc>1){
        neuron_model=argv[1];
    }

    fprintf(stderr, "Specalising for neuron %s\n", neuron_model.c_str());

    auto model=create_neuron_model(neuron_model);

    std::map<std::string,std::string> replacements;
    for(const auto &kv : model->get_substitutions() ){
        replacements["${"+kv.first+"}"] = kv.second;
    }

    std::string line;
    while(std::getline(std::cin, line)){
        for(const auto &kv : replacements){
            auto pos=line.find(kv.first);
            while(pos!=std::string::npos){
                line.replace(pos, kv.first.size(), kv.second);
                pos=line.find(kv.first);
            }
        }
        std::cout<<line<<"\n";
    }
} 

