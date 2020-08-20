#ifndef cuba_neuron_model_hpp
#define cuba_neuron_model_hpp

/*
https://brian2.readthedocs.io/en/2.0rc/examples/CUBA.html
*/

#include <cstdint>

#include "../shared_utils.hpp"


struct CUBA
{
    static float mul(float a, float b)
    { return a*b; }

    static float add(float a, float b)
    { return a+b; }

    static float sub(float a, float b)
    { return a-b; }

    static float div(float a, float b)
    { return a/b; }

    static float to_float(int32_t x)
    {
        return (float)x;
    }

    struct hasher
    {
        uint32_t acc=0;

        void operator()(const char *, float, float x)
        {
            union {
                uint32_t i;
                float f;
            }fi;
            fi.f=x;
            acc=acc*19937+fi.i;
        }

        void operator()(const char *, uint32_t, uint32_t x)
        {
            acc=acc*19937+x;
        }
    };

    template<class TP, class TCB>
    static void walk_properties(TP &p, TCB &cb)
    {
        cb("nid", uint32_t(), p.nid);
        cb("taum", float(), p.taum);
        cb("taue", float(), p.taue);
        cb("taui", float(),  p.taui);
        cb("Vt", float(), p.Vt);
        cb("Vr", float(), p.Vr);
        cb("El", float(), p.El);
        cb("refLen", uint32_t(), p.refLen);
        cb("Vo", float(), p.Vo);
    }

    struct properties_t
    {
        // Parameters
        
        uint32_t nid = 0;
        float taum = 0.020; // second
        float taue = 0.005; // second
        float taui = 0.010; // second
        float Vt = -0.050; // volt
        float Vr = -0.060; // volt
        float El = -0.049; // volt
        uint32_t refLen = 50; // 5ms / 0.1ms
        float Vo = 0;

        template<class TCB>
        void walk(TCB &cb)
        {
            walk_properties(*this, cb);
        }
    };


    template<class TS, class TCB>
    static void walk_state(TS &s, TCB &cb)
    {
        cb("v", float(), s.v);
        cb("ge", float(), s.ge);
        cb("gi", float(), s.gi);
        cb("refSteps", uint32_t(), s.refSteps);
        cb("vp", float(), s.vp);
    }

    struct state_t
    {
        // State
        float v;
        float ge;
        float gi;
        uint32_t refSteps; // Refractory time
        float vp; // v in previous time-step. Used to get distinct hashes on spike

        template<class TCB>
        void walk(TCB &cb)
        {
            walk_state(*this, cb);
        }
    };

    

    static const int STIMULUS_COUNT = 2;
    
    template<class THL, class TP, class TS>
    static void reset(THL handler_log, const TP &p, TS &s, uint64_t seed)
    {
        s.v=p.Vo;
        s.ge=0;
        s.gi=0;
        s.refSteps=0;
        s.vp=0;
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
        // stimulus is an integer number of uV
        const float SCALE=1e-6; // Get stimulus in volts

        float ge=s.ge;
        float gi=s.gi;
        float v=s.v;
        float vp=v;

        //-fprintf(stderr, "pos_stim=%d, neg_stim=%d\n", pos_stim, neg_stim);

        handler_log(4,"ge=%g, gi=%g, v=%g\n", ge, gi, v);

        ge = add(ge, mul(to_float(pos_stim) , SCALE) );
        gi = add(gi, mul(to_float(neg_stim) , SCALE) );

        bool fire=false;

        //dv/dt  = (ge+gi-(v-El))/taum : volt (unless refractory)
        unsigned refSteps=s.refSteps;
        if(refSteps==0){
            v = add(v, mul( sub(add(ge,gi), sub(v,p.El)), div(dt,p.taum) ) );
            //fprintf(stderr, "s.v=%f\n", v);
            if( v > p.Vt){
                v=p.Vr;
                fire=true;
                refSteps=p.refLen;
                s.refSteps=refSteps;
            }
        }else{
            refSteps--;
            s.refSteps=refSteps;
        }

        //dge/dt = -ge/taue : volt
        ge = sub( ge , mul( ge, div(dt,p.taue) ) ); // volt
        // dgi/dt = -gi/taui : volt
        gi = sub( gi, mul( gi, div(dt,p.taui ) ) );

        s.ge=ge;
        s.gi=gi;
        s.v=v;
        s.vp=vp;

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
