

uint32_t f(unsigned p, int16_t *x) // x in (-2,+2) with 13 fractional bits
{
    uint32_t acc=0;
    for(int i=0; i<p-1; i++){
        
        int32_t xx2=x[i+1]-x[i]; // (-4,+4) =3 bits, 13 fractional bits 
        uint32_t xx2u=xx2*xx2;   // [0,+16) = 4 bits, 26 fractional bits
        xx2u=(xx2u+((1<<25)-1))>>14; // [0,+16) = 4 bits, 12 fractional bits
        xx2u=xx2u*xx2u; // [0,+256) = 8 bits, 24 fractional bits
        xx2u=(xx2u+((1<<23)-1))>>8;  // [0,+256) = 8 bits, 16 fractional bits
        
        int32_t x2=x[i]-(1<<13); // (-3,+1) = 3 bits, 13 fractional bits
        uint32_t x2u=x2*x2;     // [0,6) = 3 bits, 26 fractional bits
        x2=(x2+((1<<25)-1))>>10;   // 16 fractional bits
        
        acc += 100*xx2 + x2;
    }
    return acc;
}

void crossover_coarse(
    uint32_t &seed,
    unsigned p,
    const int16_t *parentA,
    const int16_t *parentB,
    int16_t *offspring
){
    uint32_t bits=rng(seed);
    for(unsigned i=0; i<p; i++){
        offspring[i] = (bits&0x80000000ul) ? parentA[i] : parentB[i];
        bits=bits<<1;
    }
}

void crossover_blend(
    uint32_t &seed,
    unsigned p,
    const int16_t *parentA,
    const int16_t *parentB,
    int16_t *offspring
){
    for(unsigned i=0; i<p; i++){
        uint32_t bits=rng(seed);
        int32_t pA=bits>>16;
        int32_t pB=0xFFFF - pA;
        offspring[i] = parentA[i] * pA + parentB[i] * pB;
    }
}

void mutate_triangle(
    uint32_t &seed,
    unsigned p,
    unsigned lsbs,
    int16_t *individual
){
    for(unsigned i=0; i<p; i++){
        uint32_t bits=rng(seed);
        int32_t tri=int32_t(bits>>16)-int32_t(bits&0xFFFF); // triangle in (-65536,+65536)
        tri=tri>>(16-lsbs);
        individual[i] = std::min( (-2<<13)+1, std::max( (2<<13)-1 ,individual[i]+tri) );
    }
}


struct ga_cell
{
    unsigned p;
    uint32_t bestFitnessSeen;
    uint32_t currentFitness;
    std::vector<int16_t> currentIndividual;
    std::vector<int16_t> tempIndividual;
    
    void on_receive(uint32_t inFitness, const int16_t *inIndividual)
    {
        if(inFitness < bestFitnessSeen){
            bestFitnessSeen = inFitness;
        }
        
    }
};
