#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_poisson.hpp"

int main(int argc, char *argv[])
{
    unsigned N=1000;
    unsigned K=100;
    double rate=0.01;
    unsigned numSteps=10000;

    if(argc>1){
        N=atoi(argv[1]);
    }
    if(argc>2){
        K=atoi(argv[2]);
    }
    if(argc>3){
        rate=strtod(argv[3],nullptr);
    }
    if(argc>4){
        numSteps=atoi(argv[4]);
    }

    DumbSNNSinkToFile sink(stdout);

    std::mt19937_64 rng;
    generate_poisson(
        N,
        K,
        rate,
        numSteps,
        rng,
        sink
    );
}
