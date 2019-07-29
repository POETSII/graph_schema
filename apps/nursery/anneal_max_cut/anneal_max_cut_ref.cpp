#include <vector>
#include <cstdint>
#include <random>
#include <algorithm>

class anneal_graph
{
    struct edge
    {
        uint32_t cell_index;
        int32_t weight;
    };

    struct anneal_cell
    {
        unsigned index;
        uint32_t rng;
        std::vector<edge> edges;

        uint32_t urng()
        {
            rng = rng *1664525+1013904223;
            return rng;
        }
    };

    std::mt19937 urng;
    std::vector<unsigned> order;
    std::vector<anneal_cell> cells;

public:
    int32_t energy(const int8_t *spins)
    {
        int32_t res=0;
        for(auto &cell : cells){
            int spin=spins[cell.index];
            for(const auto &edge : cell.edges){
                res += edge.weight * (spin != spins[edge.cell_index]);
            }
        }
        return -res;
    }

    void step(uint32_t randomFlipProb, int8_t *spins)
    {
        std::shuffle(order.begin(), order.end(), urng);    

        for(auto &cell : cells){
            int spin=spins[cell.index];

            int nhood_pos_spin=0;
            int nhood_neg_spin=0;
            for(const auto &edge : cell.edges){
                nhood_pos_spin += edge.weight * (1+spins[edge.cell_index]);
                nhood_neg_spin += edge.weight * (1-spins[edge.cell_index]);
            }


            if(nhood_neg_spin > nhood_pos_spin){
                spin=-1;
            }else if(nhood_neg_spin < nhood_pos_spin){
                spin=+1;
            }else{
                // Flip? What else to do.
                spin=(cell.urng()>>31) ? +1 : -1;
            }

            if(cell.urng() < randomFlipProb){
                spin=-spin;
            }

            spins[cell.index]=spin;
        }
    }

    anneal_graph(int nCells, unsigned seed)
    {
        urng.seed(seed);
        
        cells.resize(nCells);
        for(unsigned i=0; i<nCells; i++){
            cells[i].rng=urng();
            cells[i].index=i;
            order.push_back(i);
        }
    }

    void add_coupling(unsigned a, unsigned b, int32_t w)
    {
        cells.at(a).edges.push_back(edge{b,w});
        cells.at(b).edges.push_back(edge{a,w});
    }
};

int main()
{
    int n=100000;

    std::mt19937 urng;
    anneal_graph graph(n, urng());

    for(int i=0; i<n; i++){
        graph.add_coupling(i, (i+1)%n, 2*(urng()%4)-3);
    }
    for(int j=0; j<10; j++){
        for(int i=0; i<n; i++){
            graph.add_coupling(i, urng()%n, 2*(urng()%4)-3);
        }
    }

    uint32_t flipProb=0xFFFFFFFFull>>4;
    float cooling=0.99;
    std::vector<int8_t> spins(n);

    for(int i=0; i<n; i++){
        spins[i]=urng()&1 ? +1 : -1;
    }

    int maxE=0;
    for(int i=0; i<1000; i++){
        graph.step(flipProb, &spins[0]);

        int e=graph.energy(&spins[0]);
        if(e>maxE){
            maxE=e;
        }
        fprintf(stderr, "%u, prob=%u, MaxEnergy = %d, e=%d, sum=%d\n", i, flipProb, maxE, e, std::accumulate(spins.begin(), spins.end(), 0));

        flipProb=flipProb * cooling;
    }
}
