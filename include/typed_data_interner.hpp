#ifndef typed_data_interner_hpp
#define typed_data_interner_hpp

#include "graph_core.hpp"

class TypedDataInterner
{
private:
    struct entry_t
    {
        POETSHash::hash_t hash;
        TypedDataPtr data;
        uint32_t index;
    };
    
    struct equal_entry_t
    {
        bool operator(const entry_t &o) const
        {
            if(hash!=o.hash)
                return false;
            return data==o.data;
        }
    };
    
    struct hash_entry_t
    {
        size_t operator(const entry_t &o) const
        {
            if(sizeof(size_t) < sizeof(o.hash)){
                return (o.hash>>32) ^ o.hash;
            }else{
                return o.hash;
            }
        }
    };


    std::unordered_set<entry_t> m_dataInstances;
    std::vector<TypedDataPtr> m_indexToInstance;
    
public:
    TypedDataPtr intern(const TypeDataPtr &o)
    {
        entry_t entry{ o.payloadHash(), o, m_dataInstances.size() };
        auto it=m_interned.insert( entry );
        if(it.second){
            m_indexToType.push_back(it.first->data);
        }
        return it.first->data;
    }

    uint32_t toIndex(const TypedDataPtr &d)
    {
        entry_t entry{ o.payloadHash(), o, m_dataInstances.size() };
        auto it=m_interned.insert( entry );
        if(it.second){
            m_indexToType.push_back(it.first->data);
        }
        return it.first->index;
    }
    
    TypedDataPtr fromIndex(uint32_t index)
    {
        return m_indexToType.at(index);
    }
};

#endif
