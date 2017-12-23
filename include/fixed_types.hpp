#ifndef fixed_types_hpp
#define fixed_types_hpp

#include <cstdint>

/*  Fix16 is:
    - signed
    - has 16 fractional bits
    - has at least 12 integer bits (i.e. it can do at least [-2^12+1..2^12-1]

    This implementation of Fix16 is a twos-complement number with
    - 1 sign bit
    - 14 integer bits
    - 16 fractional bits
    - 1 exception bit
    
    siiiiiii iiiiiiif ffffffff fffffffe
    
    If e is zero, then the number is valid.
      0x80000002 = most negative number
      0x7FFFFFFE = most positive number
    
    If e is 1, then we have:
      0x80000001 = -inf
      0x7fffffff = +inf
      0x00000001 = nan
      
    Overall space is:
      0x80000000 = (invalid)
      0x80000001 = negative infinity
      0x80000002 = most negative normal
      ...
      0xFFFFFFFC = -2
      0xFFFFFFFD = (invalid)
      0xFFFFFFFE = -1
      0xFFFFFFFF = (invalid)
      0x00000000 = zero
      0x00000001 = nan
      0x00000010 = 1
      0x00000011 = (invalid)
      0x00000100 = 2
      ...
      0x7FFFFFFD = (invalid)
      0x7FFFFFFE = most positive
      0x7FFFFFFF = positive inv
      
    Any overflow results in inf.
    Addition of infs with different signs results in nan
*/

const int32_t FIX16_NEG_INF_BITS = -0x7FFFFFFFl; // == 0x80000001ul
const int32_t FIX16_POS_INF_BITS = +0x7FFFFFFEl;

const fix16_t FIX16_NEG_INF = { FIX16_NEG_INF_BITS };
const fix16_t FIX16_POS_INF = { FIX16_POS_INF_BITS };
const fix16_t FIX16_NAN = { 1 };


const int32_t FIX16_MAX_RAW = (1l<<30)-1;
const int32_t FIX16_MIN_RAW = -FIX16_MAX_RAW;

const int32_t FIX16_MAX_BITS = FIX16_MAX_RAW<<1;
const int32_t FIX16_MIN_BITS = FIX16_MIN_RAW<<1;

const fix16_t FIX16_MAX_VAL = { FIX16_MAX_BITS };
const fix16_t FIX16_MIN_VAL = { FIX16_MIN_BITS };

const int32_t FIX16_MAX_INT = 0x3FFF;
const int32_t FIX16_MIN_INT = -0x3FFF;

struct fix16_t{
    int32_t v;
};

inline fix16_is_valid(fix16_t x)
{
    if(x.v&0x1){
        return x.v==1 || x.v==FIX16_POS_INF_BITS || x.v==FIX16_NEG_INF_BITS;
    }else{
        return x.v<=FIX6_MAX_BITS && x.v>=FIX16_MIN_BITS;
    }
}

inline fixed16_t fix16_create_from_int(int32_t x)
{
    if(x<FIX16_MIN_INT){
        return FIX16_NEG_INF;
    }else if(x>FIX16_MAX_INT){
        return FIX16_POS_INF;
    }else{
        return fix16_t{ x<<17 };
    }
}

inline bool fix16_less_than(fix16_t a, fix16_t b)
{
    assert(fix16_is_valid(va));
    assert(fix16_is_valid(vb));

    return (a.v < b.v) && (a.v!=1) && (b.v!=1);
}

inline bool fix16_less_than_equals(fix16_t a, fix16_t b)
{
    assert(fix16_is_valid(va));
    assert(fix16_is_valid(vb));
   
    if(a.v==1 || b.v==1)
        return false;
    return (a.v <= b.v) && (a.v!=1) && (b.v!=1);
}

inline bool fix16_equals(fix16_t a, fix16_t b)
{
    assert(fix16_is_valid(va));
    assert(fix16_is_valid(vb));
   
    if(a.v==1 || b.v==1)
        return false;
    return (a.v == b.v) && (a.v!=1) && (b.v!=1);
}

// Add with saturation
inline q16p16_t fix16_add(q16p16_t va, q16p16_t vb)
{
    assert(fix16_is_valid(va));
    assert(fix16_is_valid(vb));
    
    fix16_t res;
    
    if((a.v|v.b)&1){
        // unlikely
        if(a.v==1){
            res=a; // a=nan, b=?, r=nan
        }else if(a.b==1){
            res=b; // a=?, b=nan, r=nan
        }else if( (a>0)==(b>0) ){
            res.v = a.v | b.v ; // One of them is inf, and they are both the same sign
        }else if( (a.v&b.v)&1 ){ // The are both inf, with different sign
            res=FIX16_NAN;
        }else{  // One is inf, the other is normal, so inf dominates
            res=(a.v&1) ? a : b;
        }
    }else{
        int32_t tmp=(a.v>>1)+(b.v>>1);
        if(tmp<FIX16_MIN_RAW){
            res=FIX16_NEG_INF;
        }else if(tmp>FIX16_MAX_RAW){
            res=FIX16_POS_INF;
        }else{
            res=fix16_t{ tmp<<1 };
        }
    }
    
    assert(fix16_is_valid(res));
    
    return res;
}
    
/*! This provides convergent rounding, with
    saturation on overflow. */
inline q16p16_t fixed_mul(q16p16_t a, q16p16_t b)
{
    assert(fix16_is_valid(va));
    assert(fix16_is_valid(vb));
    
    fix16_t res;
    
    if( (a.v|b.v)&1 ){
        if(a.v==1){
            res=a;   // a is nan
        }else if(b.v==1){
            res=b;   // b is nan
        }else{
            // one of them is infinity, so no matter what the
            // answer is it will also be infinity.
            res=((a.v^b.v)<0) ? FIX16_NEG_INF : FIX16_POS_INF;
        }
    }else{
        int64_t r=int64_t(a) * int64_t(b);
        if( (r&0xFFFF) == 0x8000 ){ // Check if fraction is exactly 0.5 ...
            /*      
            sxxy.10 -> sxxy + y
            
            1xx0.10 -> 1xx0 + 0
            1xx1.10 -> 1xx1 + 1
            0xx0.10 -> 0xx0 + 0
            0xx1.10 -> 0xx1 + 1
    
            1100.10 (-3.5) -> 1100 (-4)
            1101.10 (-2.5) -> 1110 (-2)
            1110.10 (-1.5) -> 1110 (-2)
            1111.10 (-0.5) -> 0000 ( 0)
            0000.10 (+0.5) -> 0000 ( 0)
            0001.10 (+1.5) -> 0010 (+2)
            0010.10 (+2.5) -> 0010 (+2)
            0011.10 (+3.5) -> 0100 (+4)
            */
            r = r+(r&0x10000);
        }else{
            r = r+0x8000;
        }
        
        r=r>>16;
        
        if( r < FIX16_MIN_RAW {
            res=FIX16_NEG_INF;
        }else if( r > FIX16_MAX_RAW ){
            res=FIX16_POS_INF;
        }else{
            res.v = (int32_t)(r<<1);
        }
        
        assert(fix16_is_valid(res));
        
        return res;
    }
}

#endif
