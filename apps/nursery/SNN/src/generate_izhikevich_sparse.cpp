#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_izhikevich.hpp"
#include "../include/generate_fast_simple.hpp"

int main(int argc, char *argv[])
{
    unsigned Ne=800;
    unsigned Ni=200;
    unsigned K=1000;
    float dt=0.0005;
    unsigned numSteps=2000;

    if(argc>1){
        Ne=atoi(argv[1]);
    }
    if(argc>2){
        Ni=atoi(argv[2]);
    }
    if(argc>3){
        K=atoi(argv[3]);
    }
    if(argc>4){
        dt=atof(argv[4]);
    }
    if(argc>5){
        numSteps=atoi(argv[5]);
    }

    fprintf(stderr, "Ne=%u, Ni=%u, N=%u, dt=%f, numSteps=%u\n", Ne, Ni, Ne+Ni, dt, numSteps);
    double Ns=(Ne+Ni)*K;
    fprintf(stderr, "K=%u, pConn=%f, NumSynapses~=%llu=2^%f\n", K, K/double(Ne+Ni), (unsigned long long)Ns, log2(Ns));

    DumbSNNSinkToFile sink(stdout);

    std::mt19937_64 rng;
    generate_izhikevich(
        Ne,
        Ni,
        K,
        dt,
        numSteps,
        rng,
        sink
    );
}
