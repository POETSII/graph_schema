#include "fenv_control.hpp"

#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/reference_engine.hpp"


int main()
{
    DisableDenormals();

    ReferenceEngine engine;

    DumbSNNSourceFromFile(stdin, &engine);

    engine.run(
        [&](unsigned t, unsigned neuron, uint32_t hash){
            fprintf(stdout, "%u,%u,S,%08x\n", t, neuron, hash);
        },
        [&](uint32_t t, uint32_t neuron, uint32_t hash, const stats_msg_t &msg)
        {
            fprintf(stdout, "%u,%u,H,%08x,%u,%llu\n", t, neuron, hash, msg.stats_total_firings, (unsigned long long)msg.stats_sum_square_firing_gaps);
        }
    );
}
