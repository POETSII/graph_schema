#include "particle.hpp"
#include "cell.hpp"

#include "tinsel_host_default.hpp"

#include <random>
#include <iostream>

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
    
    const unsigned N=1024;
    
    const double dt=1.0/128;
    const double mass=1.0;

    const double horizon=0.5;
    const double drag=0.9;
    const double thermal=0.1;

    
    const real_t delta=real_t::from_double(horizon);

    int wCell=4;
    int hCell=4;
    
    world_info_t info(dt, mass, horizon, drag, thermal);
    info.left=real_t::from_double(-0.5);
    info.right=info.left +real_t::from_int(wCell)*delta -real_t::eps();
    info.bottom=real_t::from_double(-0.5);
    info.top=info.bottom+real_t::from_int(hCell)*delta - real_t::eps();
    
    int xCell=tinselId()%wCell;
    int yCell=tinselId()/wCell;

    cell_t cell;

    auto add_neighbour=[&](int dx, int dy)
        {
            cell.directionToNeighbours[1+dx][1+dy]=-1;
            if(dx==0 && dy==0) return;
            int ex=xCell+dx, ey=yCell+dy;
            if(ex<0 || ex>=wCell || ey<0 || ey>=hCell) return;
            cell.nhoodAddresses[cell.nhoodSize]=ex+wCell*ey;
            cell.directionToNeighbours[1+dx][1+dy]=cell.nhoodSize;

            // Debugging
            cell.xBeginNeighbour[cell.nhoodSize]=info.left+real_t::from_int(ex)*delta;
            cell.xEndNeighbour[cell.nhoodSize]=info.left+real_t::from_int(ex+1)*delta;
            cell.yBeginNeighbour[cell.nhoodSize]=info.bottom+real_t::from_int(ey)*delta;
            cell.yEndNeighbour[cell.nhoodSize]=info.bottom+real_t::from_int(ey+1)*delta;

            cell.nhoodSize++;
        };

    for(int dx=-1;dx<=+1;dx++){
        for(int dy=-1;dy<=+1;dy++){
            add_neighbour(dx,dy);
        }
    }

    static std::mutex tmp;
    std::unique_lock<std::mutex> lk(tmp);
    fprintf(stderr, "(%d,%d) = %d\n", xCell, yCell, tinselId());
    for(unsigned y=0; y<3; y++){
        for(unsigned x=0; x<3; x++){
            int d=cell.directionToNeighbours[x][y];
            if(d!=-1){
                d=cell.nhoodAddresses[d];
            }
            fprintf(stderr, "  %04d", d);
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
    lk.unlock();
    
    cell.xBegin=info.left + real_t::from_int(xCell) * delta  ;
    cell.xEnd=info.left + real_t::from_int(xCell+1) * delta;
    
    cell.yBegin=info.bottom + real_t::from_int(yCell) * delta;
    cell.yEnd=info.bottom + real_t::from_int(yCell+1) * delta;

    tinselLogSoft(0, "range = [%f,%f) x [%f,%f), nhood=%u",
                  cell.xBegin.to_double(), cell.xEnd.to_double(),
                  cell.yBegin.to_double(), cell.yEnd.to_double(),
                  cell.nhoodSize);

    for(unsigned i=0; i<N; i++){
        particle_t p;
        p.position=vector_t::from_double(unif(),unif());
        p.velocity=vector_t::from_double(unif()*0.1-0.05,unif()*0.1-0.05);
        p.colour=urng()%3;
        p.id=i;
        if( cell.xBegin <= p.position.x && p.position.x < cell.xEnd
            && cell.yBegin <= p.position.y && p.position.y < cell.yEnd){
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
        
        std::cout<<steps<<","<<steps*dt<<", "<<p.id<<","<<p.colour<<","<<p.position.x.to_double()<<","<<p.position.y.to_double()<<","<<p.velocity.x.to_double()<<","<<p.velocity.y.to_double()<<","<<tinselId()<<"\n";
        std::cout.flush();
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

        if(++steps>1000){
            break;
        }
    }

}
