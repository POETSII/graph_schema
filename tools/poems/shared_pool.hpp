#ifndef shared_pool_hpp
#define shared_pool_hpp

#include <mutex>
#include <vector>

template<class T>
struct shared_pool
{

private:
    static const int ALLOC_SIZE=256;
    static const int SHED_SIZE=4096;
    static const int MAX_LOCAL_POOL_SIZE=65536; // Is this too small for graphs with very high fanout?

    unsigned m_alloc_size;
    std::mutex m_mutex;
    std::vector<T*> m_global_pool;

    void alloc_block(std::vector<T*> &pool)
    {
        assert(pool.empty());
        unsigned target=ALLOC_SIZE;
        pool.reserve(target);
        {
            std::unique_lock<std::mutex> lk(m_mutex, std::try_to_lock);
            if(lk.owns_lock()){
                unsigned todo=std::min((unsigned)m_global_pool.size(), target);
                pool.insert(pool.end(), m_global_pool.end()-todo, m_global_pool.end());
                m_global_pool.resize(m_global_pool.size()-todo);
                // Leaving the lock
            }
        }

        for(unsigned i=pool.size(); i<target; i++){
            pool.push_back((T*)malloc(m_alloc_size));
        }
    }

    void free_block(unsigned n, T **begin)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_global_pool.insert(m_global_pool.end(), begin, begin+n);
    }

public:
    shared_pool(unsigned alloc_size)
        : m_alloc_size(alloc_size)
    {}

    ~shared_pool()
    {
        for(auto p : m_global_pool){
            free(p);
        }
        m_global_pool.clear();
    }

    struct local_pool
    {
        friend shared_pool;
    private:
        shared_pool &m_global_pool;
        std::vector<T*> m_local_pool;

        local_pool(shared_pool &global)
            : m_global_pool(global)
        {}

        local_pool() = delete;
        local_pool &operator=(const local_pool &) = delete;
    public:

        ~local_pool()
        {
            m_global_pool.free_block(m_local_pool.size(), &m_local_pool[0]);
            m_local_pool.clear();
        }

        T *alloc()
        {
            if(m_local_pool.empty()){
                m_global_pool.alloc_block(m_local_pool);
            }
            for(unsigned i=0; i<m_local_pool.size(); i++){
                assert(m_local_pool[i]);
            }
            auto res=m_local_pool.back();
            assert(res);
            m_local_pool.pop_back();
            return res;
        }

        void free(T *msg)
        {
            assert(msg);
            m_local_pool.push_back(msg);
            if(m_local_pool.size() > MAX_LOCAL_POOL_SIZE){
                assert(SHED_SIZE <= MAX_LOCAL_POOL_SIZE);
                // Get rid of the oldest ones as they are probably cold. We have to
                // shift the entire array down, but it is just a memmove of pointers.
                m_global_pool.free_block(SHED_SIZE, &m_local_pool[0]);
                m_local_pool.erase(m_local_pool.begin(), m_local_pool.begin()+SHED_SIZE);
            }
        }
    };

    local_pool create_local_pool()
    {
        return local_pool(*this);
    }

};

#endif
