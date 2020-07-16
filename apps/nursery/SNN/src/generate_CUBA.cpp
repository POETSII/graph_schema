#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_CUBA.hpp"
#include "../include/generate_fast_simple.hpp"

int main(int argc, char *argv[])
{
    unsigned numSteps=10000;

    if(argc>5){
        numSteps=atoi(argv[1]);
    }

    DumbSNNSinkToFile sink(stdout);

    std::mt19937_64 rng;
    generate_CUBA(
        numSteps,
        rng,
        sink
    );
}
