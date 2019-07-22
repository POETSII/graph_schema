#ifndef ising_spin_shared_hpp
#define ising_spin_shared_hpp

#include <cstdint>

uint32_t mod(uint32_t x, uint32_t y)
{
    while(x>=y){
    x-=y;
    }
    return x;
}

    uint32_t calc_tag(uint32_t x, uint32_t y)
    {
        return mod(x,3) + mod(y,3)*3;
    }

uint32_t urng(uint32_t &state)
{
    state = state *1664525+1013904223;
    return state;
}

uint32_t rng_init(uint32_t x, uint32_t y)
{
    y=(x<<16)^y;
    for(unsigned i=0;i<10;i++){
        // Both of these are invertible and complement each other.
        // The xorshift spreads, and the LCG is non-linear and gets us out of zero
        y^=(y<<13); y^=(y>>17); y^=(y<<5);
        urng(y);
    }
    return y;
}

    // This produces a time which is approximately distributed
    // with a sort of exponential-ish distribution
    // The maximum value is 2^21-1
    // Expected value is:
    // 0.5 * 0x4000 + 0.25 * 0x8000 + 0.125 * 0x10000 + ...
    // = sum( 2^(-i-1) * 16384 * 2^i, i=0..5) + 2^-6 * 16384 * 2^6
    // = sum( 16384 * 0.5, i=0..5 ) + 16384
    // = 8192 * 6 + 16384
    // = 2^16
    //
    // res=  2.^min(6,floor(-log2(rand(1,n)))) .* (rand(1,n)*32768);
    uint32_t erng(uint32_t &state)
{
        uint32_t v=urng(state);
        uint32_t hi=v>>26; 		// Top 6 bits
        uint32_t lo=(v<<6)>>17;				// Following 15 bits. Bottom 11 bits are discarded

        uint32_t shift=6;
        if(hi&1){ shift=0; }
        else if(hi&2){ shift=1; }
        else if(hi&4){ shift=2; }
        else if(hi&8){ shift=3; }
        else if(hi&16){ shift=4; }
        else if(hi&32){ shift=5; }

        uint32_t val=lo<<shift;
        //fprintf(stderr, "2^%u*%u = %u\n", shift, lo, val);
        return val;
    }

    void cell_init(uint32_t x, uint32_t y, int32_t &spin, uint32_t &time, uint32_t &seed)
    {
        uint32_t tag = calc_tag(x,y);
        seed=rng_init( x, y );
        spin = (urng(seed)>>31) ? +1 : -1;
        time = (erng(seed) << 4) | tag ;
    }

void chooseNextEvent(const uint32_t *probabilities, uint32_t tag, uint32_t &rng, int32_t *spins, uint32_t &nextTime)
{
    int sumStates=0;
    for(unsigned i=1; i<5; i++){ // Only sum neighbours
        sumStates+=spins[i];
    }

    unsigned index=(sumStates+4)/2 + 5*(spins[0]+1)/2;
    uint32_t prob=probabilities[index];

    auto u=urng(rng);

    //fprintf(stderr, "   sumStates=%d, state=%d, prob=%u, urng=%u\n", sumStates, spins[0], prob, u);

    if( u < prob){
        spins[0] *= -1; // Flip
    }
    nextTime = (((nextTime>>4) + erng(rng)) << 4) | tag;
}

void get_neighbour_coords(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int dir, uint32_t &nx, uint32_t &ny)
{
    nx=x;
    ny=y;
    if(dir==1){ ny = y==0 ? h-1 : y-1; }
    if(dir==2){ nx = x==w-1 ? 0 : x+1; }
    if(dir==3){ ny = y==h-1 ? 0 : y+1; }
    if(dir==4){ nx = x==0 ? w-1 : x-1; }
}

#endif
