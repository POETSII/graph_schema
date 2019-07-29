#include <cstdint>
#include <vector>
#include <queue>
#include <cmath>
#include <cstdio>
#include <cassert>

#include "ising_spin_shared.hpp"

#include "standard_probabilities.hpp"

extern "C" void get_ising_probabilities(
    double T, double J, double H, uint32_t *probabilities
){
    unsigned n=sizeof(standard_probabilities)/sizeof(standard_probabilities[0]);
    for(unsigned i=0; i<n; i++){
        auto &entry=standard_probabilities[i];
        if(entry.T==T && entry.H==H && entry.J==J){
            std::copy(entry.probabilities, entry.probabilities+10, probabilities);
            return;
        }
    }

    fprintf(stderr, "No standard probabilities for T=%f, H=%g, J=%f", T, H, J);
    exit(1);
}

struct ising_spin_model
{


    struct ising_spin_cell
    {
        uint32_t x;
        uint32_t y;
        uint32_t rng;   
        uint32_t tag;
        uint32_t nextTime;
        int32_t spin;

        void init(uint32_t _x, uint32_t _y)
        {
            x=_x;
            y=_y;

            cell_init(_x, _y, spin, nextTime, rng);
        }
    };

    struct time_greater_than
    {
        bool operator()(const ising_spin_cell *x, const ising_spin_cell *y) const
        {
            return x->nextTime > y->nextTime;
        };
    };

    using queue_t = std::priority_queue<ising_spin_cell *, std::vector<ising_spin_cell*>, time_greater_than>;
    
    unsigned m_w, m_h;
    uint32_t m_maxTime;

    uint32_t m_probabilities[10];
    std::vector<ising_spin_cell> m_cells;
    queue_t m_queue;

public:

    ising_spin_model(unsigned w, unsigned h, unsigned max_time=100, double T=1, double J=1, double H=0)
        : m_w(w)
        , m_h(h)
        , m_maxTime(max_time*(1<<20))
    {
        fprintf(stderr, "creating model: w=%u, h=%u, maxTime=%u, T=%f, J=%f, H=%f\n", w, h, max_time, T, J, H);

        get_ising_probabilities(T,H,J,m_probabilities);
        for(unsigned i=0; i<10; i++){
            fprintf(stderr, "p[%u]=%u\n", i, m_probabilities[i]);
        }
    
        m_cells.resize(w*h);

        for(unsigned y=0; y<h; y++){
            for(unsigned x=0; x<w; x++){
                m_cells[y*w+x].init(x,y);
                m_queue.push(&m_cells[y*w+x]);
            }
        }
    }

    void snapshot(FILE *dst)
    {
        std::vector<char> buffer;
        buffer.resize(m_w+2);
        buffer[m_w]='\n';
        buffer[m_w+1]=0;

        for(unsigned y=0; y<m_h; y++){
            for(unsigned x=0; x<m_w; x++){
                buffer[x]=m_cells[y*m_w+x].spin>0 ? '+' : '-';
            }
            fputs(&buffer[0], dst);
        }
    }

    void dump_rngs(FILE *dst)
    {
        for(unsigned y=0; y<m_h; y++){
            for(unsigned x=0; x<m_w; x++){
                fprintf(dst, " %08x", m_cells[y*m_w+x].rng);
            }
            fprintf(dst, "\n");
        }
    }

    void dump_rng_lsbs(FILE *dst)
    {
        for(unsigned y=0; y<m_h; y++){
            for(unsigned x=0; x<m_w; x++){
                fprintf(dst, " %u", m_cells[y*m_w+x].rng>>31);
            }
            fprintf(dst, "\n");
        }
    }

    void run()
    {
        fprintf(stderr, "running model\n");

        unsigned w=m_w, h=m_h;
        unsigned maxTime=m_maxTime;
        auto &cells=m_cells;
        auto &queue=m_queue;

        int nextSnap=0;
        while(true){
            auto head=queue.top();
            //fprintf(stderr, "  (%u,%u) : time=%u\n", head->x, head->y, head->nextTime);

            /* if(head->nextTime >= nextSnap){
                fprintf(stdout, "\n%f\n", nextSnap/double(1<<20));
                snapshot(stderr);
                nextSnap+=(1<<20);
            }*/

            if(head->nextTime >= m_maxTime){
                break;
            }

            queue.pop();
            auto x=head->x, y=head->y;
            auto left= x - 1 + (x==0)*w;
            auto right= x + 1 - (x==w-1)*w;
            auto up= y - 1 + (y==0)*h;
            auto down= y + 1 - (y==h-1)*h;
            assert(0<=left && left<w);
            assert(0<=right && right<w);
            assert(0<=up && up<h);
            assert(0<=down && down<h);
            int spins[5]={
                head->spin,
                cells[ y * w + left ].spin, cells[ y * w + right ].spin,
                cells[ up * w + x ].spin, cells[ down * w + x].spin
            };

            auto prevRng=head->rng;
            auto prevSpin=head->spin;
            auto prevTime=head->nextTime;

            chooseNextEvent(m_probabilities, head->tag, head->rng, spins, head->nextTime);
            head->spin=spins[0];

            /* fprintf(stderr, "  UPDATE: (%u,%u) : (t,s,rng)=(%u,%d,%08x) + {%d,%d,%d,%d} -> (%u,%d,%08x)\n",
                head->x, head->y,
                prevTime, prevSpin, prevRng,
                spins[1],spins[2],spins[3],spins[4],
                head->nextTime, head->spin, head->rng
            );*/


            queue.push(head);
        }
    }
};


int main(int argc, char *argv[])
{
    int n=16;
    unsigned maxTime=100;
    double T=1, J=1, H=0;

    if(argc>1){
        n=atoi(argv[1]);
    }
    if(argc>2){
        T=strtod(argv[2], 0);
    }
    if(argc>3){
        maxTime=atoi(argv[3]);
    }
    if(argc>4){
        J=strtod(argv[4], 0);
    }
    if(argc>5){
        H=strtod(argv[5], 0);
    }

    ising_spin_model model(n,n, maxTime, T, J, H);

    //model.dump_rng_lsbs(stdout);
    //model.snapshot(stdout);
    //return 0;

    model.run();
    model.snapshot(stdout);
}