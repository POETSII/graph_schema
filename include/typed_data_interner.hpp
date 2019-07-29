#ifndef typed_data_interner_hpp
#define typed_data_interner_hpp

#include "graph_core.hpp"

#include <unordered_set>

class TypedDataInterner
{
public: 
    struct entry_t
    {
        uint64_t hash;
        TypedDataPtr data;
        size_t index;
    };
private:
    
    struct equal_entry_t
    {
        bool operator()(const entry_t &a, const entry_t &b) const
        {
            return a.data==b.data;
        }
    };
    
    struct hash_entry_t
    {
        size_t operator()(const entry_t &o) const
        {
            auto h=o.hash;
            if(sizeof(size_t) < sizeof(h)){
                return (h>>32) ^ h;
            }else{
                return h;
            }
        }
    };


    std::unordered_set<entry_t,hash_entry_t,equal_entry_t> m_dataInstances;
    std::vector<const entry_t*> m_indexToInstance;
    
    
    
    const entry_t &internImpl(const TypedDataPtr &o)
    {
        entry_t entry{ o.payloadHash(), o, m_dataInstances.size() };
        auto it=m_dataInstances.find( entry );
        if(it==m_dataInstances.end()){
            entry.data=o.clone();
            assert( entry.data==o );
            assert( entry.data.get()== 0 || entry.data.get()!=o.get() );
            assert( entry.data.get()==0 || entry.data.is_unique() );
            it=m_dataInstances.insert(it, entry);
            m_indexToInstance.push_back(&(*it));
        }
        return *it;
    }
public:
    const entry_t *intern(const TypedDataPtr &o)
    {
        return &internImpl(o);
    }
    
};

#endif
