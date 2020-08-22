#ifndef random_neuron_model_hpp
#define random_neuron_model_hpp

/*
https://brian2.readthedocs.io/en/2.0rc/examples/Izhikevich.html
*/

#include <cstdint>

#include "../shared_utils.hpp"

struct poisson
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
        cb("threshold", uint32_t(), p.threshold);
        cb("iterations", uint32_t(), p.iterations);
        cb("_pad_", uint32_t(), p._pad_);
    }

    struct properties_t
    {
        // Parameters
        uint64_t rngO = 0;
        uint32_t nid = 0;
        uint32_t threshold = (uint32_t)ldexp(0.01, 32); // 100 steps / spike by default
        uint32_t iterations = 1;
        uint32_t _pad_ =0;

        template<class TCB>
        void walk(TCB &cb)
        {
            walk_properties(*this,cb);
        }
    };

    static_assert(sizeof(properties_t)==24, "Manually calculated size doesnt match.");

    template<class TS, class TCB>
    static void walk_state(TS &s, TCB &cb)
    {
        cb("rng", uint64_t(), s.rng);
    }

    struct state_t
    {
        uint64_t rng;

        template<class TCB>
        void walk(TCB &cb)
        {
            walk_state(*this,cb);
        }
    };
    
    template<class THL, class TP, class TS>
    static void reset(THL handler_log, const TP &p, TS &s, uint64_t seed)
    {
        s.rng=p.rngO+seed;
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
        s.rng += pos_stim - neg_stim;
        uint32_t rng=0;
        for(int i=0; i<p.iterations; i++){
            rng=MWC64X(&s.rng);
        }
        bool fire = rng < p.threshold;

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
