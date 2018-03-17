#include "particle.hpp"
#include "cell.hpp"


extern "C" void softswitchMain
(
 unsigned /*shardSize*/,
 const void */*constBLOB*/,
 void */*mutableBLOB*/
)
{
    // Check that the two biggests things are no more than 2/3 of stack
    static_assert( (sizeof(cell_t)+sizeof(world_info_t)) * 3 <= (2<<20), "Stack data is too large for hardware.");
    
    
    const double dt=1.0/64;
    const double mass=1.0;

    const double horizon=0.5;
    const double drag=0.9;
    const double thermal=0.1;

    const real_t delta=real_t::from_double(horizon);
    
    uint32_t seed=tinselId();
    for(int i=0; i<32; i++){
        lcg(seed); // stir vigorously
    }
    
    int wCell=tinselHostToThreadGet();
    int hCell=tinselHostToThreadGet();
    unsigned initParticles=tinselHostToThreadGet();
    int xCell=tinselHostToThreadGet();
    int yCell=tinselHostToThreadGet();
    unsigned timeSteps=tinselHostToThreadGet();
    unsigned outputDeltaInterval=tinselHostToThreadGet();
    
    
    world_info_t info(dt, mass, horizon, drag, thermal);
    info.left=real_t::from_double(0.0);
    info.right=info.left +real_t::from_int(wCell)*delta -real_t::eps();
    info.bottom=real_t::from_double(0.0);
    info.top=info.bottom+real_t::from_int(hCell)*delta - real_t::eps();

    // This is to avoid calling or emitting the constructor, which is relatively large
    uint8_t cell_storage[sizeof(cell_t)];
    cell_t &cell=cell_t::construct(cell_storage, sizeof(cell_storage));

    auto add_neighbour=[&](int dx, int dy)
        {
            cell.directionToNeighbours[1+dx][1+dy]=-1;
            if(dx==0 && dy==0) return;
            int ex=xCell+dx, ey=yCell+dy;
            if(ex<0 || ex>=wCell || ey<0 || ey>=hCell) return;
            cell.nhoodAddresses[cell.nhoodSize]=ex+wCell*ey;
            cell.directionToNeighbours[1+dx][1+dy]=cell.nhoodSize;

            cell.nhoodSize++;
        };

    for(int dx=-1;dx<=+1;dx++){
        for(int dy=-1;dy<=+1;dy++){
            add_neighbour(dx,dy);
        }
    }
    
    cell.xBegin=info.left + real_t::from_int(xCell) * delta  ;
    cell.xEnd=info.left + real_t::from_int(xCell+1) * delta;
    
    cell.yBegin=info.bottom + real_t::from_int(yCell) * delta;
    cell.yEnd=info.bottom + real_t::from_int(yCell+1) * delta;

    tinselLogSoft(0, "range = [%d,%d) x [%d,%d), nhood=%u",
                  cell.xBegin, cell.xEnd,
                  cell.yBegin, cell.yEnd,
                  cell.nhoodSize);

    for(unsigned i=0; i<initParticles; i++){
        particle_t p;
        p.position=vector_t{
            cell.xBegin+delta*real_t::random(seed), 
            cell.yBegin+delta*real_t::random(seed)
        };
        p.velocity=vector_t{ real_t::random(seed), real_t::random(seed) };
        p.colour=lcg(seed)>>30;
        p.id=tinselId()*initParticles+i;
        if( cell.xBegin <= p.position.x && p.position.x < cell.xEnd
            && cell.yBegin <= p.position.y && p.position.y < cell.yEnd){
            cell.owned.add(p);
        }
    }

    volatile void *sendSlot=tinselSlot(0);
    for(int i=1; i<TinselMsgsPerThread;i++){
        tinselLogSoft(5, "Allocating slot %u", i);
        tinselAlloc(tinselSlot(i));
    }

    int steps=0;
    int outputCountdown=outputDeltaInterval;

    auto log_particle=[&](const particle_t &p){
        tinselThreadToHostPut(steps);
        tinselThreadToHostPut((p.id<<16) | p.colour);
        tinselThreadToHostPut(p.position.x.raw());
        tinselThreadToHostPut(p.position.y.raw());
    };

    while(1){       
        cell.cell_step(info, (void*)sendSlot);

        if(--outputCountdown==0){
            for(unsigned i=0; i<cell.owned.size(); i++){
                log_particle(cell.owned[i]);
            }
            for(unsigned i=0; i<cell.nhoodSize; i++){
                for(unsigned j=0; j<cell.particlesOut[i].size(); j++){
                    log_particle(cell.particlesOut[i][j]);
                }
            }
            outputCountdown=outputDeltaInterval;
        }

        if(++steps>timeSteps){
            break;
        }
    }
    
    tinselThreadToHostPut(-1);

}
