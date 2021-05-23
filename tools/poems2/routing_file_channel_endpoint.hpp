#ifndef routing_file_channel_endpoint_hpp
#define routing_file_channel_endpoint_hpp

#include "endpoint.hpp"

#include <unistd.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <cmath>
#include <poll.h>

#include "file_channel_endpoint.hpp"

class RoutingFileChannelEndpoint
    : public Endpoint
{
private:
    static const int RECV_READ_SIZE  = 4096;
    static const int SEND_WRITE_SIZE = 4096;

    MessageManager m_manager;

    std::vector<FileChannelPeer> m_peers;

    std::vector<struct pollfd> m_poll_fds;

    uint64_t now_ms()
    {
        timespec ts;
        if(0!=clock_gettime(CLOCK_MONOTONIC, &ts)){
            throw std::runtime_error("Broken clock");
        }
        return ts.tv_sec+(ts.tv_nsec/1000000);
    }
public:
    /*  Recieves an array of endpoints.
        Endpoint addresses must be contiguous in [0,num_peers)
        The socket for self should be -1
    */
    RoutingFileChannelEndpoint(endpoint_address_t self, int num_peers, int *fds)
        : m_manager(1024) // TODO: how to choose this? Remember there is buffering in OS too...
    {
        m_self=self;

        m_peers.reserve(num_peers);

        for(int i=0; i<num_peers; i++){
            if(i==self){
                assert(fds[i]==-1);
                m_peers.emplace_back( i );
            }else{
                assert(fds[i]!=-1);
                m_peers.emplace_back( fds[i], i );
            }
            m_poll_fds.push_back({fds[i], POLLIN, 0});
        }

        m_send_buffer_list = &m_manager.get_free_message_list();
    }

    RoutingFileChannelEndpoint(RoutingFileChannelEndpoint &&o)
        : Endpoint(std::move(o))
        , m_manager(std::move(o.m_manager))
    {
        std::swap(m_peers, o.m_peers);
        std::swap(m_poll_fds, o.m_poll_fds);
        m_send_buffer_list=&m_manager.get_free_message_list();
        
        assert(m_manager.total_messages()>0);
    }


    ~RoutingFileChannelEndpoint()
    {
    }

    bool is_flushed() override
    {
        for(const auto &p : m_peers){
            if(p.have_outgoing_data()){
                return false;
            }
        }
        return true;
    }

    void progress() override
    {
        // We only try to send, we don't try to receive.
        // Incoming data will be building up in the incoming
        // file/whatever buffer
        for(unsigned i=0; i<m_peers.size(); i++){
            if(i!=m_self){
                m_peers[i].try_send_batch(false);
            }
        }
    }

    EventFlags wait(EventFlags flags, double timeoutSecs) override
    {
        if(flags & EventFlags::IDLE){
            throw std::runtime_error("Idle not supported.");
        }
        
        assert(m_send_buffer_list==&m_manager.get_free_message_list());

        assert( flags&EventFlags::TIMEOUT ? timeoutSecs >=0 : true );

        uint64_t start_time=now_ms();
        uint64_t end_time= (flags & EventFlags::TIMEOUT) ? start_time+(unsigned)floor(timeoutSecs*1000) : UINT64_MAX;

        bool do_flush=flags&FLUSHED;

        bool any_recv=false;
        bool all_flushed=true;
        for(auto &p : m_peers){
            if(m_self==p.m_peer){
                continue;
            }
            assert(-1!=*p.m_socket);
            p.try_send_batch(true);
            p.try_receive_add_data();
            any_recv |= p.have_receive_message();
            all_flushed &= !p.have_outgoing_data();
        }

        uint64_t now_time=start_time;
        while(1){
            EventFlags res=(EventFlags)0;


            const int MIN_MESSAGES_IN_FREE_LIST=8;
            if( (flags & EventFlags::SEND) && m_manager.get_free_message_list().size()>=MIN_MESSAGES_IN_FREE_LIST){
                res = EventFlags(res | EventFlags::SEND);
            }

            if( (flags & EventFlags::RECV) && any_recv){
                res = EventFlags(res | EventFlags::RECV );
            }

            if( (flags & EventFlags::TIMEOUT) && (now_time >= end_time) ){
                res = EventFlags(res | EventFlags::TIMEOUT);
            }

            if( (flags & EventFlags::FLUSHED) && all_flushed ){
                res = EventFlags(res | EventFlags::FLUSHED);
            }

            if(res){
                return res;
            }

            for(unsigned i=0; i<m_poll_fds.size(); i++){
                m_poll_fds[i].events=POLLIN;
                m_poll_fds[i].revents=0;
                if(m_peers[i].have_outgoing_data()){
                    m_poll_fds[i].events |= POLLOUT;
                }
            }
            
            while(1){
                int wait_time=std::min(uint64_t(5*1000), end_time-now_time);
                int g=::poll(&m_poll_fds[0], m_poll_fds.size(), wait_time);
                if(g<0){
                    if(EINTR){
                        continue;
                    }else{  
                        throw std::runtime_error("Error during poll.");
                    }
                }

                any_recv=false;
                all_flushed=true;
                for(unsigned i=0; i<m_peers.size(); i++){
                    if(i!=m_self){
                        if(m_poll_fds[i].revents & POLLOUT){
                            m_peers[i].try_send_batch(false);
                        }
                        if(m_poll_fds[i].revents & POLLIN){
                            m_peers[i].try_receive_add_data();
                        }
                        // Could be tracked more efficiently
                        any_recv |= m_peers[i].have_receive_message();
                        all_flushed &= !m_peers[i].have_outgoing_data();
                    }
                }

                // Timeout also gets us here.
                break;
            }

            now_time = now_ms();
        }
    }

    bool receive(const Endpoint::receive_callback_t &cb) override
    {
        bool did_recv=false;
        for(auto &p : m_peers){
            if(p.m_peer!=m_self){
                // Internally receive will try to send full batches too
                did_recv |= p.receive(cb);  
            }
        }
        return did_recv;
    }

    void send(
        MessageList &&msgs
    ) override
    {
        while(!msgs.empty()){
            Message *m=msgs.pop_front();
            assert(m->address_count > 0); // Cannot send a message with no destinations

            // Add a ref for any extra beyond 1 
            m->add_ref(m->address_count-1);
            for(unsigned i=0; i<m->address_count; i++){
                endpoint_address_t peer=m->address_list[i];
                assert(peer!=m_self);
                assert(peer < m_peers.size());
                assert(-1 != *m_peers[peer].m_socket);
                m_peers[peer].send(m);
            }
        }
    }
};

#endif
