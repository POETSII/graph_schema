#include "nn_ops.hpp"
#include <cstdint>
#include <cstdio>
#include <cmath>

#include <boost/math/special_functions/next.hpp>

float exp_ref(int x)
{
    if(x<(-64<<16)){
        return 0;
    }
    if(x>=(64<<16)){
        return INFINITY;
    }
    double fx=x/65536.0;
    return (float)exp(fx);
}

int main()
{
    int32_t x=INT32_MIN;

    double lMax=0;
    double lSum=0;
    double lCount=0;

    double worst=0;
    do{
        float gy=fix16_exp_v1(x);

        float fx=((float)x)/65536.0f;
        float ry=exp_ref(x);

        double ulps=0;
        if(std::isinf(ry)){
            ulps=0;
        }else{
            ulps=std::abs(boost::math::float_distance(gy,ry));
        }

        if(ulps > worst){
            fprintf(stderr, "x=%f, err=%g, gy=%g, ry=%g\n", fx, ulps, gy, ry);
            worst=ulps;
        }

        if(ry!=0 && !std::isinf(ry)){

            lMax=std::max(ulps, lMax);
            lSum += ulps;
            lCount++;

            if((x&0xFFFF)==0){
                fprintf(stderr, "x=%f, ry=%g, maxErr=%g, avgErr=%g\n", fx, ry, lMax, lSum/lCount);
                lMax=0;
                lSum=0;
                lCount=0;
            }

        }

        x++;
    }while(x !=INT32_MAX);
}