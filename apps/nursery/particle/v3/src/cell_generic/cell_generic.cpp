#include "particle.hpp"
#include "cell.hpp"

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
    
    struct sm
    {
        sm(unsigned _thread)
            : thread(_thread)
        {}
        
        unsigned thread;
        unsigned state=0;
        uint32_t data[4];
        
        bool operator()(uint32_t x)
        {
            if(state==-1){
                throw std::runtime_error("Received word after thread finished.");
            }
            if(x==-1){
                state=-1;
                return false;
            }
            assert(state<4);
            data[state]=x;
            state++;
            
            if(state==4){
                unsigned steps=data[0];
                unsigned id=data[1]>>16;
                unsigned colour=data[1]&0xFFFF;
                double xpos=ldexp(int32_t(data[2]),-16);
                double ypos=ldexp(int32_t(data[3]),-16);
                fprintf(stdout, "%d, %f, %d, %d, %f, %f, 0, 0, %d\n", steps, steps/64.0, id, colour, xpos, ypos, thread);
                state=0;
            }
            return true;
        }
    };
    
    std::vector<sm> m_sms;
    unsigned m_numFinished=0;

public:
    CellGenericHost(PlatformInfoPtr platform, unsigned D, int totalParticles, int totalTimesteps)  
        : HostDefault(platform)
        , m_W(D)
        , m_H(D)
        , m_totalParticles(totalParticles)
        , m_totalTimesteps(totalTimesteps)
    {
        auto topology=platform->getTopology();
        
        topology->getThreads(m_threads);
        
        if(D*D > m_threads.size()){
            throw std::runtime_error("Not enough threads to run this grid.");
        }
        if(m_threads[0]!=0 || !topology->hasContiguousThreads()){
            // Currently need this so that threads can set up their own connectivity
            throw std::runtime_error("This host needs contiguous threads starting at zero.");
        }
        
        for(unsigned i=0; i<m_W*m_H; i++){
            m_sms.push_back( sm{i} );
        }
    }

    void run()
    {
        unsigned particlesPerCell=m_totalParticles/(m_W*m_H);
        unsigned timeSteps=m_totalTimesteps;
        unsigned outputDeltaSteps=1;
        uint32_t init[7]={m_W, m_H, particlesPerCell, 0, 0, timeSteps, outputDeltaSteps};
        for(unsigned y=0; y<m_H; y++){
            for(unsigned x=0; x<m_W; x++){
                init[3]=x;
                init[4]=y;
                
                m_platformContext->hostToThreadPut(m_threads[y*m_W+x], sizeof(init)/sizeof(init[0]), init);
            }
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
    int D=4;
    if(argc>1){
        D=atoi(argv[1]);
    }
    
    int totalParticles=64;
    if(argc>2){
        totalParticles=atoi(argv[2]);
    }
    
    int totalTimesteps=64;
    if(argc>3){
        totalTimesteps=atoi(argv[3]);
    }
    
    return std::make_shared<CellGenericHost>(platformInfo,D,totalParticles,totalTimesteps);
}
