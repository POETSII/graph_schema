#ifndef shared_utils_hpp
#define shared_utils_hpp

#include <cstdint>
#include <cmath>
#include <array>

#if __cplusplus >= 201703L 
#include <string_view>
#endif

uint32_t MWC64X(uint64_t *state)
{
    /* Compiles to 6 instructions in RISCV (ignoring loads/stores, so state is in regs)
    mulhu a7, a4, a6
    mul a3, a4, a6
    xor t0, a5, a4
    add a4, a3, a5
    sltu a3, a4, a3
    add a5, a7, a3
    */
    uint32_t c=(*state)>>32, x=(*state)&0xFFFFFFFF;
    *state = x*((uint64_t)4294883355U) + c;
    return x^c;
}


float MWC64Gaussian_Tab64_CLT4(uint64_t *state)
{
    // Table has 14 fractional bits
    const int16_t tab1[64] = {
        -21993, // -1.34234619140625
        -15619, // -0.95330810546875
        -13551, // -0.82708740234375
        -12344, // -0.75341796875
        -11469, // -0.70001220703125
        -10761, // -0.65679931640625
        -10149, // -0.61944580078125
        -9598, // -0.5858154296875
        -9089, // -0.55474853515625
        -8610, // -0.5255126953125
        -8153, // -0.49761962890625
        -7714, // -0.4708251953125
        -7288, // -0.44482421875
        -6874, // -0.4195556640625
        -6469, // -0.39483642578125
        -6071, // -0.37054443359375
        -5679, // -0.34661865234375
        -5294, // -0.3231201171875
        -4912, // -0.2998046875
        -4535, // -0.27679443359375
        -4161, // -0.25396728515625
        -3791, // -0.23138427734375
        -3423, // -0.20892333984375
        -3057, // -0.18658447265625
        -2693, // -0.16436767578125
        -2331, // -0.14227294921875
        -1970, // -0.1202392578125
        -1610, // -0.0982666015625
        -1251, // -0.07635498046875
        -893, // -0.05450439453125
        -536, // -0.03271484375
        -179, // -0.01092529296875
        179, // 0.01092529296875
        536, // 0.03271484375
        893, // 0.05450439453125
        1251, // 0.07635498046875
        1610, // 0.0982666015625
        1970, // 0.1202392578125
        2331, // 0.14227294921875
        2693, // 0.16436767578125
        3057, // 0.18658447265625
        3423, // 0.20892333984375
        3791, // 0.23138427734375
        4161, // 0.25396728515625
        4535, // 0.27679443359375
        4912, // 0.2998046875
        5294, // 0.3231201171875
        5679, // 0.34661865234375
        6071, // 0.37054443359375
        6469, // 0.39483642578125
        6874, // 0.4195556640625
        7288, // 0.44482421875
        7714, // 0.4708251953125
        8153, // 0.49761962890625
        8610, // 0.5255126953125
        9089, // 0.55474853515625
        9598, // 0.5858154296875
        10149, // 0.61944580078125
        10761, // 0.65679931640625
        11469, // 0.70001220703125
        12344, // 0.75341796875
        13551, // 0.82708740234375
        15619, // 0.95330810546875
        21993, // 1.34234619140625
    };
    /*
    tab mean=0.0
    tab std=0.4999973523593257
    tab max(scaled) = 21993.0
    (4096,)
    (16777216,)
    conv4 std=0.9999947047186514
    conv4 kurt=-1.433106055115374e-05
    */

    // Total function is about 35 instructions when compiled

    /*
    GaussianF16(unsigned long long*): # @GaussianF16(unsigned long long*)
    lw a1, 0(a0)
    lw a2, 4(a0)
    lui a3, 1048556
    addi a3, a3, -2021
    mulhu a4, a1, a3
    mul a3, a1, a3
    add a5, a3, a2
    sltu a3, a5, a3
    add a6, a4, a3
    xor a1, a1, a2
    andi a2, a1, 63
    slli a2, a2, 1
    lui a4, %hi(.L__const.GaussianF16(unsigned long long*).tab1)
    addi a4, a4, %lo(.L__const.GaussianF16(unsigned long long*).tab1)
    add a7, a2, a4
    srli a3, a1, 7
    andi a3, a3, 126
    add a3, a3, a4
    srli a2, a1, 15
    andi a2, a2, 126
    add a2, a2, a4
    srli a1, a1, 23
    andi a1, a1, 126
    add a1, a1, a4
    lh a4, 0(a7)
    lh a3, 0(a3)
    lh a2, 0(a2)
    lh a1, 0(a1)
    sw a5, 0(a0)
    add a3, a3, a4
    add a2, a2, a3
    add a1, a1, a2
    slli a1, a1, 2
    sw a6, 4(a0)
    add a0, zero, a1
    ret
  */

    uint32_t u=MWC64X(state);

    int32_t acc=0;
    for(int i=0; i<4; i++){
        acc+=tab1[u&0x3F]; u=u>>6; // 5 instructions per iteration
    }

    const float scale=ldexp(1,-14);
    return acc*scale;
}

/* Generates four independent gaussians at once.
*/
std::array<float,4> MWC64Gaussian_Tab64_Hadamard4(uint64_t *state)
{
    // Table has 14 fractional bits
    const int16_t tab1[64] = {
        -21993, // -1.34234619140625
        -15619, // -0.95330810546875
        -13551, // -0.82708740234375
        -12344, // -0.75341796875
        -11469, // -0.70001220703125
        -10761, // -0.65679931640625
        -10149, // -0.61944580078125
        -9598, // -0.5858154296875
        -9089, // -0.55474853515625
        -8610, // -0.5255126953125
        -8153, // -0.49761962890625
        -7714, // -0.4708251953125
        -7288, // -0.44482421875
        -6874, // -0.4195556640625
        -6469, // -0.39483642578125
        -6071, // -0.37054443359375
        -5679, // -0.34661865234375
        -5294, // -0.3231201171875
        -4912, // -0.2998046875
        -4535, // -0.27679443359375
        -4161, // -0.25396728515625
        -3791, // -0.23138427734375
        -3423, // -0.20892333984375
        -3057, // -0.18658447265625
        -2693, // -0.16436767578125
        -2331, // -0.14227294921875
        -1970, // -0.1202392578125
        -1610, // -0.0982666015625
        -1251, // -0.07635498046875
        -893, // -0.05450439453125
        -536, // -0.03271484375
        -179, // -0.01092529296875
        179, // 0.01092529296875
        536, // 0.03271484375
        893, // 0.05450439453125
        1251, // 0.07635498046875
        1610, // 0.0982666015625
        1970, // 0.1202392578125
        2331, // 0.14227294921875
        2693, // 0.16436767578125
        3057, // 0.18658447265625
        3423, // 0.20892333984375
        3791, // 0.23138427734375
        4161, // 0.25396728515625
        4535, // 0.27679443359375
        4912, // 0.2998046875
        5294, // 0.3231201171875
        5679, // 0.34661865234375
        6071, // 0.37054443359375
        6469, // 0.39483642578125
        6874, // 0.4195556640625
        7288, // 0.44482421875
        7714, // 0.4708251953125
        8153, // 0.49761962890625
        8610, // 0.5255126953125
        9089, // 0.55474853515625
        9598, // 0.5858154296875
        10149, // 0.61944580078125
        10761, // 0.65679931640625
        11469, // 0.70001220703125
        12344, // 0.75341796875
        13551, // 0.82708740234375
        15619, // 0.95330810546875
        21993, // 1.34234619140625
    };
    /*
    tab mean=0.0
    tab std=0.4999973523593257
    tab max(scaled) = 21993.0
    (4096,)
    (16777216,)
    conv4 std=0.9999947047186514
    conv4 kurt=-1.433106055115374e-05
    */

    // Total function is about 35 instructions when compiled

    /*
    GaussianF16(unsigned long long*): # @GaussianF16(unsigned long long*)
    lw a1, 0(a0)
    lw a2, 4(a0)
    lui a3, 1048556
    addi a3, a3, -2021
    mulhu a4, a1, a3
    mul a3, a1, a3
    add a5, a3, a2
    sltu a3, a5, a3
    add a6, a4, a3
    xor a1, a1, a2
    andi a2, a1, 63
    slli a2, a2, 1
    lui a4, %hi(.L__const.GaussianF16(unsigned long long*).tab1)
    addi a4, a4, %lo(.L__const.GaussianF16(unsigned long long*).tab1)
    add a7, a2, a4
    srli a3, a1, 7
    andi a3, a3, 126
    add a3, a3, a4
    srli a2, a1, 15
    andi a2, a2, 126
    add a2, a2, a4
    srli a1, a1, 23
    andi a1, a1, 126
    add a1, a1, a4
    lh a4, 0(a7)
    lh a3, 0(a3)
    lh a2, 0(a2)
    lh a1, 0(a1)
    sw a5, 0(a0)
    add a3, a3, a4
    add a2, a2, a3
    add a1, a1, a2
    slli a1, a1, 2
    sw a6, 4(a0)
    add a0, zero, a1
    ret
  */

    uint32_t u=MWC64X(state);

    int32_t a1=tab1[u&0x3F]; u=u>>6;
    int32_t a2=tab1[u&0x3F]; u=u>>6;
    int32_t a3=tab1[u&0x3F]; u=u>>6;
    int32_t a4=tab1[u&0x3F]; u=u>>6;

    int32_t b1=a1+a2;
    int32_t b2=a1-a2;
    int32_t b3=a3+a4;
    int32_t b4=a3-a4;

    int32_t c1=b1+b3;
    int32_t c2=b1-b3;
    int32_t c3=b2+b4;
    int32_t c4=b2-b4;

    const float scale=ldexp(1,-14);
    return {c1*scale, c2*scale, c3*scale, c4*scale};
}

#if __cplusplus >= 201703L 

/* FNV is not the best hash in the world, but it 
is simple and very portable, and the 128-bit version
is reasonably well distributed. At best we're going
up to ~100,000,000 neurons, so the birthday prob
of clashes is very low, and not disastrous if it
does happen anyway.
*/
uint64_t id_to_seed(std::string_view id, uint64_t seed)
{
    using uint128 = unsigned __int128;

    static const uint128 basis=(uint128(0x6c62272e07bb0142ull)<<64)+0x62b821756295c58dull;
    static const unsigned __int128 prime=(uint128(0x0000000001000000ull)<<64)+0x000000000000013Bull;

    const char *begin=id.data();
    const char *end=begin+id.size();

    uint128 h=basis;
    h=(h^seed)*prime;
    while(begin!=end){
        size_t todo=end-begin;
        assert( (char)(todo & 0xFFu) == (char)*begin); // Should be little endian for portability
        uint64_t tmp=0;
        memcpy(&tmp, begin, todo); // avoid undefined behaviour
        h=(h^tmp)*prime;
        begin+=todo;
    }
    return uint64_t(h>>64) ^ uint64_t(h);
}

#endif

#endif