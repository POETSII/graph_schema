#ifndef izhikevich_neuron_model_hpp
#define izhikevich_neuron_model_hpp

/*
https://brian2.readthedocs.io/en/2.0rc/examples/Izhikevich.html
*/

#include <cstdint>

#include "../shared_utils.hpp"

struct izhikevich
{
    static float to_float(int32_t x)
    { return (float)x; }

    struct hasher
    {
        uint32_t acc=0;

        void add(uint32_t x)
        {
            acc=(acc*19937+(acc>>16)) ^ x;
        }

        void operator()(const char *, float, float x)
        {
            union {
                uint32_t i;
                float f;
            }fi;
            fi.f=x;
            add(fi.i);
        }

        void operator()(const char *, uint32_t, uint32_t x)
        {
            add(x);
        }

        void operator()(const char *, uint64_t, uint64_t x)
        {
            add(uint32_t(x>>32));
            add(uint32_t(x));
        }
    };

    template<class TP, class TCB>
    static void walk_properties(TP &p, TCB &cb)
    {
        cb("rngO", uint64_t(), p.rngO);
        cb("nid", uint32_t(), p.nid);
        cb("a", float(), p.a);
        cb("b", float(), p.b);
        cb("c", float(), p.c);
        cb("d", float(), p.d);
        cb("i_offset", float(),  p.i_offset);
        cb("Ir", float(), p.Ir);
        cb("uO",float(),  p.uO);
        cb("vO", float(), p.vO);
        cb("_p_", uint32_t(), p._p_);
    }

    struct properties_t
    {
        // Parameters
        uint64_t rngO = 0;
        uint32_t nid = 0;
        float a = 0.02;
        float b = 0.2;
        float c = -65;
        float d = -8;
        float i_offset = 0.0;
        float Ir = 1;
        float uO = -14;
        float vO = -70;

        uint32_t _p_=0;

        template<class TCB>
        void walk(TCB &cb)
        {
            walk_properties(*this,cb);
        }
    };

    static_assert(sizeof(properties_t)==48, "Manually calculated size doesnt match.");

    template<class TS, class TCB>
    static void walk_state(TS &s, TCB &cb)
    {
        cb("u", float(), s.u);
        cb("v", float(), s.v);
        cb("rng", uint64_t(), s.rng);
    }

    struct state_t
    {
        uint64_t rng;
        float u;
        float v;

        template<class TCB>
        void walk(TCB &cb)
        {
            walk_state(*this,cb);
        }
    };

    static const int STIMULUS_COUNT = 2;
    
    template<class THL, class TP, class TS>
    static void reset(THL handler_log, const TP &p, TS &s, uint64_t seed)
    {
        s.u=p.uO;
        s.v=p.vO;
        s.rng=p.rngO+seed;
        for(int i=0; i<32; i++){
            MWC64X(&s.rng);
        }
    }

    template<class TP, class TS>
    static uint32_t hash(const TP &p, TS &s)
    {
        hasher h;
        walk_state(s, h);
        return h.acc;
    }

    template<class THL, class TP, class TS>
    static bool step(THL handler_log, const TP &p, TS &s, float dt, int32_t pos_stim, int32_t neg_stim) 
    {
        const float STIM_SCALE=ldexp(2,-16);

        float dt_ms=dt*1000;

        float v=s.v;
        float u=s.u;

        assert(pos_stim>=0);
        assert(neg_stim<=0);
        float I=to_float(pos_stim+neg_stim);
        I *= STIM_SCALE;
        if(p.Ir!=0.0f){
            float g = MWC64Gaussian_Tab64_CLT4(&s.rng);
            //fprintf(stderr, "  g=%.8g\n", g);
            g = g*p.Ir;
            I=I+g;
        }

        // v=v+dt*(0.04*(v*v)+5*v+140-u+I+i_offset);
        {
            float vR=v+dt_ms*(0.04f*(v*v)+5*v+140.0f-u+I+p.i_offset);

            float v2=v*v;
            float v2m=0.04f*v2;
            float v5=5.0f*v;
            float acc1=v2m+v5;
            float acc2=140-u;
            float acc3=acc1+acc2;
            float acc3a=I+p.i_offset;
            float acc4=acc3+acc3a;
            float acc5=acc4*dt_ms;
            v=v+acc5;

            //fprintf(stderr, "dt_ms=%g, v2=%g, v2m=%g, v5=%g, acc1=%g, acc2=%g, acc3=%g, acc3a=%g, acc5=%g\n",
            //    dt_ms, v2, v2m, v5, acc1, acc2, acc3, acc3a, acc5);

            //fprintf(stderr, "v=%g, vR=%g, aE=%g, aE=%g\n", v, vR, v-vR, (v-vR)/vR);
            //assert( std::abs((v-vR)/vR) < 1e-5 );
        }
        // u=u+a*(b*v-u);
        {
            float bv=p.b*v;
            float bv_u=bv-u;
            float a_bv_u=p.a*bv_u;
            u=u+a_bv_u;
        }
        bool fire = v >= 30.0f;
        if(fire){
            v=p.c;
            u = u+p.d;
        }

        s.v=v;
        s.u=u;

        return fire;
    }

    template<class THL, class TP, class TS>
    static void dump(THL handler_log, const TP &p, TS &s)
    {
        handler_log_dump<THL> dumper{handler_log, 3};
        walk_properties(p, dumper);
        walk_state(s, dumper);
    } 
};

#endif
