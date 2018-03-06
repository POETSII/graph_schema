#ifndef cell_hpp
#define cell_hpp

#include "particle.hpp"

#include "tinsel_emu.h"

const unsigned MM=64;

/*
Phases:
  1 - Send outgoing particles
  2 - Send our particles as ghosts to neighbours
  3 - Receive incoming particles
  4 - Update intra-cell forces
  5 - Receive other particles as ghosts from neighbours
  6 - Update inter-cell forces
  7 - Move all particles
  8 - Place cells that have crossed boundaries into outgoing particles

*/


enum message_types{
    message_type_halo,
    message_type_particles
};

struct message_t
{
    uint32_t source;
    uint32_t type   :  8;
    uint32_t final  :  1;
    uint32_t count  :  7;
    uint32_t step   : 16;
};

template<class T,unsigned MAX>
struct array_message_t
    : message_t
{
    static const int max_entries=MAX;
    
    T elements[MAX];    
};

typedef array_message_t<
    particle_t,
    (256-sizeof(message_t))/sizeof(particle_t)
    > particle_message_t;

typedef array_message_t<
    ghost_t,
    (256-sizeof(message_t))/sizeof(ghost_t)
    > halo_message_t;



template<class T, unsigned SIZE>
struct Buffer
{
    uint32_t count;
    T entries[SIZE];
    
    bool empty() const
    { return count==0; }

    unsigned size() const
    { return count; }
    
    void clear()
    { count=0; }
    
    void add(const T &p)
    {
        assert(1+count <= SIZE);
        entries[count]=p;
        count++;
    }
    
    void add(unsigned n, const T *p)
    {
        assert(n+count <= SIZE);
        std::copy(p, p+n, entries+count);
        count+=n;
    }
    
    // Remove an element. This preserves elements in [0,index), and takes O(1)
    void remove(unsigned index)
    {
        assert(index<count);
        std::swap( entries[index], entries[count-1] );
        --count;
    }
    
    // Remove up to max_n entries.
    // Return true if there are more to send
    template<unsigned MAXN>
    bool fill(array_message_t<T,MAXN> *msg)
    {
        unsigned n=std::min(MAXN, count);
        std::copy(entries+SIZE-n, entries+SIZE, msg->elements);
        count-=n;
        bool final=count==0;
        msg->count=n;
        msg->final=final;
        return !final;
    }

    T &operator[](unsigned x)
    {
        assert(x<count);
        return entries[x];
    }

    const T &operator[](unsigned x) const 
    {
        assert(x<count);
        return entries[x];
    }
};



struct cell_t
{
    uint32_t step;
    
    uint32_t nhoodSize; // Number of neighbours
    uint32_t nhoodAddresses[8]; // Addresses of neighbours
    
    int32_t directionToNeighbours[3][3]; // Map of direction neighbours to neighbour indices (-1 is invalid)
    real_t xBegin, xEnd, yBegin, yEnd; // Boundaries of this cell. We own [xBegin,xEnd) x [yBegin,yEnd)
    
    Buffer<particle_t,MM> owned; // Particles that we own
    vector_t forces[MM];
    
    // Particles which are arriving for this time step
    uint32_t particlesInDone;
    Buffer<particle_t,MM> particlesIn;
        
    uint32_t halosInDone; // Count how many ghosts remain to be processed
    Buffer<ghost_t,8*MM> halosIn;
    
    uint32_t particlesOutDone;
    Buffer<particle_t,MM> particlesOut[8];
    
    uint32_t halosOutDone; // Count how many people we have sent all our ghosts to
    

    void cell_on_recv(const void *msg){
        switch (((const message_t*)msg)->type){
        case message_type_particles:
        {
            auto sMsg=(const particle_message_t*)msg;
            particlesIn.add(sMsg->count,sMsg->elements);
            if(sMsg->final){
                particlesInDone++;
            }
            break;
        }
        case message_type_halo:
        {
            auto sMsg=(const halo_message_t*)msg;
            halosIn.add(sMsg->count, sMsg->elements);
            if( sMsg->final ){
                halosInDone++;
            }
            break;
        }
        default:
            assert(0);
            break;
        }
    }

    void cell_send(uint32_t dest, unsigned len, const void *slot)
    {
        while(!tinselCanSend()){
            if(tinselCanRecv()){
                auto pMsg=tinselRecv();
                cell_on_recv((void*)pMsg);
                tinselAlloc(pMsg);
            }else{
                tinselWaitUntil(TINSEL_CAN_SEND_OR_RECV);
            }        
        }
        tinselSetByteLen(len);
        tinselSend(dest,(volatile void *)slot);
    }

    void cell_step(
                   world_info_t &world,
                   void *sendSlot
                   ){
        ////////////////////////////////////////////
        // Send all particles
        {
            particle_message_t *pParticleMsg=(particle_message_t*)sendSlot;
            pParticleMsg->source=tinselId();
            pParticleMsg->type=message_type_particles;
            pParticleMsg->step=step;
            
            for(unsigned i=0; i<nhoodSize; i++){
                bool keepGoing=true;
                
                // Must always send at least one message, even if we don't
                // own anything
                while(keepGoing){
                    keepGoing=particlesOut[i].fill(pParticleMsg);
                    
                    cell_send(nhoodAddresses[i], sizeof(particle_message_t), pParticleMsg);
                }
                assert(particlesOut[i].size()==0);
            }
            particlesOutDone=0;
        }
        
        
        ////////////////////////////////////////////
        // Send all halos
        {
            halo_message_t *pHaloMsg=(halo_message_t*)sendSlot;
            pHaloMsg->source=tinselId();
            pHaloMsg->type=message_type_halo;
            pHaloMsg->step=step;
            
            unsigned haloDone=0;
            bool anySent=false;
            
            // Must always send at least one message, even if we don't
            // own anything
            while(!anySent || haloDone < owned.size()){
                unsigned todo=std::min(owned.size()-haloDone, (unsigned)halo_message_t::max_entries);
                pHaloMsg->final = todo+haloDone==owned.size();
                pHaloMsg->count = todo;
                for(unsigned i=0; i<todo; i++){
                    pHaloMsg->elements[i].position=owned[haloDone+i].position;
                    pHaloMsg->elements[i].colour=owned[haloDone+i].colour;
                }
                
                for(unsigned i=0; i<nhoodSize; i++){
                    cell_send(nhoodAddresses[i], sizeof(halo_message_t), pHaloMsg);
                }
                haloDone+=todo;
                anySent=true;
            }
        }
        
        ///////////////////////////////////////////////////////////
        // Wait for incoming particles to complete, before doing all intra
        // Note: this could be overlapped with calculating intra
        
        while(particlesInDone < nhoodSize){
            tinselWaitUntil(TINSEL_CAN_RECV);
        }        
        
        ///////////////////////////////////////////////////////////
        // Update all of our particles
        
        calculate_particle_intra_forces(
                                        world,
                                        owned.size(),
                                       owned.entries,
                                        forces
                                        );
        // TODO : DANGER! This needs double-buffering, I think
        particlesInDone=0;
        particlesIn.clear();
        
        
        ///////////////////////////////////////////////////////////
        // Wait for all incoming halos
        // Note: this could be done progressively and overlapped
        
        while(halosInDone < nhoodSize){
            tinselWaitUntil(TINSEL_CAN_RECV);
        }
        
        ///////////////////////////////////////////////////////////
        // Apply inter forces from halos
        
        calculate_particle_inter_forces(
                                        world,
                                        owned.size(),
                                        owned.entries,
                                        forces,
                                        halosIn.size(),
                                        halosIn.entries
                                        );
        // TODO : DANGER! This needs double-buffering, I think
        halosInDone=0;
        halosIn.clear();
        
        ///////////////////////////////////////////////////////
        // Move the particles
        update_particles(
                         world,
                         owned.size(),
                         owned.entries,
                         forces
                         );
        
        ///////////////////////////////////////////////////////////
        // Work out if particles need to move
        for(int i=owned.size()-1; i>=0; i--){
            auto &particle=owned.entries[i];
            int xDir=0, yDir=0;
            if(particle.position.x < xBegin){
                xDir=-1;
            }else if(particle.position.x >= xEnd){
                xDir=+1;
            }
            if(particle.position.y < yBegin){
                yDir=-1;
            }else if(particle.position.y >= yEnd){
                yDir=+1;
            }
            
            if(xDir!=0 || yDir!=0){
                unsigned neighbourIndex=directionToNeighbours[xDir+1][yDir+1];
                particlesOut[neighbourIndex].add(particle);
                owned.remove(i);
            }
        }
        
        //////////////////////////////////////////////////////////
        // Woo! Done.        
        
        step++;
    }
};
    
#endif
