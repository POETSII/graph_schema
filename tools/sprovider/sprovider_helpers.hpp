#ifndef sprovider_helpers_hpp
#define sprovider_helpers_hpp

#include "sprovider_types.h"

////////////////////////////////////////////////////////////
// Needed for providers

struct empty_struct_tag;

template<class P, class S>
struct pair_prop_state
{
    static const int props_size = ((sizeof(P)+7)>>3)*8;
    static const int state_size = ((sizeof(S)+7)>>3)*8;
    static const int size = props_size+state_size;
};

template<class P>
struct pair_prop_state<P,empty_struct_tag>
{
    static const int props_size = ((sizeof(P)+7)>>3)*8;
    static const int state_size = 0;
    static const int size = props_size+state_size;
};

template<class S>
struct pair_prop_state<empty_struct_tag,S>
{
    static const int props_size = 0;
    static const int state_size = ((sizeof(S)+7)>>3)*8;
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

SPROVIDER_ALWAYS_INLINE size_t calc_TDS_size(const TypedDataPtr &p)
{
    return ((p.payloadSize()+7)>>3)*8;
}

SPROVIDER_ALWAYS_INLINE size_t calc_P_S_size(const TypedDataPtr &p, const TypedDataPtr &s)
{
    return calc_TDS_size(p)+calc_TDS_size(s);
}

SPROVIDER_ALWAYS_INLINE void copy_P_S(void *dst, const TypedDataPtr &p, const TypedDataPtr &s)
{
    memcpy(dst, p.payloadPtr(), calc_TDS_size(p));
    memcpy(((char*)dst)+calc_TDS_size(p), s.payloadPtr(), calc_TDS_size(s));
}

const void *alloc_copy_P(const TypedDataPtr &p)
{
    void *res=malloc(calc_TDS_size(p));
    memcpy(res, p.payloadPtr(), calc_TDS_size(p));
    return res;
}

#endif
