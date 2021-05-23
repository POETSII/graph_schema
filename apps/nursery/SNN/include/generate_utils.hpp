#ifndef generate_utils_hpp
#define generate_utils_hpp

#include "dumb_snn_sink.hpp"

#include <random>
#include <algorithm>
#include <iostream>

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

template<class TCont, class F>
void sample_exactly_k(
    unsigned k,
    const TCont &c,
    const F &f,
    std::mt19937_64 &rng,
    robin_hood::unordered_flat_set<unsigned> &working
){
    unsigned n=c.size();
    assert(k<=n);

    if(k==nan){
        for(unsigned i=0; i<k; i++){
            f(c[i]);
        }
    }else if(k < (n*3)/4){
        working.clear();
        unsigned done=0;
        while(done<k){
            unsigned i=rng()%n;
            if(working.insert(i).second){
                f(c[i]);
                done++;
            }
        }
    }else{
        working.clear();
        unsigned done=0;
        while(done<n-k){
            unsigned i=rng()%n;
            if(working.insert(i).second){
                done++;
            }
        }
        for(unsigned i=0; i<n; i++){
            if(working.find(i)==working.end()){
                f(c[i]);
            }
        }
    }
}



#endif