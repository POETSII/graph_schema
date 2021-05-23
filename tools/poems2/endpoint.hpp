#ifndef endpoint_hpp
#define endpoint_hpp

#include "message.hpp"

#include <functional>

enum EventFlags : unsigned{
    SEND = 1,       // It is possible to get a new message buffer
    RECV = 2,       // There is at least one message to send
    IDLE = 4,       // Entire system has hit a barrier
    FLUSHED = 8,    // All previously sent messages are in flight
    TIMEOUT = 16   
};

EventFlags operator|(EventFlags a, EventFlags b)
{
    return EventFlags((unsigned)a|(unsigned)b);
}

/*
    This interface is designed to try and avoid copying messages as much
    as possible, and enable batching of messages where feasible. 
*/
class Endpoint
{
protected:
    endpoint_address_t m_self = -1;

    // The implementation must set this to non-null
    MessageList *m_send_buffer_list = 0;

    Endpoint()
    {}

    Endpoint(Endpoint &&o)
    {
        std::swap(m_self, o.m_self);
        m_send_buffer_list=0;
    }
public:
    virtual ~Endpoint()
    {}

    endpoint_address_t self_address() const
    {
        assert(m_self!=-1);
        return m_self;
    }

    // Return true if all previously sent messages are in-flight (though not nesc.received)
    virtual bool is_flushed() =0;

    /* Wait until at least one of the conditions is true.
        The flags mask must not be empty.
        Will return at least one event which was met.
    */
    virtual EventFlags wait(EventFlags flags, double timeoutNs=0) =0;

    /* Try to make background progress.
        The client can call this occasionally if they are doing lots of
        other stuff that does not interact with the endpoint. Should be
        cheap if there is nothing to do.
    */
    virtual void progress()
    {}

    /*  Gets a list of buffers used to send messages. The list may be empty
        if there is no send capacity.
        Clients should pop off the back in order to get the most recently#
        added message buffers.    
    */
    MessageList &get_send_buffer_list()
    {
        return *m_send_buffer_list;
    }

    /*  Get a single send buffer. The list may be empty
        if there is no send capacity. */
    Message *get_send_buffer()
    {
        return m_send_buffer_list->pop_back_or_null();
    }

    /* Append the given messages to the send queue.
        Messages can always be sent, with throttling on the ability to get
        buffers.
    */
    virtual void send(MessageList &&list) =0;

    using receive_callback_t = std::function<bool(const message_t &msg)>;

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


#endif