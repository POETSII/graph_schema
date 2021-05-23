#ifndef message_hpp
#define message_hpp

#include <vector>
#include <unordered_map>

#include <deque>
#include <cassert>
#include <stack>
#include <vector>
#include <algorithm>
#include <utility>
#include <stdexcept>

/* Identifies a particular endpoint. This is usually going to contain a cluster
    of actual devices. */
using endpoint_address_t = uint32_t;

using message_length_t = uint32_t;

// This is a token than means something to the receiver endpoint and allows it to fan it
// out in whatever way needed. The receiver key will be the same for every
// every destination endpoint, but will likely mean something different in each one.
// One approach is to assign a unique key for every (device-instance,output-port-name)
// pair, however, you'd really want to only allocate keys for nets which actually cross
// between endpoints
// Another optimisation might be to pack single-address destinations into the key somehow, though
// this would rely on endpoint implementation optimisations and careful device address mapping.
// We just treat this as an opaque value.
using fanout_key_t = uint32_t;

static const fanout_key_t INVALID_FANOUT_KEY = -1;

static const int MAX_MESSAGE_SIZE = 64;

/* This is designed so that exactly the same message can be sent to every
    destination endpoint, as it potentially makes scatter-style sends more efficient.
*/
struct message_t
{
    // Stuff that is actually transmitted
    message_length_t length ;     // Number of bytes, including this variable and fanout key
    fanout_key_t fanout_key;    // Used by receiver endpoints to work out how to process and locally fan it out

    char payload[MAX_MESSAGE_SIZE]; // Hopefully the compiled doesn't feel compelled to add padding before this array...

    const char *data() const
    { return (const char*)&length; }

    unsigned payload_length() const
    { return length-min_length; }

    static const int min_length = sizeof(message_length_t)+sizeof(fanout_key_t);
    static const int max_length = sizeof(message_length_t)+sizeof(fanout_key_t)+MAX_MESSAGE_SIZE;
};

static_assert(offsetof(message_t,payload) == offsetof(message_t,fanout_key)+sizeof(fanout_key_t), "Compiler has inserted unwanted padding.");
static_assert(offsetof(message_t,fanout_key) == offsetof(message_t,length)+sizeof(message_length_t), "Compiler has inserted unwanted padding.");
static_assert(sizeof(message_t)==message_t::max_length, "Compiler has inserted unwanted padding.");


class Message;

/*
This is a weird class, as we have public members and then a derived class.
We also derived a virtual from non-virtual class. It's so that all the common
stuff is direct on members, and only lifetime management gets handled virtually.

Messages are sized to the maximum that can be transmitted, and are
assumed to be small, e.g. around 128 bytes for each Message. It's
assumed that the MessageManager is allocated slabs of these messages,
then handing them out. Not sure whether it is possible to recover
locality and repack messages in order efficiently.

There is at least 40 bytes of overhead in the base-class members+vtable pointer.
Hopefully the derived class keeps it under 60 bytes.
*/
struct Message
    : message_t
{
public:    

    // Users of messages can be intrusive linked-lists
    Message *prev = 0;
    Message *next = 0;

    // Messages may have an associated address list.
    // The address list always has effective static lifetime, and must last longer than
    // any message pointing to it.
    const endpoint_address_t *address_list = 0;
    // These two should pack into a single pointer width
    endpoint_address_t address_count = 0; // This is an address so that it can hold a count the same size as the address space
    endpoint_address_t single_address = 0; // For the use-case where there is exactly one address, address_list can point here for locality.

    void set_header( endpoint_address_t dest, fanout_key_t fanout_key, unsigned payload_size)
    {
        assert(payload_size <= MAX_MESSAGE_SIZE);

        address_list = &single_address;
        address_count = 1;
        single_address = dest;

        this->fanout_key=fanout_key;
        length=min_length+payload_size;
    }

    void set_header(unsigned ndests, endpoint_address_t *pdests, fanout_key_t fanout_key, unsigned payload_size)
    {
        assert(payload_size <= MAX_MESSAGE_SIZE);

        address_list = pdests;
        address_count = ndests;

        this->fanout_key=fanout_key;
        length=min_length+payload_size;
    }


    // Message is no longer needed. Get rid of it or recycle it
    virtual void release() =0;

    virtual void add_ref(unsigned count) =0;

    // NOTE: If you ever try to destroy via this class you are doing it wrong. Use release
    virtual ~Message()
    {}
};

struct MessageList
{
    Message *first=0;
    Message *last=0;
    unsigned count=0;

    void sanity_check() const
    {
#ifndef NDEBUG
        unsigned n=0;
        const Message *prev=nullptr;
        const Message *curr=first;
        while(curr){
            ++n;
            if(prev){
                assert(curr->prev==prev);
            }else{
                assert(curr==first);
            }
            prev=curr;
            curr=curr->next;
        }
        if(prev){
            assert(last==prev);
        }
        assert(count==n);
#endif
    }

    MessageList()
    {}

    ~MessageList()
    {
        while(first){
            Message *m=first;
            first=first->next;
            m->release();
        }
        first=0;
        last=0;
        count=0;
    }

    MessageList(const MessageList &) = delete;
    MessageList &operator=(const MessageList &) = delete;

    MessageList(Message *m)
    {
        m->prev=0;
        m->next=0;
        first=m;
        last=m;
    }

    // Gauranteed to leave o empty
    MessageList(MessageList &&o)
    {
        swap(o);
        assert(o.empty());
    }

    void swap(MessageList &o)
    {
        std::swap(first, o.first);
        std::swap(last, o.last);
        std::swap(count, o.count);
    }

    bool empty() const
    { return !first; }

    unsigned size() const
    { return count; }

    void push_back(Message *m)
    {
        m->next=0;
        if(first==0){
            assert(last==0);
            first=m;
            m->prev=0;
        }else{
            m->prev=last;
            last->next=m;
        }
        last=m;
        ++count;
    }

    Message *pop_back()
    {
        assert(last);
        Message *res=last;
        last=res->prev;
        if(last){
            last->next=0;
        }else{
            first=0;
        }
        --count;
        return res;
    }

    Message *pop_back_or_null()
    {
        Message *res=last;
        if(res){
            last=res->prev;
            if(last){
                last->next=0;
            }else{
                first=0;
            }
            --count;
        }
        return res;
    }

    Message *pop_front()
    {
        assert(first);
        Message *res=first;
        first=res->next;
        if(first){
            first->prev=0;
        }else{
            last=0;
        }
        --count;
        return res;
    }

    void release_front()
    {
        Message *m=pop_front();
        m->release();
    }

    void splice_back(MessageList &&list)
    {
        sanity_check();
        list.sanity_check();

        if(list.empty()){
            return;
        }
        if(empty()){
            swap(list);
            return;
        }

        last->next=list.first;
        list.first->prev=last;
        last=list.last;
        count += list.count;

        list.first=0;
        list.last=0;
        list.count=0;
    }
};

struct MessageQueue
{
    const int MIN_CAPACITY=16;
    const int REBALANCE_ABS_THRESH=8;   // Always balance down if there are less than this number at the end
    const int REBALANCE_REL_THRESH=8;   // Balance down if count/capacity exceeds this

    Message **root=0;
    uint32_t offset=0;
    uint32_t count=0;
    uint32_t capacity=0;

    void sanity()
    {
#ifndef NDEBUG
        if(root){
            assert(offset+count<=capacity);
        }else{
            assert(offset==0);
            assert(count==0);
            assert(capacity==0);
        }
#endif
    }

    void ensure_extra_capacity(size_t extra_count)
    {
        sanity();

        if(offset+count+extra_count > capacity){
            // We have a number of choices:
            // - Shift down to make room
            // - Reallocate up to make room
            // - Steal the original's buffer. We'll ignore this one, as less likely.
            if( (offset+count+extra_count < capacity) && rebalance_cond() ){
                std::copy_backward(root+offset, root+offset+count, root);
                offset=0;
            }else{
                capacity=std::max((unsigned)MIN_CAPACITY, 2*capacity);
                while(capacity < offset+count+extra_count){
                    capacity*=2;
                }
                root=(Message**)realloc(root, sizeof(Message*)*capacity);
                if(root==0){
                    throw std::bad_alloc();
                }
            }
        }

        sanity();

    }

    MessageQueue()
    {}

    ~MessageQueue()
    {
        if(root){
            Message **begin=root+offset;
            Message **end=begin+count;
            while(begin!=end){
                (*begin)->release();
                begin++;
            }
            free(root);
            offset=0;
            count=0;
            root=0;
            capacity=0;
        }
    }

    MessageQueue(const MessageQueue &) = delete;
    MessageQueue &operator=(const MessageQueue &) = delete;

    MessageQueue(Message *m)
    {
        root=(Message**)malloc(sizeof(Message*)*MIN_CAPACITY);
        if(root==nullptr){
            throw std::bad_alloc();
        }
        root[0]=m;
        offset=0;
        count=1;
        capacity=MIN_CAPACITY;

        sanity();
    }

    // Gauranteed to leave o empty
    MessageQueue(MessageQueue &&o)
    {
        swap(o);
        assert(o.empty());
    }

    void swap(MessageQueue &o)
    {
        std::swap(root, o.root);
        std::swap(offset, o.offset);
        std::swap(count, o.count);
        std::swap(capacity, o.capacity);
    }

    bool empty() const
    { return !count; }

    unsigned size() const
    { return count; }

    bool rebalance_cond() const
    {
        return count <= REBALANCE_ABS_THRESH || REBALANCE_REL_THRESH * count < capacity;
    }

    void push_back(Message *m)
    {   
        sanity();
        ensure_extra_capacity(1);
        root[offset+count]=m;
        count++;
        sanity();
    }

    Message *pop_back()
    {
        sanity();
        assert(count);
        count--;
        Message *res=root[offset+count];
        sanity();
        return res;
    }

    Message *pop_back_or_null()
    {
        Message *res=nullptr;
        if(count){
            count--;
            res=root[offset+count];
        }
        return res;
    }

    Message *pop_front()
    {
        sanity();
        assert(count);
        Message *res=root[offset];
        ++offset;
        --count;
        sanity();
        return res;
    }

    void release_front()
    {
        Message *m=pop_front();
        m->release();
    }

    void splice_back(MessageQueue &&list)
    {
        sanity();
        if(list.empty()){
            return;
        }

        if(empty()){
            swap(list);
        }

        ensure_extra_capacity(list.count);

        std::copy(list.root+list.offset, list.root+list.offset+list.count, root+offset+count);
        count += list.count;

        list.offset=0;
        list.count=0;
        sanity();
    }


    void splice_back(MessageList &&list)
    {
        sanity();
        if(list.empty()){
            return;
        }

        ensure_extra_capacity(list.count);
        assert(offset+count+list.count <= capacity);

        while(!list.empty()){
            Message *m=list.pop_front();
            assert(offset+count+1<=capacity);
            root[offset+count]=m;
            count++;
        }

        sanity();
    }
};


/* Example of single-message interface. Not very efficient.*/
class SingleMessage
    : public Message
{
private:
    unsigned count=1;
public:
    virtual void release()
    {
        if(--count==0){
            delete this;
        }
    }

    virtual void add_ref(unsigned extra)
    {
        count+=extra;
    }

    ~SingleMessage()
    {
        // This is non-virtual, and does not call the base-class destructor
    }
};

class MessageManager
{   
private:
    class ManagedMessage
        : public Message
    {
    public:
        MessageManager *parent;
        unsigned count;

        void release() override
        {
            if(--count==0){
                this->count=1;
                parent->m_free_list.push_back(this);
            }
        }

        void add_ref(unsigned size) override
        {
            count+=size;
        }

        ~ManagedMessage()
        {
            assert(parent==0); // Must be destroyed by parent, not directly
        }
    };

    const int PAGE_SIZE=4096;
    const int MESSAGES_PER_PAGE=PAGE_SIZE / sizeof(ManagedMessage);

    MessageList m_free_list;
    std::vector<ManagedMessage> m_messages;
public:
    MessageManager(const MessageManager &m) = delete;
    MessageManager &operator=(const MessageManager &m) = delete;

    MessageManager(unsigned pool_size)
    {
        m_messages.resize(pool_size);
        for(unsigned i=0; i<pool_size; i++){
            m_messages[i].parent=this;
            m_messages[i].count=1;
            m_free_list.push_back(&m_messages[i]);
        }
    }

    MessageManager(MessageManager &&o)
    {
        assert(o.m_free_list.size() == o.total_messages());
        // We just abandon the other manager and it will get destructed.
        // Life is too short, and this is not performance critical
        
        m_messages.resize(o.total_messages());
        for(unsigned i=0; i<m_messages.size(); i++){
            m_messages[i].parent=this;
            m_messages[i].count=1;
            m_free_list.push_back(&m_messages[i]);
        }
    }

    ~MessageManager()
    {
        assert(m_free_list.size()==m_messages.size()); // All messages should have been returned.
        while(!m_free_list.empty()){
            auto m=m_free_list.pop_back();
            auto mm=dynamic_cast<ManagedMessage*>(m);
            assert(mm && mm->parent==this);
            mm->parent=nullptr;
        }
    }

    size_t total_messages() const
    { return m_messages.size(); }

    bool has_free_messages()
    {
        return !m_free_list.empty();
    }

    MessageList &get_free_message_list()
    {
        return m_free_list;
    }
};

class SlidingByteWindow
{
private:
    const int SMALL_COPY_ABS_THRESH = 256;
    const int BIG_COPY_RATIO_THRESH = 4;

    int m_offset = 0;
    int m_reserve_base = 0;
    std::vector<char> m_buffer;
public:
    void push_back(size_t n, const char *p)
    {
        assert(m_reserve_base==0);
        m_buffer.insert(m_buffer.end(), p, p+n);
    }

    char *reserve_back(size_t space)
    {
        assert(m_reserve_base==0);
        m_reserve_base=m_buffer.size();
        m_buffer.resize(m_reserve_base+space);
        return &m_buffer[m_reserve_base];
    }

    void commit_back(size_t count)
    {
        assert(m_buffer.size()-m_reserve_base >= count);
        m_buffer.resize(m_reserve_base+count);
        m_reserve_base=0;
    }

    size_t size() const
    {
        assert(m_reserve_base==0);
        return m_buffer.size()-m_offset;
    }

    bool empty() const
    {
        assert(m_reserve_base==0);
        return m_buffer.size()==m_offset;
    }

    const char *data() const
    {
        assert(m_reserve_base==0);
        assert(size()>0);
        return &m_buffer[m_offset];
    }

    void pop_front(size_t n)
    {
        assert(m_reserve_base==0);
        if(n==0){
            return;
        }
        
        m_offset += n;
        auto s=size();
        if(s==0){
            auto c=m_buffer.capacity(); // For debug assumptions 

            m_offset=0;
            m_buffer.clear(); 

            assert(m_buffer.capacity()==c); // Assumed to leave buffer allocated
        }else if( (s <= SMALL_COPY_ABS_THRESH) || ( m_buffer.size() > s * BIG_COPY_RATIO_THRESH  )){
            assert(m_offset>0);
            std::copy_backward( m_buffer.begin()+m_offset, m_buffer.end(), m_buffer.begin() );
            m_buffer.resize(s);
            m_offset=0;
        }
    }
};

#endif
