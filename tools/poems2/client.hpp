
#include "endpoint.hpp"

#include <cstdlib>
#include <cstring>
#include <cmath>

#include "../sprovider/sprovider_types.h"

/*

// From  https://planetmath.org/goodhashtableprimes
// with additions at the bottom and top usin gsame method
static const size_t hash_table_sizes[]
{
    7,
    13,
    29,
    53,
    97,
    193,
    389,
    769,
    1543,
    3079,
    6151,
    12289,
    24593,
    49157,
    98317,
    196613,
    393241,
    786433,
    1572869,
    3145739,
    6291469,
    12582917,
    25165843,
    50331653,
    100663319,
    201326611,
    402653189,
    805306457,
    1610612741,
    3221225473,
    6442450967,
    12884901893,
    25769803799,
    51539607599,
    103079215111
};
static const unsigned hash_table_sizes_count=sizeof(hash_table_sizes)/sizeof(hash_table_sizes[0]);

class ep_es_map
{
private:
    unsigned m_elt_size=0; // Granularity of the table in bytes, including fanout key
    size_t m_length=0;   // Size of the hash-table for hash reduction purposes  
    size_t m_alloc_length=0; // Size of table including spill at the end
    char *m_data=0;

    size_t m_count=0;
    int m_length_index=-1;

    static const int G=sizeof(fanout_key_t);

    
    void resize_up()
    {
        ep_es_map tmp(m_elt_size-sizeof(fanout_key_t), m_length_index+1 );

        char *todo=m_data;
        char *end_todo=m_data+m_alloc_length*m_elt_size;
        while(todo < end_todo){
            if( *(fanout_key_t*) todo != INVALID_FANOUT_KEY ){
                tmp.add(*(fanout_key_t*) todo, todo +sizeof(fanout_key_t) );
            }
            todo += m_elt_size;
        }
        
        assert(m_count==tmp.m_count);

        swap(tmp);
    }

    size_t make_probe(fanout_key_t key)
    {
        return key % m_length; // TODO: something more complicated
    }


public:
    void swap(ep_es_map &o)
    {
        std::swap(m_elt_size, o.m_elt_size);
        std::swap(m_length, o.m_length);
        std::swap(m_alloc_length, o.m_alloc_length);
        std::swap(m_data, o.m_data);
        std::swap(m_count, o.m_count);
        std::swap(m_length_index, o.m_length_index);
    }

    ep_es_map()
    {}

    ep_es_map &operator=(const ep_es_map &) = delete;

    ep_es_map(int data_size, int size_index)
        : m_elt_size( ((data_size+G-1)/G)*G + sizeof(fanout_key_t) )
    {
        m_length=hash_table_sizes[size_index];
        m_alloc_length=(unsigned)floor(8+log2(m_length));

        m_data=(char*)malloc(m_alloc_length*m_elt_size);
        if(m_data==0){
            throw std::bad_alloc();
        }
        memset(m_data, -1, m_alloc_length*m_elt_size);
    }

    void add(fanout_key_t key, const void *data)
    {
        if(2*m_count == m_length){
            resize_up();
        }
        size_t probe=make_probe(key);
        char *base=m_data+m_elt_size*probe;
        
        unsigned steps=0;
        unsigned max_steps=m_alloc_length-m_length;
        while( *(fanout_key_t*)base == INVALID_FANOUT_KEY ){
            base += m_elt_size;
            steps++;
            if(steps>max_steps){
                throw std::runtime_error("Hashing function causes too many collisions.");
            }
        }
        *(fanout_key_t*)base = key;
        memcpy(base+sizeof(fanout_key_t), data, m_elt_size-sizeof(fanout_key_t));
    }

    void *lookup(fanout_key_t key)
    {
        size_t probe=make_probe(key); // TODO: something more complicated?
        char *base=m_data+m_elt_size*probe;
        while( *(fanout_key_t*)base != key ){
            base += m_elt_size;
        }
        return base+sizeof(fanout_key_t);
    }
};

*/

class Client
{
private:
    struct delivery_key_t
    {
        uint32_t device_type_index  : 16;
        uint32_t pin_index          : 15;
        uint32_t is_global_route    : 1; // This is going to leave this domain
        uint32_t has_large_ep_es    : 1; // If t is encoded as offset, rather than directly

        union{
            struct{ // locally delivered messages
                uint32_t dp_ds_offset;
                uint32_t ep_es_offset_or_value; // Either literal value, or the offset into g_ep_es in uint64s
            };
            uint64_t global_fanout_key; // globally delivery offset
        };
    };

    struct packed_delivery_list_t
    {
        size_t begin;
        size_t end;
    };

    struct delivery_list_t
    {
        const delivery_key_t *begin;
        const delivery_key_t *end;
    };

    // Indexed by fanout_key;
    std::unordered_map<uint64_t,delivery_list_t> m_global_receive_delivery;

    struct device_instance_t
    {
        union{
            unsigned device_type_index;
            uint64_t _pad_;
        };
        char dp_ds[];
    };

    std::vector<device_instance_t*> m_devices;
    void *m_gp;
    void *m_ctxt;

    std::vector<uint64_t> m_dp_ds;
    std::vector<uint64_t> m_large_ep_es;


    Endpoint *m_ep;

    delivery_list_t get_delivery_list(unsigned local_device_index, unsigned port_index);

    void do_deliver_local(const delivery_key_t &rk, const void *m)
    {
        void *dp_ds=&m_dp_ds[0] + rk.dp_ds_offset;
        void *ep_es=(void*)&rk.ep_es_offset_or_value;
        if( rk.has_large_ep_es ){
            ep_es = &m_large_ep_es[rk.ep_es_offset_or_value];
        }
        sprovider_do_recv(m_ctxt, rk.device_type_index, rk.pin_index, m_gp, dp_ds, ep_es, m);
    }

    active_flag_t try_send_or_compute(unsigned device_index, Message *m)
    {
        device_instance_t *inst=m_devices[device_index];

        int action_taken=-2;
        int output_port=-2;
        unsigned message_size;
        int send_index;

        auto active = sprovider_try_send_or_compute(
            m_ctxt, inst->device_type_index, m_gp, inst->dp_ds,
            &action_taken, &output_port, &message_size, &send_index, (void*)m->data()
        );
        if(output_port>=0){
            auto dkl = get_delivery_list(device_index, output_port);
            while(dkl.begin!=dkl.end){
                if(dkl.begin->is_global_route){
                    
                }else{
                    do_deliver_local(*dkl.begin, m);
                }
                dkl.begin++;
            }
        }

        m->length=m->min_length+message_size;
    
        return active;
    }


public:
}