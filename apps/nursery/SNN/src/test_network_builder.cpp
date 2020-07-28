#include "../include/network.hpp"

#include "../include/generate_izhikevich.hpp"

#include <unistd.h>
#include <iostream>


int main()
{
    NetworkBuilder builder;

    std::mt19937_64 rng;
    generate_izhikevich(
        80, 20,
        40,
        0.001,
        1000,
        rng,
        builder
    );

    Network net(std::move(builder.net));
    std::cout<<"Num nodes="<<net.nodes.size()<<"\n";
}
