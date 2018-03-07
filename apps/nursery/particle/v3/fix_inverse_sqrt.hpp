#ifndef fix_inverse_sqrt_hpp
#define fix_inverse_sqrt_hpp

template<class T>
T fix_inverse_sqrt_s15p16(T v)
{
    static_assert(T::frac_bits==16, "Only works for 16 fractional bits.");
    
    const T zero = T::from_double(0.0);
    const T three = T::from_double(3.0);
    const T half = T::from_double(0.5);
    const T two_eps = T::from_double(1.0/32768);
    
    // Can't handle zero or negatives
    assert( v > zero );
    // So we round it up to 2^-16 in hardware...
    v=std::max(v,T::eps());
    
    int32_t e=v.log2ceil(); // 2^-16 -> -16, (2^15-1) -> 15
    
    assert(-16 <= e && e <= 15);
    
    /* for i: -16 thru 15 do (
    print("T::from_double(",float(1/sqrt(2^i)),"), // i=",i)
    );*/
    const static T starter[32]={
        T::from_double( 256.0 ), // i= -16 
        T::from_double( 181.0193359837563 ), // i= -15 
        T::from_double( 128.0 ), // i= -14 
        T::from_double( 90.50966799187816 ), // i= -13 
        T::from_double( 64.0 ), // i= -12 
        T::from_double( 45.25483399593907 ), // i= -11 
        T::from_double( 32.0 ), // i= -10 
        T::from_double( 22.62741699796953 ), // i= -9 
        T::from_double( 16.0 ), // i= -8 
        T::from_double( 11.31370849898477 ), // i= -7 
        T::from_double( 8.0 ), // i= -6 
        T::from_double( 5.656854249492382 ), // i= -5 
        T::from_double( 4.0 ), // i= -4 
        T::from_double( 2.828427124746191 ), // i= -3 
        T::from_double( 2.0 ), // i= -2 
        T::from_double( 1.414213562373095 ), // i= -1 
        T::from_double( 1.0 ), // i= 0 
        T::from_double( 0.7071067811865475 ), // i= 1 
        T::from_double( 0.5 ), // i= 2 
        T::from_double( 0.3535533905932737 ), // i= 3 
        T::from_double( 0.25 ), // i= 4 
        T::from_double( 0.1767766952966368 ), // i= 5 
        T::from_double( 0.125 ), // i= 6 
        T::from_double( 0.0883883476483184 ), // i= 7 
        T::from_double( 0.0625 ), // i= 8 
        T::from_double( 0.0441941738241592 ), // i= 9 
        T::from_double( 0.03125 ), // i= 10 
        T::from_double( 0.02209708691207959 ), // i= 11 
        T::from_double( 0.015625 ), // i= 12 
        T::from_double( 0.0110485434560398 ), // i= 13 
        T::from_double( 0.0078125 ), // i= 14 
        T::from_double( 0.005524271728019897 ) // i= 15 
    
};
    
    T xi=starter[e+16];
    
    // Do iterations of newton raphson
    for(unsigned i=0; i<5; i++){
        xi=xi*(three - v * xi * xi )*half;
    }
        
    return xi;
}

#endif
