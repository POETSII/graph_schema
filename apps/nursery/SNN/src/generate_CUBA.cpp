#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_CUBA.hpp"
#include "../include/generate_fast_simple.hpp"

int main(int argc, char *argv[])
{
    unsigned N=4000;
    unsigned numSteps=10000;

    if(argc>1){
        N=atoi(argv[1]);
    }
    if(argc>2){
        numSteps=atoi(argv[2]);
    }

    DumbSNNSinkToFile sink(stdout);

    std::mt19937_64 rng;
    generate_CUBA(
        N,
        numSteps,
        rng,
        sink
    );
}
