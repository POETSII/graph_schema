#ifndef sprovider_helpers_hpp
#define sprovider_helpers_hpp

#include "sprovider_types.h"

#include "valgrind/memcheck.h"

////////////////////////////////////////////////////////////
// Needed for providers

struct empty_struct_tag;

template<class P, class S>
struct pair_prop_state
{
    static const int props_size = ((sizeof(P)+3)>>2)*4;
    static const int state_size = ((sizeof(S)+3)>>2)*4;
    static const int size = props_size+state_size;
};

template<class P>
struct pair_prop_state<P,empty_struct_tag>
{
    static const int props_size = ((sizeof(P)+3)>>2)*4;
    static const int state_size = 0;
    static const int size = props_size+state_size;
};

template<class S>
struct pair_prop_state<empty_struct_tag,S>
{
    static const int props_size = 0;
    static const int state_size = ((sizeof(S)+3)>>2)*4;
    static const int size = props_size+state_size;
};

template<>
struct pair_prop_state<empty_struct_tag,empty_struct_tag>
{
    static const int props_size = 0;
    static const int state_size = 0;
    static const int size = props_size+state_size;
};

template<class P,class S>
SPROVIDER_ALWAYS_INLINE const P *get_P(void *pv)
{ return (const P*)pv; }

template<class P,class S>
SPROVIDER_ALWAYS_INLINE S *get_S(void *pv)
{ return (S*)((char*)pv+pair_prop_state<P,S>::props_size); }



#include "graph_core.hpp"

SPROVIDER_ALWAYS_INLINE size_t calc_TDS_size_padded(const TypedDataPtr &p)
{
    return p ? ((p.payloadSize()+3)&0xFFFFFFFCul) : 0;
}

SPROVIDER_ALWAYS_INLINE size_t calc_TDS_size(const TypedDataPtr &p)
{
    return p ? p.payloadSize() : 0;
}

SPROVIDER_ALWAYS_INLINE size_t calc_P_S_size(const TypedDataPtr &p, const TypedDataPtr &s)
{
    return calc_TDS_size_padded(p)+calc_TDS_size_padded(s);
}

SPROVIDER_ALWAYS_INLINE void copy_P_S(void *dst, const TypedDataPtr &p, const TypedDataPtr &s)
{
#ifdef POEMS_ENABLE_VALGRIND_MEMCHECK
    assert(p ? 0==VALGRIND_CHECK_MEM_IS_DEFINED(p.payloadPtr(), p.payloadSize()) : 1);
    assert(s ? 0==VALGRIND_CHECK_MEM_IS_DEFINED(s.payloadPtr(), s.payloadSize()) : 1);
#endif

    memcpy(dst, p.payloadPtr(), calc_TDS_size(p));
    memcpy(((char*)dst)+calc_TDS_size_padded(p), s.payloadPtr(), calc_TDS_size(s));
}

const void *alloc_copy_P(const TypedDataPtr &p)
{
    void *res=malloc(calc_TDS_size_padded(p));
    memcpy(res, p.payloadPtr(), calc_TDS_size(p));
    return res;
}

std::string get_json_properties(const DeviceTypePtr &p, void *ps)
{
    std::string res="";
    unsigned psize=p->getPropertiesSpec()->payloadSize();
    unsigned ssize=p->getStateSpec()->payloadSize();

    if(psize>0){
        TypedDataPtr copy=p->getPropertiesSpec()->create();
        memcpy(copy.payloadPtr(), ps, psize);
        res=p->getPropertiesSpec()->toJSON(copy);
    }
    return "{"+res+"}";
}

std::string get_json_state(const DeviceTypePtr &p, void *ps)
{
    std::string res="";
    unsigned psize_pad=(p->getPropertiesSpec()->payloadSize()+3)&0xFFFFFFFCul;
    unsigned ssize=p->getStateSpec()->payloadSize();

    if(ssize>0){
        TypedDataPtr copy=p->getStateSpec()->create();
        memcpy(copy.payloadPtr(), psize_pad+(char*)ps, ssize);
        res=p->getStateSpec()->toJSON(copy);
    }
    return "{"+res+"}";
}

#endif
