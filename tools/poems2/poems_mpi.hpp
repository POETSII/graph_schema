#include <vector>
#include <unordered_map>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <poll.h>
#include <time.h>

#include <mpi/mpi.h>

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

static const MAX_MESSAGE_SIZE = 64;

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
    { return &length; }


    static_assert(offsetof(Message,payload) == offsetof(Message,fanout_key)+sizeof(fanout_key_t), "Compiler has inserted unwanted padding.");
    static_assert(offsetof(Message,fanout_key) == offsetof(Message,length)+sizeof(message_length_t), "Compiler has inserted unwanted padding.");

    const int min_length = sizeof(message_length_t)+sizeof(fanout_key_t);
    const int max_length = sizeof(message_length_t)+sizeof(fanout_key_t)+MAX_MESSAGE_SIZE;

    static_assert(sizeof(message_t)==max_length, "Compiler has inserted unwanted padding.");
};

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
    const address_t *address_list = 0;
    // These two should pack into a single pointer width
    address_count_t address_count = 0;
    address_t single_address = 0; // For the use-case where there is exactly one address, address_list can point here for locality.

    // Message is no longer needed. Get rid of it or recycle it
    virtual release() =0;

    // NOTE: This is explicitly non-virtual. If you ever try to destroy via this
    // class you are doing it wrong.
    ~Message()
    {
        assert(false); // Shouldn't destroy via pointer to Message
    }
};


struct MessageList
{
    Message *first=0;
    Message *last=0;
    unsigned count=0;

    ~MessageList()
    {
        while(first){
            Message *m=first;
            first=first->next;
            m->manager->release(m);
        }
        first=0;
        last=0;
        count=0;
    }

    MessageList(const MessageList &) = delete;
    MessageList &operator=(const MessageList &) = delete;

    // Gauranteed to leave o empty
    MessageList(MessageList &&o)
    {
        std::swap(first, o.first);
        std::swap(last, o.last);
        assert(o.empty());
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
        if(list.empty()){
            return;
        }

        last->next=list.first;
        list.first->prev=last;
        last=list.first;
        list.first=0;
        list.last=0;
    }
};


/* Example of single-message interface. Not very efficient.*/
class SingleMessage
    : public Message
{
public:
    virtual release()
    {
        delete this;
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

        void release() override
        {
            parent->m_free_list.push_back(this);
        }

        ~ManagedMessage()
        {
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
            m_free_list.push_back(&m_messages[i]);
        }
    }

    ~MessageManager()
    {
        assert(m_free_list.size()==m_messages.size()); // All messages should have been returned.
    }

    bool has_free_messages()
    {
        return !m_free_list.empty();
    }

    MessageList get_free_messages()
    {
        return std::move(m_free_list);
    }
};


/*
    This interface is designed to try and avoid copying messages as much
    as possible, and enable batching of messages where feasible. 
*/
class Endpoint
{
public:
    virtual ~Endpoint()
    {}

    enum EventFlags : unsigned{
        SEND = 1,
        RECV = 2,
        IDLE = 4,
        TIMEOUT = 8
    };

    /* Wait until at least one of the conditions is true.
        The flags mask must not be empty.
        Will return at least one event which was met.
    */
    virtual EventFlags wait(EventFlags flags, double timeoutNs) const=0;

    /*  Gets a list of buffers used to send messages. The list may be empty
        if there is no send capacity. */
    virtual MessageList get_send_buffers() =0;

    /* Append the given messages to the send queue.
        Messages can always be sent, with throttling on the ability to get
        buffers.
    */
    virtual void send(MessageList &&list) =0;

    using receive_callback_t = std::function<bool(const message_t *msg)>;

    /*  Receiving is done by callback, to avoid copying data as much as possible.
        \retval True if any messages were received
        The endpoint is allowed to keep processing more receive messages as long
        as the callback returns true. Once the callback returns false, the endpoint
        should eventually stop and try to return, though it may carry on for a while
        if much more efficient, e.g. to reach the end of a batch.
    */
    virtual bool receive(
        const receive_callback_t &cb
    )=0;
};


class TCPEndpoint
    : public Endpoint
{
private:
    MessageManager m_manager;

    endpoint_address_t m_peer;
    int m_socket;

    const int RECV_READ_SIZE  = 4096;

    std::vector<char> m_recv_buffer;

    MessageList m_send_queue;
    unsigned m_send_first_offset = 0; // Partial offset into the first packet in queue

    bool have_outgoing_data() const
    {
        return !m_send_queue.empty();
    }

    bool have_receive_message() const
    {
        if( m_recv_buffer.size() < message_t::min_length ){
            return false;
        }
        return *(const message_length_t*)&m_recv_buffer[0] <= m_recv_buffer.size();
    }

    void try_receive_add_data()
    {
        if(have_receive_message()){
            return;
        }

        size_t valid=m_recv_buffer.size();
        m_recv_buffer.resize(valid+RECV_READ_SIZE);
        int g=::recv(m_socket, &m_recv_packet[m_recv_valid], 4-m_recv_valid, MSG_DONTWAIT);
        if(g>0){
            m_recv_buffer.resize(valid+g);
            return true;
        }else if(g==0){
            // Other side has shut-down gracefully
            if(m_recv_buffer.size()!=0){
                throw std::runtime_error("Partial packet");
            }
            return false;
        }else if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR){
            return false;
        }else{
            throw std::runtime_error("Receive error.");
        }
    }

    // Attempt to get rid of all messages in the message_queue without blocking
    void try_send_batch()
    {
        while(!m_message_queue.empty()){

            // Number of messages we try to send at once. This is a tradeoff between the
            // overhead of the system call and the time spend setting up the data-structures.
            const int IOVEC_MAX=8; 
            iovec iovecs[IOVEC_MAX];
            int num_iovecs=0;

            Message *head=m_send_queue.first;
            while(head && num_iovecs<IOVEC_MAX){
                iovecs[num_iovecs].iov_base=head->data();
                iovecs[num_iovecs].iov_len=head->length;
                head=head->next;
                num_iovecs++;
            }

            if(m_send_first_offset>0){
                assert(m_send_first_offset < iovecs[0].iov_len);
                iovecs[0].iov_base = ((const char *)iovecs[0].iov_base) + m_send_first_offset;
                iovecs[0].iov_len -= m_send_first_offset;
            }

            struct msghdr hdr;
            memset(&hder, 0, sizeof(msghdr));
            hdr.msg_iov=iovecs;
            hdr.msg_iovlen=num_iovecs;
            
            int s=::sendmsg(m_socket, &hdr, MSG_DONTWAIT);
            if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR){
                throw std::runtime_error("Partial packet sent before peer closed.");
            }else if(s<=0){
                throw std::runtime_error("Send error.");
            }

            m_send_queue_bytes -= s;

            m_send_first_offset=0;
            int acc_data=0;
            for(unsigned i=0; i<num_iovecs; i++){
                int next_acc_data = acc_data + iovecs[i].iov_len;
                if(acc_data >= s){
                    m_send_queue.release_front();
                    acc_data=next_acc_data;
                }else{
                    m_send_first_offset=next_acc_data - s;
                    return;
                }
            }
        }
    }

    uint64_t now_ms()
    {
        timespec ts;
        if(0!=clock_gettime(CLOCK_MONOTONIC, &ts)){
            throw std::runtime_error("Broken clock");
        }
        return ts.tv_sec+(ts.tv_nsec/1000000);
    }
public:

    virtual EventFlags wait(EventFlags flags, double timeoutSecs)
    {
        assert(flags & EventFlags::RECV);
        assert(timeoutSecs >=0 );

        bool do_timeout=timeoutSecs>=0;

        assert(timeoutSecs>0);
        timeoutSecs=std::min(timeoutSecs, 60.0);

        uint64_t start_time=now_ms();
        uint64_t end_time= (flags & EventFlags::TIMEOUT) ? start_time+(unsigned)floor(timeoutSecs*1000) : UINT64_MAX;

        try_send_batch();
        try_receive_add_data();

        uint64_t now_time=start_time;
        while(1){
            EventFlags res=0;

            if( (flags & EventFlags::SEND) && m_manager.has_free_messages()){
                res |= EventFlags::SEND;
            }

            if( (flags & EventFlags::RECV) && have_receive_message()){
                res |= EventFlags::RECV;
            }

            if( (flags & EventFlags::TIMEOUT) && (now_time >= end_time) ){
                res |= EventFlags::TIMEOUT;
            }

            if(res){
                return res;
            }

            pollfd pfd;
            pfd.fd=m_socket;
            pfd.events=POLLIN;
            if(have_outgoing_data()){
                pfd.events |= POLLOUT;
            }
            pfd.revents=0;
            
            while(1){
                int g=::poll(&pfd, 1, std::min(60*1000ull, end_time-now_time));
                if(g<0){
                    if(EINTR){
                        continue;
                    }else{
                        throw std::runtime_error("Error during poll.");
                    }
                }

                if(pfd.revents & POLLOUT){
                    try_send_batch();
                }
                if(pfd.revents & POLLIN){
                    try_receive_add_data();
                }
                // Timeout also gets us here.
                break;
            }

            now_time = now_ms();
        }
    }

    bool receive(const Endpoint::receive_callback_t &cb) override
    {
        try_send_batch(); // We really want to get things into the channel, so always try to flush messages
        try_receive_add_data();

        bool did_recv=false;
        const char *begin=&m_recv_buffer[0];
        const char *curr=begin;
        const char *end=&m_recv_buffer[m_recv_buffer.size()-1];

        while(curr+sizeof(message_length_t) <= end){
            message_length_t length=*(const message_length_t*)curr;
            if(length < message_t::min_length || length > message_t::max_length){
                throw std::runtime_error("Invalid message size.");
            }
            if(curr+length > end){
                break; // Not enough data yet
            }
            const message_t *msg=(const message_t *)curr;
            cb(msg);
            did_recv=true;
            curr += length;
        }

        if(did_recv){
            m_recv_buffer.erase(m_recv_buffer.begin(), m_recv_buffer.begin()+(curr-begin));
        }
        
        return did_recv;
    }

    virtual void send(
        MessageList &&msgs
    )
    {
        m_send_queue.splice_back(msgs);
        try_send_batch();
    }
};

