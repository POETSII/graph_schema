#include "parameters.hpp"

#include "particle.hpp"
#include "cell.hpp"
#include "state_machine.hpp"

#include "tinsel_host_default.hpp"

#include <random>

class CellGenericHost
    : public HostDefault
{
private:
    unsigned m_W, m_H;
    unsigned m_totalParticles;
    unsigned m_totalTimesteps;

    std::vector<tinsel_address_t> m_threads;

    std::vector<sm> m_sms;
    unsigned m_numFinished=0;

public:
    CellGenericHost(PlatformInfoPtr platform)  
        : HostDefault(platform)
        , m_W(CELL_W)
        , m_H(CELL_H)
        , m_totalParticles(PARTICLES_PER_CELL*CELL_W*CELL_H)
        , m_totalTimesteps(TIME_STEPS)
    {
        auto topology=platform->getTopology();
        
        topology->getThreads(m_threads);
        
        if(m_W*m_H > m_threads.size()){
            throw std::runtime_error("Not enough threads to run this grid.");
        }
        if(m_threads[0]!=0 || !topology->hasContiguousThreads()){
            // Currently need this so that threads can set up their own connectivity
            throw std::runtime_error("This host needs contiguous threads starting at zero.");
        }
        
        for(unsigned i=0; i<m_W*m_H; i++){
		sm s;
                s.thread=i;
            m_sms.push_back( s );
        }
    }
    
    void onThreadToHostPut(
        tinsel_address_t source,
        unsigned numWords,
        const uint32_t *pWords
    ) override {
        auto &sm=m_sms.at(source);
        
        bool more=true;
        for(unsigned i=0; i<numWords;i++){
            more=sm(pWords[i]);
        }
        if(!more){
            m_numFinished++;
            if(m_numFinished==m_W*m_H){
                fprintf(stderr, "Finished all\n");
                exit(0);
            }
        }
    }
};

HostPtr hostCreate(std::shared_ptr<PlatformInfo> platformInfo, int argc, const char **argv)
{
    return std::make_shared<CellGenericHost>(platformInfo);
}
