#define OPTIM_ENABLE_ARMA_WRAPPERS
#include "optim/optim.hpp"

#include "opt_rat_poly.hpp"

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <iomanip>

template<class T, class TH=std::hash<T>>
struct updating_queue
{
    updating_queue(TH h=TH())
        : current(1024, h)
    {}

    std::set<std::pair<double,T>> sorted;
    std::unordered_map<T,double,TH> current;

    void update(double w, const std::vector<float> &x)
    {
        auto it_b=current.insert(std::make_pair(x,w));
        if(it_b.second){
            sorted.insert(std::make_pair(w,x));
        }else{
            auto it=it_b.first;
            if(it->second > w){
                auto it2=sorted.find(std::make_pair(it->second,x));
                assert(it2!=sorted.end());
                sorted.erase(it2);

                sorted.insert(std::make_pair(w,x));
                it->second=w;
            }
        }
    }

    T pop()
    {
        assert(!sorted.empty());
        auto wx=*sorted.begin();
        sorted.erase(sorted.begin());
        current.erase(wx.second);
        //fprintf(stderr, "  w=%g, s=%u\n", wx.first, current.size());
        return wx.second;
    }
};

namespace std{


template<>
struct hash<std::vector<float>>
{
    size_t operator()(const std::vector<float> &x) const
    {
        uint64_t hash=x.size();
        auto r=(const uint32_t *)&x[0];
        for(unsigned i=0; i<x.size(); i++){
            hash=hash*19937 + r[i] + (hash>>32);
        }
        return hash;
    }
};

};

template<class F>
void enum_box(const std::vector<float> &x, std::mt19937_64 &rng, const F &f, int i=0)
{
    std::vector<float> xx(x);

    for(int i=0; i<x.size(); i++){
        std::normal_distribution<> g;

        for(int j=0; j<x.size(); j++){
            int dist=(int)(1024*g(rng));
            xx[i]=boost::math::float_advance(x[i], dist);
        }
        f(xx);
    }

    for(int i=0; i<x.size(); i++){
        std::normal_distribution<> g;

        for(int j=0; j<x.size(); j++){
            int dist=(int)(256*g(rng));
            xx[i]=boost::math::float_advance(x[i], dist);
        }
        f(xx);
    }
    

    for(int i=0; i<x.size(); i++){
        std::normal_distribution<> g;

        for(int j=0; j<x.size(); j++){
            int dist=(int)(16*g(rng));
            xx[i]=boost::math::float_advance(x[i], dist);
        }
        f(xx);
    }

    

    xx=x;

    int o=rng()%x.size();
    for(int i=0; i<x.size(); i++){
        int io=(i+o)%x.size();
        xx[io]=nextafterf(x[io], INFINITY);
        f(xx);

        xx[io]=nextafterf(x[io], -INFINITY);
        f(xx);

        xx[io]=x[io];
    }
}

struct kv
{
    double w;
    size_t h;
    std::vector<float> x;

    kv(double _w, const std::vector<float> &_x)
        : w(_w)
        , h(std::hash<std::vector<float>>()(_x))
        , x(_x)
    {
    }

    bool operator<(const kv &o) const 
    {
        if(w<o.w) return true;
        if(w>o.w) return false;
        if(h<o.h) return true;
        if(h>o.h) return false;
        return x < o.x;
    }

    bool operator>(const kv &o) const
    {
        if(w>o.w) return true;
        if(w<o.w) return false;
        if(h>o.h) return true;
        if(h<o.h) return false;
        return x > o.x;
    }

};

double optimise_de(const OptPoly &poly, std::vector<float> &x0, int n_gen=10000, int max_fn_eval=1000000)
{
    optim::algo_settings_t settings;
    settings.vals_bound=1;
    settings.lower_bounds.ones(poly.n+poly.m+2);
    settings.lower_bounds *= -2;
    settings.upper_bounds.ones(poly.n+poly.m+2);
    settings.upper_bounds *= 2;
    settings.print_level=2;
    settings.de_settings.mutation_method=1;
    settings.de_settings.n_gen=n_gen;
    settings.de_settings.max_fn_eval = max_fn_eval;

    arma::vec vx0;
    vx0=std::vector<double>(x0.begin(), x0.end());

    optim::de_prmm(vx0, poly.get_eval_arma(), nullptr, settings);

    x0.assign(vx0.colptr(0), vx0.colptr(0)+x0.size());

    return poly.eval(&x0[0]);
}

double optimise_nm(const OptPoly &poly, std::vector<float> &x0)
{
    optim::algo_settings_t settings;
    settings.print_level=2;

    arma::vec vx0;
    vx0=std::vector<double>(x0.begin(), x0.end());

    optim::nm(vx0, poly.get_eval_arma(), nullptr, settings);

    x0.assign(vx0.colptr(0), vx0.colptr(0)+x0.size());

    return poly.eval(&x0[0]);
}

double optimise_tabu(const OptPoly &poly, std::vector<float> &x0, std::mt19937_64 &rng)
{
    double bestErr=poly.eval(&x0[0]);       
    std::vector<float> bestX=x0;

    using KV=kv;
    std::priority_queue<
        KV,
        std::vector<KV>,
        std::greater<KV>
    > q;
    std::unordered_set<std::vector<float> > done;

    q.push({bestErr, x0});

    std::vector<float> todo=x0;
    unsigned i=0;
    unsigned trials=0;

    unsigned repeats=0;

    while(1){       
        if((i&0xFF)==0){
            fprintf(stderr, "  t=%u, s=%u, repeats=%u, head=%g, opt=%g\n", trials, q.size(), repeats, q.top().w, bestErr);
            fprintf(stderr, "  %llx, ", std::hash<std::vector<float>>()(q.top().x));
            for(unsigned i=0; i<q.top().x.size(); i++){
                fprintf(stderr, " %.9g", q.top().x[i]);
            }
            fprintf(stderr, "\n");
            fprintf(stderr, "      ");
            for(unsigned i=0; i<q.top().x.size(); i++){
                fprintf(stderr, " %a", q.top().x[i]);
            }
            fprintf(stderr, "\n");
        }
        todo=q.top().x;   
        q.pop();  
        enum_box(todo, rng, [&](const std::vector<float> &x) {
            if(done.insert(x).second){
                ++trials;
                std::vector<double> tmp{x.begin(),x.end()};
                double err=poly.eval_avx2(&tmp[0]);
                if(err < bestErr){
                    fprintf(stderr, "i=%d, trials=%d, got=%.16g\n", i, trials, err);
                    bestErr=err;
                    bestX=x;
                    x0=x;
                }
                q.push({err,x});
            }else{
                repeats++;
            }
        });

        ++i;
    }


}

int main()
{
    std::vector<float> x0{
        -0.01413606583476082f,
        -0.007433195916076948f,
        -0.001604790561658129f,
        -0.0001531624443904903f,
        
        -0.01413606586298593f,
        0.006702872352956657f,
        -0.001239663901569256f,
        9.12546890524298e-05f
    };


    OptPoly poly;
    poly.n=3;
    poly.m=3;
    poly.use_fma=false;

    for(int i=0; i<(1<<16); i++){
        float x=((float)i) * (1.0f/65536.0f);
        poly.x.push_back( x );
        poly.y.push_back( std::exp((double)x) );
    }

    std::cerr<<std::setprecision(12); 
    std::cerr<<poly.eval(&x0[0])<<"\n";
    std::vector<double> x0f(x0.begin(), x0.end());
    std::cerr<<poly.eval_avx2(&x0f[0])<<"\n";

    std::mt19937_64 rng;

    if(0){
        double bestErr=poly.eval(&x0[0]);       
        std::vector<float> bestX=x0;

        using KV=kv;
        std::priority_queue<
            KV,
            std::vector<KV>,
            std::greater<KV>
        > q;
        std::unordered_set<std::vector<float> > done;

        q.push({bestErr, x0});

        std::vector<float> todo=x0;
        unsigned i=0;
        unsigned trials=0;

        unsigned repeats=0;

        while(1){       
            if((i&0xFF)==0){
                fprintf(stderr, "  t=%u, s=%u, repeats=%u, head=%g, opt=%g\n", trials, q.size(), repeats, q.top().w, bestErr);
                fprintf(stderr, "  %llx, ", std::hash<std::vector<float>>()(q.top().x));
                for(unsigned i=0; i<q.top().x.size(); i++){
                    fprintf(stderr, " %.9g", q.top().x[i]);
                }
                fprintf(stderr, "\n");
                fprintf(stderr, "      ");
                for(unsigned i=0; i<q.top().x.size(); i++){
                    fprintf(stderr, " %a", q.top().x[i]);
                }
                fprintf(stderr, "\n");
            }
            todo=q.top().x;   
            q.pop();  
            enum_box(todo, rng, [&](const std::vector<float> &x) {
                if(done.insert(x).second){
                    ++trials;
                    std::vector<double> tmp{x.begin(),x.end()};
                    double err=poly.eval_avx2(&tmp[0]);
                    if(err < bestErr){
                        fprintf(stderr, "i=%d, trials=%d, got=%.16g\n", i, trials, err);
                        bestErr=err;
                        bestX=todo;
                    }
                    q.push({err,x});
                }else{
                    repeats++;
                }
            });

            ++i;
        }
    }else if(0){
        double bestErr=poly.eval(&x0[0]);       
        std::vector<float> bestX=x0;

        updating_queue<std::vector<float>> q;
        std::unordered_set<std::vector<float> > done;

        std::vector<float> todo=x0;
        unsigned i=0;

        while(1){
            done.insert(todo);
            double err=poly.eval(&todo[0]);
            
            if(err < bestErr){
                fprintf(stderr, "i=%d, got=%.16g\n", i, err);
                bestErr=err;
                bestX=todo;
            }
            
            enum_box(todo, rng, [&](const std::vector<float> &x) {
                if(done.find(x)==done.end()){
                    q.update(err, x);
                }
            });

            todo=q.pop();
            ++i;
        }

    }else if(0){
        
        for(int i=0; i<x0.size(); i++){
            x0[i]=0;
        }

        double bestErr=poly.eval(&x0[0]);
        std::vector<float> bestX=x0;

        std::set<std::vector<float>> done;

        for(int i=0; i<100000; i++){
            std::vector<float> x=poly.perturb(rng, bestX);
            if(done.insert(x).second){

                double got=poly.eval(&x[0]);
                if(got < bestErr){
                    fprintf(stderr, "i=%d, got=%.16g\n", i, got);
                    bestErr=got;
                    bestX=x;
                }
            }
        }

    }else{
        std::cerr<<"de = "<<optimise_de(poly, x0, 1000)<<"\n";
        std::cerr<<"nm = "<<optimise_nm(poly, x0)<<"\n";
        std::cerr<<"tabu = "<<optimise_tabu(poly, x0, rng)<<"\n";
    }
}