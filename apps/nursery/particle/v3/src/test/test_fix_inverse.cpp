#include "particle.hpp"

#include "fix_inverse.hpp"
#include "fix_inverse_sqrt.hpp"

#include <random>

void test_log2ceil()
{
    for(int e=-16; e<=+15; e++){
        real_t v=real_t::from_double(ldexp(1.0, e));
        auto lg2=v.log2ceil();
        //fprintf(stderr, "Expected = %d, got=%d\n", e, lg2);
        assert(lg2==e);
    }
    
    std::mt19937 rng;
    std::uniform_real_distribution<> urng;
    
    for(int i=0; i<10000; i++){
        double u=urng(rng)*real_t::max_pos().to_double();
        if(real_t::eps().to_double() <= u && u <= real_t::max_pos().to_double()){
            real_t v=real_t::from_double(u);
            
            auto lg2=v.log2ceil();
            auto ref=(int)ceil(log2(u));
            //fprintf(stderr, " %f : got=%d, exp=%d\n", u, lg2, ref);
            assert(lg2==ref);
        }
    }    
}

void test_inverse()
{
    for(int e=-15; e<=+14; e++){
        real_t v=real_t::from_double(ldexp(1.0, e));
        auto got=fix_inverse_s15p16(v);
        auto ref=real_t::from_double( 1.0/v.to_double() );
        fprintf(stderr, "v = %g, got = %g, ref = %g\n", v.to_double(), got.to_double(), ref.to_double());
        assert(got==ref);
    }
    
    std::mt19937 rng;
    std::uniform_real_distribution<> urng;
    
    
    for(int i=0; i<10000000; i++){
        double u=urng(rng)*real_t::max_pos().to_double();
        if(real_t::eps().to_double() <= u && u <= real_t::max_pos().to_double()){
            real_t v=real_t::from_double(u);
            
            auto got=fix_inverse_s15p16(v);
            auto ref=real_t::from_double(1.0/v.to_double());
            double fgot=got.to_double(), fref=ref.to_double();
            double absErr=fgot-fref;
            double relErr=(fgot-fref)/fref;
            if( std::abs(absErr) > ldexp(1.0,-15) && std::abs(relErr) > ldexp(1.0,-15) ){
                fprintf(stderr, " %g : got=%g, exp=%g,  absE=%g, relE=%g\n", v.to_double(), got.to_double(), ref.to_double(), absErr, relErr);
                fprintf(stderr, "fail on error.\n");
                exit(1);
            }
        }
    }    
}

void test_inverse_sqrt()
{
    std::mt19937 rng;
    std::uniform_real_distribution<> urng;
    
    
    for(int i=0; i<10000000; i++){
        double u=urng(rng)*real_t::max_pos().to_double();
        if(real_t::eps().to_double() <= u && u <= real_t::max_pos().to_double()){
            real_t v=real_t::from_double(u);
            
            auto got=fix_inverse_sqrt_s15p16(v);
            auto ref=real_t::from_double(1.0/sqrt(v.to_double()));
            double fgot=got.to_double(), fref=ref.to_double();
            double absErr=fgot-fref;
            double relErr=(fgot-fref)/fref;
            if( std::abs(absErr) > ldexp(1.0,-14) && std::abs(relErr) > ldexp(1.0,-14) ){
                fprintf(stderr, " %g : got=%g, exp=%g,  absE=%g, relE=%g\n", v.to_double(), got.to_double(), ref.to_double(), absErr, relErr);
                fprintf(stderr, "fail on error.\n");
                exit(1);
            }
        }
    }    
}

int main()
{
    test_log2ceil();
    test_inverse_sqrt();
    test_inverse();
}
