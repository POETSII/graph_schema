#include "particle.hpp"
#include "cell.hpp"

#include "tinsel_host_default.hpp"

#include <random>

HostPtr hostCreate(std::shared_ptr<PlatformInfo> platformInfo, int, const char **)
{
    return std::make_shared<HostDefault>(platformInfo);
}

extern "C" void softswitchMain
(
 unsigned /*shardSize*/,
 const void */*constBLOB*/,
 void */*mutableBLOB*/
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
    cell.nhoodSize=1;
    if(tinselId()==0){
        cell.nhoodAddresses[0]=1;
        cell.directionToNeighbours[2][1]=0;
    }else{
        cell.nhoodAddresses[0]=0;
        cell.directionToNeighbours[0][1]=0;
    }
    
    real_t split=real_t::from_double(1.0);
    
    cell.xBegin=tinselId()==0 ? info.left : split;
    cell.xEnd=tinselId()==0 ? split : info.right+real_t::eps();
    
    cell.yBegin=info.bottom;
    cell.yEnd=info.top+real_t::eps();

    if(tinselId()==0){
        for(unsigned i=0; i<N; i++){
            particle_t p;
            p.position=vector_t::from_double(unif(),unif());
            p.velocity=vector_t::from_double(unif()*0.1,unif()*0.1);
            p.colour=urng()%3;
            p.id=i;
            cell.owned.add(p);
        }
    }

    tinselLogSoft(2, "Messages per thread = %u", TinselMsgsPerThread);

    volatile void *sendSlot=tinselSlot(0);
    for(int i=1; i<TinselMsgsPerThread;i++){
        tinselLogSoft(5, "Allocating slot %u", i);
        tinselAlloc(tinselSlot(i));
    }

    int steps=0;

    auto log_particle=[&](const particle_t &p){
        static std::mutex logMutex;
        std::unique_lock<std::mutex> lock(logMutex);
        
        std::cout<<steps<<","<<steps*dt<<", "<<p.id<<","<<p.colour<<","<<p.position.x.to_double()<<","<<p.position.y.to_double()<<","<<p.velocity.x.to_double()<<","<<p.velocity.y.to_double()<<"\n";
    };

    while(1){       
        cell.cell_step(info, (void*)sendSlot);

        for(unsigned i=0; i<cell.owned.size(); i++){
            log_particle(cell.owned[i]);
        }
        for(unsigned i=0; i<cell.nhoodSize; i++){
            for(unsigned j=0; j<cell.particlesOut[i].size(); j++){
                log_particle(cell.particlesOut[i][j]);
            }
        }
        
        

        std::cout.flush();

        if((steps%10)==0){
            std::cerr<<"Step "<<steps<<"\n";
        }

        if(++steps>200){
            break;
        }
    }

}
