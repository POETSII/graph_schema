#include "../include/izhikevich_neuron.hpp"
#include "../include/CUBA_neuron_v2.hpp"
#include "../include/poisson_neuron.hpp"

std::shared_ptr<NeuronModel> create_neuron_model(
    std::string_view model
){
    if(model=="izhikevich"){
        return std::make_shared<IzhikevichNeuronModel>();
    }else if(model=="CUBA"){
        return std::make_shared<CUBANeuronModel>();
    }else if(model=="poisson"){
        return std::make_shared<PoissonNeuronModel>();
    }else{
        throw std::runtime_error(std::string("Unknown neuron model ")+std::string{model});
    }
}
