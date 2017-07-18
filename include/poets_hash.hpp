

class POETSHash
{
private:
    uint64_t m_hash;
public:
    typedef uint64_t hash_t;

    // At the moment this is just 64-bit fnv1a 

    POETSHash()
        : m_hash(0xcbf29ce484222325ull)
    {}
    
    hash_t getHash() const
    { return m_hash; }
    
    void add(uint8_t x)
    {
        m_hash ^= x;
        m_hash *= 0x100000001B3ull;
    }

    void add(uint64_t x)
    {
        for(int i=0;i<8;i++){
            add(x&0xFF);
            x=x>>8;
        }
    }
    
    void add(uint32_t x)
    {
        for(int i=0;i<4;i++){
            add(x&0xFF);
            x=x>>8;
        }
    }

    void add(int32_t x)
    { add( *(uint32_t*)&x ); }
    
    void add(float x)
    { add( *(uint32_t*)&x ); }
    
    void add(const uint8_t *x, size_t s)
    {
        for(size_t i=0; i<s; i++){
            add(x[i]);
        }
    }
    
    void add(const char *x)
    {
        while(*x){
            add(*x);
            ++x;
        }
    }
    
};
