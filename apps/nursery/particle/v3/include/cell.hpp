#ifndef cell_hpp
#define cell_hpp

#include "particle.hpp"

#include "tinsel_emu.h"
#include <cstring>

//#include <unordered_set>

const unsigned MM=1024;

/*
Phases:
  1 - Send outgoing particles
  2 - Send our particles as ghosts to neighbours
  3 - Receive incoming particles
  4 - Update intra-cell forces
  5 - Receive other particles as ghosts from neighbours
  6 - Send step ack
  7 - Update inter-cell forces
  8 - Move all particles
  9 - Place cells that have crossed boundaries into outgoing particles
  10 - Wait for all step acks
*/


enum message_types{
    message_type_halo,
    message_type_particles,
    message_type_step_ack
};

struct message_t
{
    uint32_t hash; // Hash over the entire message with hash==0
    uint32_t length;
    uint32_t source;
    uint32_t type;//  :  8;
    uint32_t final;//  :  1;
    uint32_t count;//  :  7;
    uint32_t step;//   : 16;
    uint32_t id; // Combination of host id and sequence number
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


inline uint32_t calc_hash(const message_t *msg)
{
    const uint8_t *pRaw=(const uint8_t*)msg;

    uint32_t hash=0;
    for(unsigned i=4; i<msg->length; i++){
        hash = hash*19937 + pRaw[i];
    }
    return hash;
}


template<class T, uint32_t SIZE>
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
        unsigned n=std::min(uint32_t(MAXN), count);
        std::copy(entries+count-n, entries+count, msg->elements);
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
    // Note: _everything_ in here can be zero-initialised. That is to
    // allow us to avoid having the code for the default constructor
    // where possible
    cell_t()
    {
        // This is just to prove that it works. Ideally we never use it.
        #ifndef NDEBUG
        memset(this, 0, sizeof(cell_t));
        #endif
    }
    
    // Create an empty cell_t in the given space
    static cell_t &construct(uint8_t *space, unsigned size)
    {
        assert(size >= sizeof(cell_t));
        memset(space, 0, sizeof(cell_t));
        return *(cell_t*)space;
    }
    
    uint32_t step;
    
    uint32_t nhoodSize; // Number of neighbours
    uint32_t nhoodAddresses[8]; // Addresses of neighbours
    
    int32_t directionToNeighbours[3][3]; // Map of direction neighbours to neighbour indices (-1 is invalid)
    real_t xBegin, xEnd, yBegin, yEnd; // Boundaries of this cell. We own [xBegin,xEnd) x [yBegin,yEnd)

    // Pure debugging
    real_t xBeginNeighbour[8];
    real_t xEndNeighbour[8];
    real_t yBeginNeighbour[8];
    real_t yEndNeighbour[8];
    
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

    unsigned stepAcksDone; // Uses to avoid double-buffering

    unsigned seq=0; // Tracks messages sent

    //std::unordered_set<uint32_t> seenMessages;

    void cell_on_recv(const volatile message_t *msg){
        auto type=msg->type;
        #ifndef NDEBUG
        tinselLogSoft(3, "Recv: id=%u, type=%u, hash=%u, length=%u", msg->id, msg->type, msg->hash, msg->length);
        uint32_t ghash=calc_hash((message_t*)msg);
        if(ghash!=msg->hash){
            tinselLogSoft(0, "Hash failed");
            tinselLogSoft(0, "id=%u, type=%u, hash=%u, length=%u; calchash=%u", msg->id, msg->type, msg->hash, msg->length, ghash);

            /*for(unsigned i=0; i<msg->length; i++){
                tinselLogSoft(0, "  msg[%u]=%u", i, ((uint8_t*)msg)[i]);
                }*/
            assert(0);
        }
        /*if(seenMessages.find((unsigned)msg->id)!=seenMessages.end()){
            tinselLogSoft(0, "Duplicate message received.");
            exit(1);
        }
        seenMessages.insert((unsigned)msg->id);*/
        #endif
        
        switch (type){
        case message_type_particles:
        {
            auto sMsg=(const particle_message_t*)msg;
            #ifndef NDEBUG
            for(unsigned i=0; i<sMsg->count; i++){
                const auto &particle=sMsg->elements[i];
                tinselLogSoft(2, "Received particle %u at (%d,%d)", particle.id, particle.position.x.raw(), particle.position.y.raw());
            }
            #endif
            particlesIn.add(sMsg->count,sMsg->elements);
            if(sMsg->final){
                particlesInDone++;
            }
            tinselLogSoft(2, "Received %u particles, final=%u, particlesInDone=%u, particlesInCount=%u\n", sMsg->count, sMsg->final,particlesInDone, particlesIn.size());
            assert( particlesInDone <= nhoodSize);
            break;
        }
        case message_type_halo:
        {
            auto sMsg=(const halo_message_t*)msg;
            halosIn.add(sMsg->count, sMsg->elements);
            if( sMsg->final ){
                halosInDone++;
            }
            tinselLogSoft(2, "Received %u ghosts, final=%u, halosInDone=%u, nhoodSize=%u\n", sMsg->count, sMsg->final, halosInDone, nhoodSize);
            assert( halosInDone <= nhoodSize);
            break;
        }
        case message_type_step_ack:
        {
            stepAcksDone++;
            tinselLogSoft(2, "Received stepAck, got=%u, needed=%u\n", stepAcksDone, nhoodSize);
            assert( stepAcksDone <= nhoodSize);
            break;
        }
        default:
            tinselLogSoft(0, "Uknonwn tag\n");
            assert(0);
            break;
        }
    }

    void cell_receive_impl()
    {
        assert(tinselCanRecv());
            
        auto pMsg=tinselRecv();
        cell_on_recv((volatile message_t*)pMsg);
        
        #ifndef NDEBUG
        memset((void*)pMsg, 0, TinselBytesPerMsg);
        #endif
        tinselAlloc(pMsg);
    }
    
    
    // Receive a single message
    void cell_receive()
    {
        tinselWaitUntil(TINSEL_CAN_RECV);
        cell_receive_impl();
    }

    void cell_send(uint32_t dest, unsigned len, volatile void *slot)
    {
        while(!tinselCanSend()){
            if(tinselCanRecv()){
                cell_receive_impl();
            }else{
                tinselWaitUntil(TINSEL_CAN_SEND_OR_RECV);
            }        
        }
        volatile message_t *msg=(volatile message_t*)slot;
        msg->id = ((seq++)<<16) | tinselId();
        msg->length=len;
        msg->source=tinselId();
        msg->step=step;

        #ifndef NDEBUG
        msg->hash=calc_hash((message_t*)msg);
        #endif

        tinselLogSoft(3, "Send: id=%u, type=%u, hash=%u, length=%u",
                      msg->id, msg->type, msg->hash, msg->length);
        /*for(unsigned i=0; i<msg->length; i++){
            tinselLogSoft(0, "  msg[%u]=%u", i, ((uint8_t*)msg)[i]);
            }*/

        tinselSetByteLen(len);
        tinselSend(dest,(volatile void *)slot);
    }

    void cell_step(
                   world_info_t &world,
                   void *sendSlot
                   ){
        tinselLogSoft(1, "Begin step %d", step);
        
        ////////////////////////////////////////////
        // Send all particles
        {
            particle_message_t *pParticleMsg=(particle_message_t*)sendSlot;
            
            for(unsigned i=0; i<nhoodSize; i++){
                if(particlesOut[i].size()>0){
                    tinselLogSoft(2, "Sending particles to neighbour %u, total=%u\n",
                                  nhoodAddresses[i], particlesOut[i].size());
                }
                
                bool keepGoing=true;
                
                // Must always send at least one message, even if we don't
                // own anything
                while(keepGoing){
                    pParticleMsg->type=message_type_particles;
                    keepGoing=particlesOut[i].fill(pParticleMsg);
                    tinselLogSoft(3, "Sending particle message to %u, count=%u, final=%u\n", pParticleMsg->id, nhoodAddresses[i], pParticleMsg->count, pParticleMsg->final);
                                      
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
            
            unsigned haloDone=0;
            bool anySent=false;
            
            // Must always send at least one message, even if we don't
            // own anything
            while(!anySent || haloDone < owned.size()){
                
                unsigned todo=std::min(owned.size()-haloDone, (unsigned)halo_message_t::max_entries);
                pHaloMsg->type=message_type_halo;
                pHaloMsg->final = todo+haloDone==owned.size();
                pHaloMsg->count = todo;
                tinselLogSoft(2, "Preparing halo of count=%u, total=%u, final=%u\n", pHaloMsg->count, owned.size(), pHaloMsg->final);
                
                for(unsigned i=0; i<todo; i++){
                    pHaloMsg->elements[i].position=owned[haloDone+i].position;
                    pHaloMsg->elements[i].colour=owned[haloDone+i].colour;
                }
                
                for(unsigned i=0; i<nhoodSize; i++){
                    tinselLogSoft(3, "Delivering halo to neighbour %u (id=%x)\n", nhoodAddresses[i], pHaloMsg->id);
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
            tinselLogSoft(2, "Wiating for incoming particles: got %u out of %u", particlesInDone, nhoodSize);
            cell_receive();
        }
        if(particlesIn.size()>0){
            tinselLogSoft(2, "Adding %u new particles.", particlesIn.size());
            #if 0
            for(unsigned i=0; i<particlesIn.size(); i++){
                auto &p=particlesIn[i];
                if(p.position.x < xBegin || xEnd <= p.position.x
                   || p.position.y < yBegin || yEnd <= p.position.y ){
                    tinselLogSoft(0, "Received a particle at (%f,%f) that is not in range [%f,%f)x[%f,%f)",
                                  p.position.x.to_double(), p.position.y.to_double(),
                                  xBegin.to_double(), xEnd.to_double(),
                                  yBegin.to_double(), yEnd.to_double());
                    assert(0);
                }
            }
            #endif
            owned.add(particlesIn.size(), particlesIn.entries);
        }
        
        ///////////////////////////////////////////////////////////
        // Update all of our particles

        tinselLogSoft(2, "Calculating intra forces over %u particles.", owned.size());
        calculate_particle_intra_forces(
                                        world,
                                        owned.size(),
                                       owned.entries,
                                        forces
                                        );
        particlesInDone=0;
        particlesIn.clear();
        
        
        ///////////////////////////////////////////////////////////
        // Wait for all incoming halos
        // Note: this could be done progressively and overlapped
        
        while(halosInDone < nhoodSize){
            tinselLogSoft(2, "Waiting for incoming halos. Got %u out of %u.", halosInDone, nhoodSize);
            cell_receive();
        }

       
        ///////////////////////////////////////////////////////////
        // Apply inter forces from halos

        tinselLogSoft(2, "Calculating inter forces.");
        calculate_particle_inter_forces(
                                        world,
                                        owned.size(),
                                        owned.entries,
                                        forces,
                                        halosIn.size(),
                                        halosIn.entries
                                        );
        halosInDone=0;
        halosIn.clear();

        ////////////////////////////////////////////////////////////
        // Send out our step acks
        // TODO: move this earlier again

        {
            message_t *pAckMsg=(message_t*)sendSlot;
            pAckMsg->type=message_type_step_ack;

            
            for(unsigned i=0; i<nhoodSize; i++){
                tinselLogSoft(2,"Sending ack to thread %u", nhoodAddresses[i]);
                cell_send(nhoodAddresses[i], sizeof(message_t), pAckMsg);
            }
        }

        
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
                tinselLogSoft(3, "Send particle %u at (%d,%d) in dir (%d,%d)", particle.id, particle.position.x.raw(), particle.position.y.raw(), xDir, yDir);
                unsigned ni=directionToNeighbours[xDir+1][yDir+1];
                /*if( particle.position.x < xBeginNeighbour[ni] || xEndNeighbour[ni] <= particle.position.x
                    || particle.position.y < yBeginNeighbour[ni] || yEndNeighbour[ni] <= particle.position.y){
                    tinselLogSoft(0, "Attempting to send to wrong cell.");
                }*/
                
                particlesOut[ni].add(particle);
                owned.remove(i);
            }
        }

        ///////////////////////////////////////////////////////////
        // Wait until we get all step acks

        while(stepAcksDone < nhoodSize){
            tinselLogSoft(2, "Waiting for step acks. Got %u out of %u.", stepAcksDone, nhoodSize);
            cell_receive();
        }
        stepAcksDone=0;

        unsigned particlesOutPending=0;
        for(unsigned i=0;i<nhoodSize;i++){
            particlesOutPending+=particlesOut[i].size();
        }
        tinselLogSoft(2, "Bottom: owned=%u, leaving=%u, halosInDone=%u, ghostsInDone=%u, stepsAckDone=%u",
                      owned.size(), particlesOutPending, halosInDone, particlesInDone, stepAcksDone);

        
        //////////////////////////////////////////////////////////
        // Woo! Done.        
        
        step++;
    }
};
    
#endif
