#ifndef cpp_op2_extra_hpp
#define cpp_op2_extra_hpp

#include "cpp_op2.hpp"

#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

#include <atomic>
#include <limits>
#include <mutex>


void atomic_add_double(double *of, double d)
{
    static_assert(sizeof(std::atomic<double>)==sizeof(double), "Assumptions about atomic double incorrect.");

    // !! HACK !!
    auto &f=*(std::atomic<double>*)of;
    
    // https://www.reddit.com/r/cpp/comments/338pcj/atomic_addition_of_floats_using_compare_exchange/
    double old = f.load();
    double desired;
    do{
        desired = old+d;
    } while ( !f.compare_exchange_weak(old, desired));
    // return desired;
}

double lock_double(double *of)
{
    static_assert(sizeof(std::atomic<double>)==sizeof(double), "Assumptions about atomic double incorrect.");

    // !! HACK !!
    auto &f=*(std::atomic<double>*)of;
    
    double prev;
    const double sentinel=std::numeric_limits<double>::signaling_NaN();
    do{
        prev=f.load();
    }while((prev!=prev) || !f.compare_exchange_weak(prev,sentinel));
    return prev;
}

void unlock_double(double *of,double prev)
{
    // !! HACK !!
    auto &f=*(std::atomic<double>*)of;
    
    f.store(prev);
}



enum op2_access_mode
{
    READ,
    WRITE,
    INC
};

template<class TMap,unsigned TIndex, op2_access_mode TMode, class TMember>
struct map_access
{
    map_access(const TMap &_map, const TMember &_member)
        : map(_map)
        , member(_member)
    {}
    
    typedef TMap map_type;
    const TMap &map;
    const TMember member;
    
    const unsigned index=TIndex;
    const op2_access_mode mode=TMode;
};

const std::integral_constant<unsigned,0> _0;
const std::integral_constant<unsigned,1> _1;
const std::integral_constant<unsigned,2> _2;
const std::integral_constant<unsigned,3> _3;


template<class TMap,unsigned TIndex,class TMember>
map_access<TMap,TIndex,INC,TMember> op2_inc(const TMap &map, std::integral_constant<unsigned,TIndex>, TMember member)
{ return map_access<TMap,TIndex,INC,TMember>{map,member}; }

template<class TMap,unsigned TIndex,class TMember>
map_access<TMap,TIndex,WRITE,TMember> op2_write(const TMap &map, std::integral_constant<unsigned,TIndex>, TMember member)
{ return map_access<TMap,TIndex,WRITE,TMember>{map}; }

template<class TMap,unsigned TIndex,class TMember>
map_access<TMap,TIndex,READ,TMember> op2_read(const TMap &map, std::integral_constant<unsigned,TIndex>, TMember member)
{ return map_access<TMap,TIndex,READ,TMember>{map,member}; }
 

template<class TType, op2_access_mode TMode>
struct global_access
{
    global_access(TType &_value)
        : value(_value)
    {}
    
    typedef TType value_type;
    TType &value;
    
    const op2_access_mode mode=TMode;
    
    TType working;
};

template<class T>
auto op2_inc(T &val) -> global_access<T,INC>
{
    return global_access<T,INC>(val);
}


template<class TMap,unsigned TIndex, op2_access_mode TMode, class TMember>
auto build_accessor(unsigned i, const map_access<TMap,TIndex,TMode,TMember> &acc)
        -> decltype( acc.map.otherSet[0].*(acc.member) )
{
    const auto &indices=acc.map[i];
    auto &other=acc.map.otherSet[indices[TIndex]];
    return other.*(acc.member);
}

auto build_accessor(unsigned i, const global_access<double,INC> &acc) -> double &
{
    return acc.value;
}



template<class TAccessor>
struct accessor_context
{    
    const TAccessor &_accessor;
    
    accessor_context(const TAccessor &__accessor, std::mutex &)
        : _accessor(__accessor)
    {}
    
    void pre_batch(unsigned len)
    {}
    
    void pre_item(unsigned)
    {}
    
    typedef decltype(build_accessor(0,_accessor)) accessor_type;    
    
    auto accessor(unsigned i) -> accessor_type
    { return build_accessor(i,_accessor); }
    
    void post_item(unsigned)
    {}
    
    void post_batch()
    {}
};

void clear_buffer(double &x)
{ x=0; }

void add_buffer(double &x, const double &y)
{
    atomic_add_double(&x, y);
}

void add_buffer_race(double &x, const double &y)
{
    x+=y;
}

template<unsigned long N>
void clear_buffer(std::array<double,N> &x)
{
    for(unsigned i=0; i<N; i++){
        x[i]=0;
    }
}

template<unsigned long N>
void add_buffer(std::array<double,N> &x, const std::array<double,N> &y)
{
    if(1){
        double x0=lock_double(&x[0]);
        
        for(unsigned i=1; i<N; i++){
            x[i] += y[i];
        }
        
        unlock_double(&x[0], x0+y[0]);
    }else{
        for(unsigned i=0; i<N; i++){
            x[i] += y[i];
        }
    }
}

template<unsigned long N>
void add_buffer_race(std::array<double,N> &x, const std::array<double,N> &y)
{
    for(unsigned i=0; i<N; i++){
        x[i] += y[i];
    }
}

/*
template<class TMap, unsigned TIndex,class TMember>
struct accessor_context<map_access<TMap,TIndex,INC,TMember> >
{    
    typedef map_access<TMap,TIndex,INC,TMember> accessor_t;
    
    const accessor_t &_accessor;

    
    typedef decltype(build_accessor(0,_accessor)) accessor_type;   
    
    typedef typename std::remove_reference<accessor_type>::type value_type;
    
    
    value_type buffer;
    
    accessor_context(const accessor_t &__accessor, std::mutex &)
        : _accessor(__accessor)
    {}
    
    void pre_batch(unsigned len)
    {}
    
    void pre_item(unsigned i)
    {
        clear_buffer(buffer);
    }
    
    
    auto accessor(unsigned i) -> accessor_type
    { return buffer; }
    
    
    void post_item(unsigned i)
    {
        value_type &target=build_accessor(i,_accessor);
        add_buffer(target, buffer);
    }
    
    void post_batch()
    {}
};
*/

template<class TMap, unsigned TIndex,class TMember>
struct accessor_context<map_access<TMap,TIndex,INC,TMember> >
{    
    typedef map_access<TMap,TIndex,INC,TMember> accessor_t;
    
    const accessor_t &_accessor;
    std::mutex &_mutex;

    
    typedef decltype(build_accessor(0,_accessor)) accessor_type;   
    
    typedef typename std::remove_reference<accessor_type>::type value_type;
    
    
    std::vector<std::pair<value_type*,value_type> > buffers;
    unsigned offset;
    
    accessor_context(const accessor_t &__accessor, std::mutex &__mutex)
        : _accessor(__accessor)
        , _mutex(__mutex)
    {}
    
    void pre_batch(unsigned len)
    {
        buffers.resize(len);
        offset=0;
    }
    
    void pre_item(unsigned i)
    {
        buffers[offset].first=&build_accessor(i,_accessor);
        clear_buffer(buffers[offset].second);
    }
    
    
    auto accessor(unsigned i) -> accessor_type
    { return buffers[offset].second; }
    
    
    void post_item(unsigned i)
    {
        offset++;
    }
    
    void post_batch()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for(unsigned i=0;i<buffers.size();i++){
            add_buffer_race(*buffers[i].first,buffers[i].second);
        }
    }
};

template<class TType>
struct accessor_context<global_access<TType,INC> >
{    
    typedef global_access<TType,INC> accessor_t;
    
    const accessor_t &_accessor;
    
    TType buffer;
    
    accessor_context(const accessor_t &__accessor, std::mutex &)
        : _accessor(__accessor)
    {}
    
    void pre_batch(unsigned len)
    {
        buffer=0;
    }
    
    void pre_item(unsigned i)
    {}
    
    typedef decltype(build_accessor(0,_accessor)) accessor_type;    
    
    auto accessor(unsigned i) -> accessor_type
    { return buffer; }
    
    void post_item(unsigned i)
    {}
    
    void post_batch()
    {
        double &target=build_accessor(0,_accessor);
        atomic_add_double(&target, buffer);
        buffer=0;
    }
};


template<class ...TAccessors>
struct accessor_contexts;

template<class THeadAccessor, class ...TTailAccessors>
struct accessor_contexts<THeadAccessor,TTailAccessors...>
{
    accessor_contexts(std::mutex &mutex, const THeadAccessor &head,const TTailAccessors &...tail)
        : m_head(head,mutex)
        , m_tail(mutex,tail...)
    {}
    
    accessor_context<THeadAccessor> m_head;
    accessor_contexts<TTailAccessors...> m_tail;
    
    template<unsigned TCount>
    struct accessor_type
    {
        typedef typename std::conditional<
            TCount!=0,
            typename accessor_contexts<TTailAccessors...>::template accessor_type<TCount-1>::type,
            typename accessor_context<THeadAccessor>::accessor_type
        >::type type;
    };
    
    void pre_batch(unsigned len)
    {
        m_head.pre_batch(len);
        m_tail.pre_batch(len);
    }
    
    template<unsigned TCount>
    auto accessor_impl(unsigned i, std::integral_constant<unsigned,0>) -> typename accessor_type<TCount>::type
    {
        return m_tail.accessor<TCount-1>(i);
    };
    
    template<unsigned TCount>
    auto accessor_impl(unsigned i, std::integral_constant<unsigned,1>) -> typename accessor_type<TCount>::type
    {
        return m_head.accessor(i);
    };
    
    
    void pre_item(unsigned i)
    {
        m_head.pre_item(i);
        m_tail.pre_item(i);
    }
    
    template<unsigned TCount>
    auto accessor(unsigned i) -> typename accessor_type<TCount>::type
    {
        return accessor_impl<TCount>(i,std::integral_constant<unsigned, (TCount==0)?1:0>{});
    };
    
    void post_item(unsigned i)
    {
        m_head.post_item(i);
        m_tail.post_item(i);
    }
    
    void post_batch()
    {
        m_head.post_batch();
        m_tail.post_batch();
    }
};

template<>
struct accessor_contexts<>
{
    accessor_contexts(std::mutex &)
    {}
    
    void pre_batch()
    {}
    
    template<unsigned TCount>
    struct accessor_type
    {
        typedef void type;
    };
    
    void pre_batch(unsigned len)
    {}
    
    void pre_item(unsigned i)
    {}
    
    template<unsigned TCount>
    auto accessor(unsigned i)
    {
        throw std::logic_error();
    };
    
    void post_item(unsigned i)
    {}
    
    void post_batch()
    {}
};


template<class TSetElt, class TKernel, class TGlobals, class TAccessorContexts, std::size_t... TSeq>
void exec_kernel(
    TSetElt &set,
    const TKernel &kernel,
    const TGlobals &globals,
    unsigned i,
    TAccessorContexts &contexts,
    std::index_sequence<TSeq...>
){
    (set.*kernel)(
        globals,
        contexts.template accessor<TSeq>(i)...
    );
}



template<class TKernel,class TSet,class TGlobals,class... TAccessors>
void parallel_for(
    const TKernel &kernel,
    TSet &set,
    const TGlobals &globals,
    TAccessors ...accessors
){
    typedef tbb::blocked_range<unsigned> range_t;
    range_t rGlobal(0, set.size(),256);
    
    std::mutex mutex;
    
    tbb::parallel_for(rGlobal,
        [&](const range_t &tLocal)
        {
            accessor_contexts<TAccessors...> contexts(mutex, accessors...);
            
            unsigned batchLength=tLocal.size();
                        
            contexts.pre_batch(batchLength);
            for(unsigned i=tLocal.begin(); i<tLocal.end(); i++){
                contexts.pre_item(i);
                exec_kernel(
                    set[i],
                    kernel,
                    globals,
                    i,
                    contexts,
                    std::index_sequence_for<TAccessors...>{}
                );
                contexts.post_item(i);
            }
            contexts.post_batch();
        }
    );
}

#endif
