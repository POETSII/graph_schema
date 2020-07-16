#include "../include/dumb_snn_sink_to_file.hpp"
#include "../include/dumb_snn_source_from_file.hpp"

#include "../include/generate_izhikevich.hpp"

#include <unistd.h>

int main()
{
    FILE *dst=fopen("/tmp/.dt10.test_nn", "w");

    DumbSNNSinkToFile sink(dst);

    std::mt19937_64 rng;
    generate_izhikevich(
        8000, // Ne
        2000, // Ni
        200, // K
        rng,
        sink
    );
    fclose(dst);

    FILE *src=fopen("/tmp/.dt10.test_nn", "r");

    DumbSNNSinkToFile sink2(stdout);
    DumbSNNSourceFromFile(src, &sink2);

    unlink("/tmp/.dt10.test_nn");
}
