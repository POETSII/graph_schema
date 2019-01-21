#include "particle.hpp"
#include "cell.hpp"

#include "tinsel_host_default.hpp"

#include <random>
#include <iostream>

HostPtr hostCreate(std::shared_ptr<PlatformInfo> platformInfo, int, const char **)
{
    return std::make_shared<HostDefault>(platformInfo);
}

extern "C" void softswitchMain(
    unsigned shardSize,
    const void *constBLOB,
    void *mutableBLOB
)
{
    std::mt19937 urng;
    auto unif=[&](){
        std::uniform_real_distribution<> u;
        return u(urng);
    };
    
    const unsigned N=64;
    
    const double dt=1.0/64;
    const double mass=1.0;

    const double horizon=1.0;
    const double drag=0.9;
    const double thermal=0.1;
    
    world_info_t info(dt, mass, horizon, drag, thermal);
    info.left=real_t::from_double(-0.5);
    info.right=real_t::from_double(2.5);
    info.top=real_t::from_double(2.5);
    info.bottom=real_t::from_double(-0.5);
    
    cell_t cell;
    cell.nhoodSize=0;
    cell.xBegin=info.left;
    cell.xEnd=info.right+real_t::eps();
    cell.yBegin=info.bottom;
    cell.yEnd=info.top+real_t::eps();

    for(unsigned i=0; i<N; i++){
        particle_t p;
        p.position=vector_t::from_double(unif(),unif());
        p.velocity=vector_t::from_double(unif()*0.1,unif()*0.1);
        p.colour=urng()%3;
        cell.owned.add(p);
    }    

    volatile void *sendSlot=tinselSlot(0);
    for(int i=1; i<TinselMsgsPerThread;i++){
        tinselAlloc(tinselSlot(i));
    }

    int steps=0;
    while(1){       
        cell.cell_step(info, (void*)sendSlot);

        for(unsigned i=0; i<N; i++){
            std::cout<<steps<<","<<steps*dt<<", "<<i<<","<<cell.owned[i].colour<<","<<cell.owned[i].position.x.to_double()<<","<<cell.owned[i].position.y.to_double()<<","<<cell.owned[i].velocity.x.to_double()<<","<<cell.owned[i].velocity.y.to_double()<<"\n";
        }

        std::cout.flush();

        if((steps%10)==0){
            std::cerr<<"Step "<<steps<<"\n";
        }

        if(++steps>2000){
            break;
        }
    }

}
