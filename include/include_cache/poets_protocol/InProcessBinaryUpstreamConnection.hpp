#ifndef simple_in_proc_connection_hpp
#define simple_in_proc_connection_hpp

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <climits>
#include <cassert>
#include <unordered_map>

#include "types.hpp"

/*! This is an API for hosting external logic directly within
    another process. It exposes a simplified API whereby source
    routing is handled externally, and incoming messages are
    routed directly to to each relevant external device.

    It does not expose certain high-performance features such
    as batching, and does not allow for direct access to source
    routed incoming messages. It is also very limited in terms
    of access to the wider graph, such as walking edges and
    so on. It is much closer to just implementing standard
    devices in terms of API.

    This interface is very fragile, as to remove inter-module dependencies
    it is sometimes convenient to "cache" it in external modules. If there
    are ever two copies of the interface that differ then it is disastrous,
    and extremely difficult to debug. As a consequence we go
    to some efforts to protect against it via all of this
    INTERFACE_HASH_* nonsense. The intent is that the results hash
    gets compiled into both shared modules and into any clients,
    so they can check whether there is any discrepancy.

    This hash process is not fool-proof - anyone modifiying the master
    interface defn. should always bump INTERFACE_HASH_VAL_BASE.
 */
class InProcessBinaryUpstreamConnection
{
public:
    static constexpr size_t hash_combine(size_t a, size_t b)
    {   
        return a*6521908912666391051ull+b;
    };

    /* NOTE: This should be bumped by at least 1 whenever the API
        changes in some way.
    */
    const size_t INTERFACE_HASH_VAL_BASE=4;

    const size_t INTERFACE_HASH_VAL_0 = hash_combine(INTERFACE_HASH_VAL_BASE, __LINE__);

    /* NOTE: The receiver of this object does not control the lifetime of this object, and should
    never call delete on it.
    Servers may wish to protect against un-expected deletion by clients.
    */
    virtual ~InProcessBinaryUpstreamConnection()
    {}

    const int INTERFACE_UNIQUIFIER_PROTECT1=__LINE__;

    /*! The device_type in owned can either be:
        - empty - in which case it matches anything
        - a regex - in which case it matches any device type with that regex
        - an id - Will match only that device type
    */
    virtual void connect(
        const std::string &graph_type,      //! Regex for matching graph type, or empty
        const std::string &graph_instance,  //! Regex for matching graph instance, or empty
        const std::vector<std::pair<std::string,std::string>> &owned    //! Vector of (device_id,device_type) pairs
    ) =0;

    const size_t INTERFACE_HASH_VAL_1=hash_combine(INTERFACE_HASH_VAL_0, __LINE__);

    //! Returns the overall graph properties
    /*! \retval The size of the structure that was written.
        \pre cbBuffer>=sizeof(GRAPH_PROPERTIES_T)
    */
    virtual size_t get_graph_properties(size_t cbBuffer, void *pBuffer)=0;

    const size_t INTERFACE_HASH_VAL_1a=hash_combine(INTERFACE_HASH_VAL_1, __LINE__);

    //! Returns the properties of an external
    /*! \retval The size of the structure that was written.
        \pre The address must be the address of an external
        \pre cbBuffer>=sizeof(DEVICE_PROPERTIES_T)
    */
    virtual size_t get_device_properties(poets_device_address_t address, size_t cbBuffer, void *pBuffer)=0;

    //! Map a device instance id to an address. Throws if it is not known
    virtual poets_device_address_t get_device_address(const std::string &id)=0;

    const size_t INTERFACE_HASH_VAL_2=hash_combine(INTERFACE_HASH_VAL_1a, __LINE__);

    //! Map a device address to an instance id. Throws if it is not known
    virtual std::string get_device_id(poets_device_address_t address)=0;

    const size_t INTERFACE_HASH_VAL_2a=hash_combine(INTERFACE_HASH_VAL_2, __LINE__);

    //! Maps a (device,output) endpoint pair to a list of external addresses that should receive on it
    /*! If the source endpoint is not an indexed send then this list should only contain external endpoints,
        and there are no requirements on ordering.

        If the source endpoint is an indexed send, then the function must return a complete set of addresses
        in order, and so must mark internals as invalid. The ordering must match either the explicit order
        from the source graph instance, or match the implicit ordering chosen by the server.

        If the endpoint is unknown, then throw.

        If the endpoint has no external connections, then just return an empty vector.
    */
    virtual void get_endpoint_destinations(poets_endpoint_address_t source, std::vector<poets_endpoint_address_t> &destinations)=0;

    const size_t INTERFACE_HASH_VAL_3=hash_combine(INTERFACE_HASH_VAL_2a, __LINE__);

    //! Return true if we can send without blocking
    virtual bool can_send()=0;

    const size_t INTERFACE_HASH_VAL_4=hash_combine(INTERFACE_HASH_VAL_3, __LINE__);

    //! This sends a single source routed message
    /*! This method will never block. If can_send is true, then it is guaranteed message will be sent.
        
        \param sendIndex This is only relevant for indexed pins.
               For normal pins it should be UINT_MAX.

        \retval True if the message was sent, false if we would have blocked.
     */
    virtual bool send(
        poets_endpoint_address_t source,
        const std::shared_ptr<std::vector<uint8_t>> &payload,
        unsigned sendIndex = UINT_MAX
    ) =0;

    const size_t INTERFACE_HASH_VAL_5=hash_combine(INTERFACE_HASH_VAL_4, __LINE__);

    //! Ensure that any buffered messages have left internal batching buffers.
    /*! This should not be a blocking message in the normal sense,
        as the use of can_send and credit control should mean that
        flushing is more about moving things into the outgoing queue,
        rather than waiting until there is space. 
     */
    virtual void flush()=0;

    const size_t INTERFACE_HASH_VAL_6=hash_combine(INTERFACE_HASH_VAL_5, __LINE__);

    //! Return true if we can receive without blocking
    virtual bool can_recv()=0;

    const size_t INTERFACE_HASH_VAL_7=hash_combine(INTERFACE_HASH_VAL_6, __LINE__);

    /*! This API will produce individual source routed message.
        This method will never block. If can_recv was true, then it will always return a message.

        \retval If true then a message was received. If false, then no message was ready.
    */
    virtual bool recv(
        poets_endpoint_address_t &source,
        std::shared_ptr<std::vector<uint8_t>> &payload,
        unsigned &sendIndex
    ) =0;

    const size_t INTERFACE_HASH_VAL_8=hash_combine(INTERFACE_HASH_VAL_7, __LINE__);

    //! Grabs the termination information and finishes the connection
    /*! \retval Either returns the terminate message or NULL if termination has
       not happened yet. 
       
       Once the termination has been captured the connection is done,
       and you shouldn't attempt to send or receive messages.
    */ 
    virtual const halt_message_type *get_terminate_message() =0;

    const size_t INTERFACE_HASH_VAL_10=hash_combine(INTERFACE_HASH_VAL_7, __LINE__);

    enum Events{
        CAN_RECV = 0x1,   // There is at least one incoming message to receive
        CAN_SEND = 0x2,   // There is capacity to send at least one message
        TERMINATED = 0x4, // The terminate event is ready
        TIMEOUT  = 0x8    // A timeout was passed into wait_until, and it expired before any other event
    };

    //! Wait until you can either send, receive, or until terminate or a timeout happens.
    /*! 
        \param eventSet A set of events that the client wants to wait until. Any combination of valid bits
                        is valid, including the empty set (which will trivially just return without blocking).

        \param timeoutMacroSeconds Specifies the maximum time to block in this call if no other event becomes ready.
                The TIMEOUT event must be present in eventSet for this to take effect.
                A timeout of 0 is an infinite timeout, and is a valid timeout even if TIMEOUT is present in the set.
    
        \retval A set of events that are currently ready. It will always be the case that (eventSet!=0) -> (eventSet&retVal!=0),
            so as long as you're actually waiting for at least one event it will be guaranteed that at least one
            is true on return. However, the implementation may choose to return a larger set of events, so it could include
            events not present in eventSet.

        A blocking wait will flush messages. However, if there is no need to wait
        then flushing may or may not happen. 
    */
    virtual Events wait_until(
        Events eventSet,
        uint64_t timeoutMicroSeconds = 0
    )=0;

    const size_t INTERFACE_HASH_VAL_11=hash_combine(INTERFACE_HASH_VAL_10, __LINE__);

    /*! Allows externals to report messages. This _may_ go through the same machinery
        as handler_log (depend on the protocol level and transport), but it might also
        turn up locally. It is implementation defined where these go.
     */
    virtual void external_log(
        int level,
        const char *msg,
        ...
    )=0;

    const size_t INTERFACE_HASH_VAL_FINAL=hash_combine(INTERFACE_HASH_VAL_11, __LINE__);

    /* By making this virtual we are guaranteed the optimiser can't see
        through this, as the pointer is coming from a shared object.
        TODO: Beware -fwhole-program...?
    */
    virtual size_t get_interface_hash_value() const
    { return INTERFACE_HASH_VAL_FINAL; }

    /////////////////////////////////////////////////
    // Non-virtual helper functions

    void check_interface_hash_value() const
    {
        auto got=get_interface_hash_value();
        auto exp=INTERFACE_HASH_VAL_FINAL;
        if(exp!=got){
            throw std::runtime_error("Hash of InProcessBinaryUpstreamConnection interface does not match. Do you have a shared object using a different version of the header?");
        }
    }
};

using in_proc_external_main_t = void (*)(
    InProcessBinaryUpstreamConnection &services,
    int argc,
    const char *argv[]
);

#endif
