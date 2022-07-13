#ifndef fenv_control_hpp
#define fenv_control_hpp

#include <stdexcept>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma STDC FENV_ACCESS ON
#pragma GCC diagnostic pop

#include <xmmintrin.h>	// SSE instructions
#include <pmmintrin.h>	// SSE instructions

#ifndef __SSE2_MATH__
#error "Denormal control probably won't work."
#endif

inline void CheckDenormalsDisabled()
{
    if( _MM_GET_FLUSH_ZERO_MODE() != _MM_FLUSH_ZERO_ON ){
        throw std::runtime_error("Denormals appear enabled.");
    }
    if( _MM_GET_DENORMALS_ZERO_MODE() != _MM_DENORMALS_ZERO_ON ){
        throw std::runtime_error("Denormals appear enabled.");
    }

    volatile float x=2e-38f; // Just above sub-normal
    volatile float zero=0;

    x=x*0.5f;
    if(x!=zero){
        throw std::runtime_error("Denormals appear enabled.");
    }
}

inline void DisableDenormals()
{
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON );
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON );
}


#endif
