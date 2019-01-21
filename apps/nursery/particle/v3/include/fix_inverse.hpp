#ifndef fix_inverse_hpp
#define fix_inverse_hpp

#include <cassert>

// This only works for fixed-point with 16 bits,
// though could be adapted
template<class T>
T fix_inverse_s15p16(T x)
{
    static_assert(T::frac_bits==16, "Only works for 16 fractional bits.");
    
    const T zero = T::from_double(0.0);
    const T two = T::from_double(2.0);
    const T two_eps = T::from_double(1.0/32768);
    
    T v=x.abs();
    
    // We have a problem, in that 2^-16 does not have an inverse.
    assert( v > T::eps() );
    // We also can't handle zero
    assert( v != zero );
    // So we round it up to 2^-15 in hardware...
    // Really we need a NaN
    v=std::max(v,two_eps);
    
    int32_t e=v.log2ceil(); // 2^-16 -> -16, (2^15-1) -> 15
    
    // Take advantage of the fact that 1<<e is 2^e, and
    // we want 2^-e.
    // We can take advantage of symmetry around 1.
    // 1/1.0 = 1 / 2^16 = 2^16 = 1.0
    // 1/2.0 = 1 / 2^17 = 2^15 = 0.5
    // 1/4.0 = 1 / 2^18 = 2^14 = 0.25
    //         1 / 2^e  = 2^(32-e)
    assert(-15 <= e && e <= +15);
    
    // This is now our starter
    T xi=T::from_raw( 1<<(16-e) );
    
    // Do iterations of newton raphson
    for(unsigned i=0; i<4; i++){
        xi=xi*(two-v*xi);
    }
    
    if(x < zero){
        xi=-xi;
    }
    
    return xi;
}

#endif
