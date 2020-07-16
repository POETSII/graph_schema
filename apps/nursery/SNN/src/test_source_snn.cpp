#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_izhikevich.hpp"
#include "../include/generate_fast_simple.hpp"

int main()
{
    DumbSNNSinkToFile sink(stdout);

    std::mt19937_64 rng;
    generate_fast_simple(
        100000,
        1000, // K
        rng,
        sink
    );
}
