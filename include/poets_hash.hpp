

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

    void add(uint16_t x)
    {
        for(int i=0;i<2;i++){
            add(x&0xFF);
            x=x>>8;
        }
    }


    void add(int32_t x)
    { add( (uint32_t)x ); }


    void add(int64_t x)
    { add( (uint64_t)x ); }

    void add(int16_t x)
    { add( (uint16_t)x ); }

    void add(int8_t x)
    { add( (uint8_t)x ); }

    
    void add(float x)
    {
        uint32_t tmp;
        memcpy(&tmp, &x, 4);
        add( tmp );
    }
    
    void add(double x)
    { 
        uint64_t tmp;
        memcpy(&tmp, &x, 8);
        add( tmp );
    }
    
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
