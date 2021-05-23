#ifndef file_channel_endpoint_hpp
#define file_channel_endpoint_hpp

#include "endpoint.hpp"

#include <memory>

#include <unistd.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <cmath>
#include <poll.h>

struct FileChannelPeer
{
    endpoint_address_t m_peer;
    std::shared_ptr<int> m_socket;

    uint64_t m_raw_bytes_read=0;
    uint64_t m_raw_calls_read=0;
    uint64_t m_raw_bytes_sent=0;
    uint64_t m_raw_bytes_written=0;
    uint64_t m_raw_calls_write=0;
    uint64_t m_raw_iovecs_queued=0;
    uint64_t m_raw_iovecs_sent=0;
    uint64_t m_messages_queued=0;
    uint64_t m_messages_sent=0;
    uint64_t m_messages_delivered=0;

    static const int RECV_READ_SIZE  = 4096;
    static const int IOVEC_MAX=256;
    static const int IOVEC_MIN=4; 

    static const int SEND_WINDOW_SIZE = 4096;
    static const int SEND_MIN_SIZE = 2048;

    std::vector<char> m_recv_buffer;

    MessageQueue m_send_queue;
    unsigned m_send_first_offset = 0; // Partial offset into the first packet in queue
    int m_send_iovec_count=IOVEC_MIN;

    SlidingByteWindow m_send_window;

    FileChannelPeer(FileChannelPeer &&) = default;

    FileChannelPeer(int fd, endpoint_address_t peer)
        : m_socket(new int(fd), [](int *x){ ::close(*x); delete x; } )
        , m_peer(peer)
    {
        auto prev=::fcntl(*m_socket, F_GETFL, 0);
        if(::fcntl(*m_socket, F_SETFL, prev | O_NONBLOCK) < 0){
            throw std::runtime_error("Couldn't put file into non-blocking mode.");
        }
    }

    FileChannelPeer(endpoint_address_t peer)
        : m_socket(new int(-1), [](int *x){ ::close(*x); delete x; } )
        , m_peer(peer)
    {
    }

    ~FileChannelPeer()
    {
        assert(!have_outgoing_data());
        assert(m_recv_buffer.empty());
    }

    bool have_outgoing_data() const
    {
        if(SEND_WINDOW_SIZE>0 && !m_send_window.empty()){
            return true;
        }
        return !m_send_queue.empty();
    }

    bool have_receive_message() const
    {
        if( m_recv_buffer.size() < message_t::min_length ){
            return false;
        }
        auto length=*(const message_length_t*)&m_recv_buffer[0];
        return length <= m_recv_buffer.size();
    }

    __attribute__ ((noinline)) void try_receive_add_data()
    {
        if(have_receive_message() || *m_socket==-1){
            return;
        }

        size_t valid=m_recv_buffer.size();
        m_recv_buffer.resize(valid+RECV_READ_SIZE);
        int g=::read(*m_socket, &m_recv_buffer[valid], RECV_READ_SIZE);
        int err=errno;
        m_recv_buffer.resize(valid+std::max(0,g));
        m_raw_bytes_read += std::max(0,g);
        m_raw_calls_read++;
        if(g>0){
            // Success
        }else if(g==0){
            // Other side has shut-down gracefully
            if(m_recv_buffer.size()!=0){
                throw std::runtime_error("Partial packet");
            }
        }else if(err==EAGAIN || err==EWOULDBLOCK || err==EINTR){
            return; // We'll go again later
        }else{
            throw std::runtime_error("Receive error.");
        }
    }

    // Attempt to get rid of all messages in the message_queue without blocking
    __attribute__ ((noinline)) void try_send_batch_queue(bool enforce_min_size)
    {
        assert(0);
#if 0

        assert(m_messages_queued==m_send_queue.size()+m_messages_sent);

        while(!m_send_queue.empty()){

            // Number of messages we try to send at once. This is a tradeoff between the
            // overhead of the system call and the time spend setting up the data-structures.
            struct iovec iovecs[IOVEC_MAX];
            int num_iovecs=0;
            int raw_bytes=0;

            Message *head=m_send_queue.first;
            while(head && num_iovecs<m_send_iovec_count){
                iovecs[num_iovecs].iov_base=(void*)head->data();
                iovecs[num_iovecs].iov_len=head->length;
                raw_bytes+=head->length;
                head=head->next;
                num_iovecs++;
            }

            if(m_send_first_offset>0){
                assert(m_send_first_offset < iovecs[0].iov_len);
                iovecs[0].iov_base = (void*)( ((const char *)iovecs[0].iov_base) + m_send_first_offset );
                iovecs[0].iov_len -= m_send_first_offset;
                raw_bytes -= m_send_first_offset;
            }

            int s=::writev(m_socket, iovecs, num_iovecs);
            m_raw_bytes_sent += raw_bytes;
            m_raw_bytes_written += std::max(0,s);
            m_raw_calls_write++;
            m_raw_iovecs_queued+=num_iovecs;
            
            if(s<=0){
                if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR){
                    return;
                }
            
                throw std::runtime_error("Send error.");
            }
            
            m_send_first_offset=0;
            int acc_data=0;
            int iovecs_sent=0;
            for(unsigned i=0; i<num_iovecs; i++){
                int next_acc_data = acc_data + iovecs[i].iov_len;
                if(next_acc_data <= s){
                    m_send_queue.release_front();
                    acc_data=next_acc_data;
                    m_messages_sent++;
                    ++iovecs_sent;
                }else{
                    m_send_first_offset=s-acc_data;
                    assert(m_send_first_offset < iovecs[i].iov_len);
                    break;
                }
            }

            m_raw_iovecs_sent+=iovecs_sent;

            if(iovecs_sent==num_iovecs){
                m_send_iovec_count=std::min(IOVEC_MAX, num_iovecs+1);
            }else if(iovecs_sent < (num_iovecs*3)/4){
                m_send_iovec_count=std::max(IOVEC_MIN, num_iovecs-4);
            }
        }

        assert(m_messages_queued==m_send_queue.size()+m_messages_sent);
#endif
    }

        // Attempt to get rid of all messages in the message_queue without blocking
    __attribute__ ((noinline)) void try_send_batch_window(bool enforce_min_size)
    {
        assert(SEND_WINDOW_SIZE>0);
        assert(*m_socket!=-1);

        while(1){
            while(!m_send_queue.empty() && m_send_window.size() < SEND_WINDOW_SIZE){
                Message *head=m_send_queue.pop_front();
                m_send_window.push_back(head->length, head->data());
                head->release();
                m_messages_sent++;
            }

            if(m_send_window.size() < (enforce_min_size ? SEND_MIN_SIZE : 1 ) ){
                break;
            }
            

            size_t to_send=m_send_window.size();
            assert(!enforce_min_size || to_send >= SEND_MIN_SIZE);
            int s=::write(*m_socket, m_send_window.data(), to_send);
            m_send_window.pop_front(std::max(0,s));
            m_raw_bytes_sent += to_send;
            m_raw_bytes_written += std::max(0,s);
            m_raw_calls_write++;
                
            if(s<=0){
                if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR){
                    break;
                }
        
                throw std::runtime_error("Send error.");
            }
            
            assert(m_messages_queued==m_send_queue.size()+m_messages_sent);

            if(s<to_send){
                break;
            }
        }
    }

    void try_send_batch(bool enforce_min_size)
    {
        if(SEND_WINDOW_SIZE>0){
            try_send_batch_window(enforce_min_size);
        }else{
            try_send_batch_queue(enforce_min_size);
        }
    }

    bool receive(const Endpoint::receive_callback_t &cb)
    {
        try_send_batch(true); // We really want to get things into the channel, so always try to flush messages
        try_receive_add_data();

        if(m_recv_buffer.empty()){
            return false;
        }

        bool did_recv=false;
        const char *begin=&m_recv_buffer[0];
        const char *curr=begin;
        const char *end=&m_recv_buffer[m_recv_buffer.size()];

        while(curr+sizeof(message_length_t) <= end){
            message_length_t length=*(const message_length_t*)curr;
            if(length < message_t::min_length || length > message_t::max_length){
                throw std::runtime_error("Invalid message size.");
            }
            if(curr+length > end){
                break; // Not enough data yet
            }
            const message_t *msg=(const message_t *)curr;
            cb(*msg);
            m_messages_delivered++;
            did_recv=true;
            curr += length;
        }

        if(did_recv){
            m_recv_buffer.erase(m_recv_buffer.begin(), m_recv_buffer.begin()+(curr-begin));
        }
        
        return did_recv;
    }

    void send(MessageList &&msgs)
    {
        m_messages_queued+=msgs.size();
        m_send_queue.splice_back(std::move(msgs));

        try_send_batch(true);
    }

    // It is assumed there is already a reference on the message
    void send(Message *m)
    {
        m_messages_queued++;

        if(m_send_window.size() >= SEND_WINDOW_SIZE ){
            m_send_queue.push_back(m);
        }else{
            m_send_window.push_back(m->length, m->data());
            m->release();
            m_messages_sent++;

            // If we exceed the size, try and send immediately
            if(m_send_window.size() >= SEND_WINDOW_SIZE ) {
                try_send_batch(true);
            }
        }
    }
};

class FileChannelEndpoint
    : public Endpoint
{
private:
    MessageManager m_manager;

    FileChannelPeer m_peer;

    uint64_t now_ms()
    {
        timespec ts;
        if(0!=clock_gettime(CLOCK_MONOTONIC, &ts)){
            throw std::runtime_error("Broken clock");
        }
        return ts.tv_sec+(ts.tv_nsec/1000000);
    }
public:
    FileChannelEndpoint(int fd, endpoint_address_t peer)
        : m_peer(fd, peer)
        , m_manager(1024) // TODO: how to choose this? Remember there is buffering in OS too...
    {
        m_self=1-peer; // There can only be two addresses
        m_send_buffer_list = &m_manager.get_free_message_list();
    }

    ~FileChannelEndpoint()
    {
        fprintf(stderr, "Reads=%llu, bytes/read=%lf\n", m_peer.m_raw_calls_read, m_peer.m_raw_bytes_read/(double) m_peer.m_raw_calls_read);
        fprintf(stderr, "Writes=%llu, bytes/write=%lf, bytes/send=%lf\n", m_peer.m_raw_calls_write, m_peer.m_raw_bytes_written/(double) m_peer.m_raw_calls_write, m_peer.m_raw_bytes_sent/(double) m_peer.m_raw_calls_write);
        fprintf(stderr, "   iovecs-queued/writev=%lf, iovecs-sent/writev=%lf\n", m_peer.m_raw_iovecs_queued/(double) m_peer.m_raw_calls_write, m_peer.m_raw_iovecs_sent/(double) m_peer.m_raw_calls_write);
    }

    bool is_flushed() override
    {
        m_peer.try_send_batch(true);
        return !m_peer.have_outgoing_data();
    }

    void progress() override
    {
        // We only try to send, we don't try to receive.
        // Incoming data will be building up in the incoming
        // file/whatever buffer
        m_peer.try_send_batch(true);
    }

    __attribute__ ((noinline)) EventFlags wait(EventFlags flags, double timeoutSecs) override
    {
        if(flags & EventFlags::IDLE){
            throw std::runtime_error("Idle not supported.");
        }

        assert( flags&EventFlags::TIMEOUT ? timeoutSecs >=0 : true );

        uint64_t start_time=now_ms();
        uint64_t end_time= (flags & EventFlags::TIMEOUT) ? start_time+(unsigned)floor(timeoutSecs*1000) : UINT64_MAX;

        bool do_flush=(flags & EventFlags::FLUSHED);

        m_peer.try_send_batch(!do_flush);
        m_peer.try_receive_add_data();

        uint64_t now_time=start_time;
        while(1){
            EventFlags res=(EventFlags)0;

            const int MIN_MESSAGES_IN_FREE_LIST=8;
            if( (flags & EventFlags::SEND) && m_manager.get_free_message_list().size()>=MIN_MESSAGES_IN_FREE_LIST){
                res = EventFlags(res | EventFlags::SEND);
            }

            if( (flags & EventFlags::RECV) && m_peer.have_receive_message()){
                res = EventFlags(res | EventFlags::RECV );
            }

            if( (flags & EventFlags::TIMEOUT) && (now_time >= end_time) ){
                res = EventFlags(res | EventFlags::TIMEOUT);
            }

            if( (flags & EventFlags::FLUSHED) && !m_peer.have_outgoing_data() ){
                res = EventFlags(res | EventFlags::FLUSHED);
            }


            if(res){
                return res;
            }

            pollfd pfd[1];
            pfd[0].fd=*m_peer.m_socket;
            pfd[0].events=POLLIN;
            pfd[0].revents=0;

            if(m_peer.have_outgoing_data()){
                pfd[0].events |= POLLOUT;
            }
            
            while(1){
                int wait_time=std::min(uint64_t(60*1000), end_time-now_time);
                int g=::poll(pfd, 1, wait_time);
                if(g<0){
                    if(EINTR){
                        continue;
                    }else{
                        throw std::runtime_error("Error during poll.");
                    }
                }

                if(pfd[0].revents & POLLOUT){
                    m_peer.try_send_batch(!do_flush);
                }
                if(pfd[0].revents & POLLIN){
                    m_peer.try_receive_add_data();
                }
                // Timeout also gets us here.
                break;
            }

            now_time = now_ms();
        }
    }

    __attribute__ ((noinline)) bool receive(const Endpoint::receive_callback_t &cb) override
    {
        return m_peer.receive(cb);
    }

    __attribute__ ((noinline)) void send(
        MessageList &&msgs
    ) override
    {
        m_peer.send(std::move(msgs));
    }
};

#endif
