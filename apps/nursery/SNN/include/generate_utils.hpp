#ifndef generate_utils_hpp
#define generate_utils_hpp

#include "dumb_snn_sink.hpp"

#include <random>
#include <algorithm>

#include "robin_hood.hpp"

template<class TCont, class F>
void sample_by_prob(
    double p,
    const TCont &c,
    const F &f,
    std::mt19937_64 &rng,
    robin_hood::unordered_flat_set<unsigned> &working
){
    size_t n=c.size();
    if( p==1 ){
        for(const auto &x : c){
            f(x);
        }
    }else if( p > 0.125 ){ // Arbitrary threshold
        uint64_t thresh=(uint64_t)ldexp(p,64);
        for(const auto &x : c){
            if(rng() >= thresh){
                f(x);
            }
        }
    }else{
        std::geometric_distribution<> gdist(p);

        assert(gdist.min()==0);

        unsigned off=gdist(rng);
        while(off < c.size()){
             f(c[off]);
            off += 1+gdist(rng);
        }


        /*working.reserve( size_t(n * p * 1.05 ) );
        working.clear();

        double k=std::binomial_distribution(n, p)(rng);

        while(working.size() < k){
            unsigned candidate=rng()%n;
            if(working.insert(candidate).second){
                f(c[candidate]);
            }
        }*/
    }
}

#endif