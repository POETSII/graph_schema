
#include <cstdint>
#include <cmath>
#include <cstdio>

float fix16_exp_v0(int32_t x)
{
    // Must balance table size and ops. This implementation
    // goes heavy on ops, and optimises for values closer to 0

    static const float C[11]={
        2.71828182845905f, // exp(1)
        0.367879441171442f,  // exp(-1)
        // P(x)
        -0.0001531624443904903f,
        -0.001604790561658129f,
        -0.007433195916076948f,
        -0.01413606583476082f,
        // Q(x)
        9.12546890524298e-05f,
        -0.001239663901569256f,
        0.006702872352956657f,
        -0.01413606586298593f,
        // 1
        1.0f
    };

    const int frac_bits=16;

    if(x<(-127<<frac_bits)){
        return 0.0f;
    }else if(x>(127<<frac_bits)){
        return INFINITY;
    }

    unsigned xlo=x&0xFFFF;
    int xhi=x>>16;

    /* This nonsense is because otherwise both clang and gcc
        will generate the address for each constant using lui+addi+flw,
        rather than just keeping one pointer in a register
        and using relative offsets for one flw.
        Doing it this way saves ~16 instructions.
    */
    const float volatile * P=C;

    float ep=P[0]; // exp(1)
    if(xhi<0){
        ep=P[1];  // exp(-1)
        xhi=-xhi;
    }

    float iy=P[10]; // 1
    while(xhi){
        if(xhi&1){
            iy *= ep;
        }
        ep = ep*ep;
        xhi=xhi>>1;
    }

    //float fx_f=ldexpf((float)xlo,-16);
    float fx_f=((float)xlo) * (1.0f/65536.0f);
    float fy = (((P[2] * fx_f + P[3]) * fx_f + P[4]) * fx_f + P[5])
        /
        (((P[6] * fx_f + P[7]) * fx_f + P[8]) * fx_f + P[9]);

    return iy * fy;
}

static const float H[16]={
        expf(0), expf(16), expf(32), expf(48), expf(64), expf(80), expf(96), expf(112),
        expf(-128), expf(-112), expf(-96), expf(-80), expf(-64), expf(-48), expf(-32), expf(-16)
    };
    static const float M[16]={
        expf(0), expf(1), expf(2), expf(3), expf(4), expf(5), expf(6), expf(7),
        expf(8), expf(9), expf(10), expf(11), expf(12), expf(13), expf(14), expf(15)
    };

float fix16_exp_v1(int32_t x)
{
    // x = HHHHMMMM.LLLLLLLLLLLLLLLL

    

    if(x<(-64<<16)){
        return 0.0f;
    }else if(x>=(64<<16)){
        return INFINITY;
    }

    unsigned xlo=x&0xFFFF;
    unsigned xmid=(x>>16)&0xF;
    unsigned xhi=(x>>20)&0xF;

    static const float C[8]={
        // P(x)
        -0.0001531624443904903f,
        -0.001604790561658129f,
        -0.007433195916076948f,
        -0.01413606583476082f,
        // Q(x)
        9.12546890524298e-05f,
        -0.001239663901569256f,
        0.006702872352956657f,
        -0.01413606586298593f
    };
    const float volatile * P=C;

    float fx_f=((float)xlo) * (1.0f/65536.0f);
    float fy = (((P[0] * fx_f + P[1]) * fx_f + P[2]) * fx_f + P[3])
        /
        (((P[4] * fx_f + P[5]) * fx_f + P[6]) * fx_f + P[7]);

    float gy=H[xhi] * M[xmid] * fy;

    //fprintf(stderr, "x=%d, xhi=%u, xmi=%u, xlo=%u, %g, %g, %g, y=%g, r=%g\n", x, xhi, xmid, xlo, H[xhi], M[xmid], fy, gy, expf((float)(x/65536.0f)));

    return gy;
}
