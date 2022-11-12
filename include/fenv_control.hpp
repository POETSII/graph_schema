#ifndef fenv_control_hpp
#define fenv_control_hpp

#include <stdexcept>

#if defined(__x86_64__)

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

    CheckDenormalsDisabled();
}

#elif defined(__aarch64__)

inline int getStatusWord()
{
    int result;
    asm volatile("mrs %[result], FPCR" : [result] "=r" (result));
    return result;
}

inline void setStatusWord(int a)
{
    asm volatile("msr FPCR, %[src]" : : [src] "r" (a));
}



inline void CheckDenormalsDisabled()
{
    volatile float x=2e-38f; // Just above sub-normal
    volatile float zero=0;

    x=x*0.5f;
    if(x!=zero){
        throw std::runtime_error("Denormals appear enabled.");
    }
}

inline void DisableDenormals()
{
    // https://codereview.chromium.org/402803003/patch/40001/50001
    auto orig = getStatusWord();
    // Bit 24 is the flush-to-zero mode control bit. Setting it to 1 flushes denormals to 0.
    setStatusWord(orig | (1 << 24));
    
  CheckDenormalsDisabled();
}


#else

#error "Don't know what CPU architecture this is."

#endif


#endif
