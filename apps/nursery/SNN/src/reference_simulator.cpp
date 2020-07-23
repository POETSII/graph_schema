#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/reference_engine.hpp"


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
