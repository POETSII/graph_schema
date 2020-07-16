#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/reference_engine.hpp"
#include "../include/izhikevich_neuron.hpp"
#include "../include/CUBA_neuron.hpp"

neuron_factory_t create_neuron_factory(
    const prototype &p
){
    if(p.model=="Izhikevich"){
        return create_izhikevich_neuron_factory(p);
    }else if(p.model=="CUBA"){
        return create_CUBA_neuron_factory(p);
    }else{
        throw std::runtime_error("Unknown neuron model "+p.model);
    }
}

int main()
{
     _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON );
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON );

    ReferenceEngine engine;

    DumbSNNSourceFromFile(stdin, &engine);

    engine.run(0, [&](unsigned t, unsigned neuron){
        fprintf(stdout, "%u,%u\n", t, neuron);
    });
}
