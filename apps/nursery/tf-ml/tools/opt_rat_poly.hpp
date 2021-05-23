#ifndef opt_rat_poly_hpp
#define opt_rat_poly_hpp

#include <vector>
#include <functional>
#include <cassert>
#include <random>

#include <armadillo>

#include <boost/math/special_functions/next.hpp>

#include <immintrin.h>

#include <rapidjson/document.h>

int distance(float a, float b)
{

    union{
        int32_t u;
        float f;
    } aa, bb;

    bool nega=false;
    if(a<0){
        aa.f=-a;
        nega=true;
    }else{
        aa.f=a;
    }
    bool negb=false;
    if(b<0){
        negb=true;
        bb.f=-b;
    }else{
        bb.f=b;
    }

    if(nega==negb){
        return aa.u-bb.u;
    }else{
        return aa.u+bb.u;
    }
}

struct OptPoly
{
    std::string expression;
    double range_low, range_high, range_step;

    unsigned n, m;
    bool use_fma=false;
/*
    rapidjson::Value &save(rapidjson::Document::AllocatorType &alloc, const std::vector<float> &c)
    {
        rapidjson::Value res();

        res.AddString("type", "OptPoly", alloc);
        res.Add 
    }
*/

    std::vector<float> x;
    std::vector<double> y;

    template<class T, bool FMA=true>
    double eval(
        const T *coeffs,
        int metric_order=0
    ) const
    {
        std::vector<float> pc(coeffs,coeffs+n+1);
        std::vector<float> qc(coeffs+n+1,coeffs+n+m+2);

        double worst=0;

        for(unsigned i=0; i<x.size(); i++){
            float vx=x[i];
            double vy=y[i];

            float p=pc[n];
            for(int j=n-1; j>=0; j--){
                if(!FMA){
                    float tmp=p * vx;
                    p = tmp + pc[j];
                }else{
                    p=fmaf(p , vx , pc[j]);
                }
            }
            float q=qc[m];
            for(int j=m-1; j>=0; j--){
                if(!FMA){
                    float tmp=q * vx;
                    q = tmp + qc[j];
                }else{
                    q=fmaf(q , vx , qc[j]);
                }
            }

            float gy=p/q;
            double err=std::abs((vy-gy)/vy);
            switch(metric_order){
                case 0:
                    worst=std::max(worst, err);
                    break;
                case 1:
                    worst += err;
                    break;
                case 2:
                    worst += err*err;
                    break;
                case 3:
                    worst += err*err*err;
                    break;
                case 4:
                    err = err *err;
                    worst += err*err;
                    break;
                default:
                    assert(false);
            }
        }

        if(metric_order > 0){
            worst = std::pow( worst / x.size(), 1.0/metric_order );
        }

        return worst;
    }

    double eval_avx2(
        const double *coeffs,
        unsigned metric_order=0
    ) const
    {
        std::vector<float> pc(coeffs,coeffs+n+1);
        std::vector<float> qc(coeffs+n+1,coeffs+n+m+2);

        if(n==1) return eval_avx2_n<1>(&pc[0], &qc[0], x.size(), &x[0], &y[0], metric_order);
        if(n==2) return eval_avx2_n<2>(&pc[0], &qc[0], x.size(), &x[0], &y[0], metric_order);
        if(n==3) return eval_avx2_n<3>(&pc[0], &qc[0], x.size(), &x[0], &y[0], metric_order);
        if(n==4) return eval_avx2_n<4>(&pc[0], &qc[0], x.size(), &x[0], &y[0], metric_order);
        if(n==5) return eval_avx2_n<5>(&pc[0], &qc[0], x.size(), &x[0], &y[0], metric_order);
        if(n==6) return eval_avx2_n<6>(&pc[0], &qc[0], x.size(), &x[0], &y[0], metric_order);
        if(n==7) return eval_avx2_n<7>(&pc[0], &qc[0], x.size(), &x[0], &y[0], metric_order);
        throw std::runtime_error("Unsupported order.");
    }

    template<unsigned N>
    double eval_avx2_n(
        const float *pc,
        const float *qc,
        unsigned nx,
        const float *x,
        const double *y, unsigned metric_order
    ) const
    {
        if(m==1) return eval_avx2_n_m<N,1>(&pc[0], &qc[0], nx, &x[0], &y[0], metric_order);
        if(m==2) return eval_avx2_n_m<N,2>(&pc[0], &qc[0], nx, &x[0], &y[0], metric_order);
        if(m==3) return eval_avx2_n_m<N,3>(&pc[0], &qc[0], nx, &x[0], &y[0], metric_order);
        if(m==4) return eval_avx2_n_m<N,4>(&pc[0], &qc[0], nx, &x[0], &y[0], metric_order);
        if(m==5) return eval_avx2_n_m<N,5>(&pc[0], &qc[0], nx, &x[0], &y[0], metric_order);
        if(m==6) return eval_avx2_n_m<N,6>(&pc[0], &qc[0], nx, &x[0], &y[0], metric_order);
        if(m==7) return eval_avx2_n_m<N,7>(&pc[0], &qc[0], nx, &x[0], &y[0], metric_order);
        throw std::runtime_error("Unsupported order.");
    }

    template<unsigned N, unsigned M>
    double eval_avx2_n_m(
        const float *pc,
        const float *qc,
        unsigned nx,
        const float *x,
        const double *y, unsigned metric_order
    ) const
    {
        if(metric_order==0) return eval_avx2_n_m_mo<N,M,0>(&pc[0], &qc[0], nx, &x[0], &y[0]);
        if(metric_order==1) return eval_avx2_n_m_mo<N,M,1>(&pc[0], &qc[0], nx, &x[0], &y[0]);
        if(metric_order==2) return eval_avx2_n_m_mo<N,M,2>(&pc[0], &qc[0], nx, &x[0], &y[0]);
        if(metric_order==3) return eval_avx2_n_m_mo<N,M,3>(&pc[0], &qc[0], nx, &x[0], &y[0]);
        if(metric_order==4) return eval_avx2_n_m_mo<N,M,4>(&pc[0], &qc[0], nx, &x[0], &y[0]);
        throw std::runtime_error("Invalid order.");
    }

    template<unsigned N,unsigned M,unsigned metric_order>
    __attribute__((noinline)) double eval_avx2_n_m_mo(
        const float *pc,
        const float *qc,
        unsigned nx,
        const float *x,
        const double *y
    ) const
    {
        if(use_fma) return eval_avx2_n_m_mo_fma<N,M,0,true>(&pc[0], &qc[0], nx, &x[0], &y[0]);
        else return eval_avx2_n_m_mo_fma<N,M,0,false>(&pc[0], &qc[0], nx, &x[0], &y[0]);
    }

    template<unsigned N,unsigned M,unsigned metric_order, bool use_fma>
    __attribute__((noinline)) double eval_avx2_n_m_mo_fma(
        const float *pc,
        const float *qc,
        unsigned nx,
        const float *x,
        const double *y
    ) const
    {
        const int n=N;
        const int m=M;
        typedef float vecF __attribute__ ((vector_size (32))) ;
        using vecD = double __attribute__ ((vector_size (64)));

        auto fmav=[](vecF a, vecF b, vecF c) -> vecF
        {
            union {
                __m256 mm;
                vecF v;
            } aa, bb, cc, dd;

            aa.v=a;
            bb.v=b;
            cc.v=c;

            dd.mm=_mm256_fmadd_ps (aa.mm, bb.mm, cc.mm);

            return dd.v;
        };

        vecD worst=0.0 - vecD{};

        assert((nx%8)==0);

        for(unsigned i=0; i<nx; i+=8){
            vecF vx;
            vecD vy;
            memcpy(&vx, x+i, sizeof(vecF));
            memcpy(&vy, y+i, sizeof(vecD));

            vecF p=pc[n]-vecF{};
            for(int j=n-1; j>=0; j--){
                if(use_fma){
                    p=fmav(p, vx, pc[j]-vecF{});
                }else{
                    vecF tmp=p * vx;
                    p = tmp + pc[j];
                }
            }
            vecF q=qc[m]-vecF{};
            for(int j=m-1; j>=0; j--){
                if(use_fma){
                    q=fmav(q, vx, qc[j]-vecF{});
                }else{
                    vecF tmp=q * vx;
                    q = tmp + qc[j];
                }
            }

            vecF gy=p/q;
            vecD dgy={gy[0],gy[1],gy[2],gy[3],gy[4],gy[5],gy[6],gy[7]};
            vecD err=(vy-dgy)/vy;
            if(metric_order==0){
                err = err < 0 ? -err : err; 
                worst = err > worst ? err : worst;
            }else if(metric_order==1){
                err = err < 0 ? -err : err; 
                worst += err;
            }else if(metric_order==2){
                err=err*err;
                worst += err;
            }else if(metric_order==3){
                err = err < 0 ? -err : err; 
                worst += err*err*err;
            }else if(metric_order==4){
                err=err*err;
                err=err*err;
                worst += err;
            }else{
                assert(0<=metric_order && metric_order<=4);
            }
        }

        double worstS=0;
        for(int i=0; i<8; i++){
            if(metric_order>0){
                worstS += worst[i];
            }else{
                worstS=std::max(worstS, worst[i]);
            }
        }

        if(metric_order>0){
            worstS = std::pow(worstS/nx,1.0/metric_order);
        }

        return worstS;
    }

    std::function<double(const arma::vec &, arma::vec*, void*)> get_eval_arma() const
    {
        return [this](const arma::vec &x, arma::vec* grad_out, void* opt_data) -> double
        { 
            unsigned l=x.n_elem;
            assert(l==n+m+2);

            //return eval(x.colptr(0));
            return eval_avx2(x.colptr(0));
        };
    }

    std::vector<float> perturb(std::mt19937_64 &rng, const std::vector<float> &x) const
    {
        std::normal_distribution<> g;

        std::vector<float> n(x);

        while(x==n){
            for(int i=0; i<x.size(); i++){
                int p=(int)(2*g(rng));
                n[i]=boost::math::float_advance(x[i], p);
            }
        }

        return n;
    }
};

#endif

