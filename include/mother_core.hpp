
//! Identifies a port on a device (application or mother)
typedef uint8_t port_index_t;

//! Identifies just a device
typedef uint64_t device_address_t;

//! Identifies a specific point on a device
typedef uint64_t endpoint_address_t;


/*! Represents a message send task, which can be
    broadcast to an atritrary number of target devices.
*/
struct send_buffer_t
{
    endpoint_address_t      source;         // This is a port on this mother-core 
    unsigned                nDestinations;  // Number of places to deliver to
    endpoint_address_t *    pDestinations;  // Caller allocated list of addresses (caller retains ownership)
    typed_data_ptr_t        pPayload;       // The caller should add a reference count on entry
};

/*! Represents a single message being received. */
struct receive_buffer_t
{
    endpoint_address_t      source;     // This is a port on some device.
    endpoint_address_t      destination;// This will be a port on this mother-core
    typed_data_ptr_t        pPayload;   // Should be null on entry. Caller has a reference on exit
};



class MotherDeviceContext
{
    //! Gets the MPI group containing the mother devices and the root node      
    MPI_Comm getMotherGroup();
    
    //! Get MPI rank of self within the mother group
    /*! If this is rank 0 then this device is the root (head) node, and
        is not directly connected to the P-Cores. Non-zero rank implies
        it is one of the ARM cores.*/
    int getMotherRankOfSelf(); 
    
    
    
    //! Get the address of self in device world.
    /*! This is the address that any devices owned by this core will send to
        when they send to their mother-device. In principle other non-owned
        devices could also send to the mother-device
    */
    device_address_t getAddressOfSelf();

    //! Get endpoint address of a particular port on self
    endpoint_address_t getEndpointOfSelf(const char *name);
    
    device_addr_t deviceInstanceIdToAddr(const char *device);
    
    port_addr_t deviceInstanceIdToEndpoint(const char *device, const char *name);
    
    int getRankOfDeviceInstanceId(device_addr_t instance);
    
    port_id_t portNameToId(const char *name);
    
    //! Send one or more messages to clients
    /*! When the function call completes, either the
        messages have been sent or they have been
        buffered somewhere else.
        The caller gives the implementation 1 reference count
        on the messages.
    */
    void sendMessages(
        unsigned nBuffers,
        send_buffer_t *pBuffers
    );
    
    void sendMessage(endpoint_address_t src, endpoint_address_t dest, typed_data_ptr_t payload)
    {
        send_buffer_t buffer{
            source=src,
            nDestinations=1,
            pDestinations=&dest,
            pPayload=payload
        };
        sendMessages(1, &buffer);
    }
    
    //! Receive up to nMaxMessages from devices
    /*! On input the buffers should be zeroed. On output the
        buffers receiving data will get a pointer to the data
        and 1 reference on that data. If the reference count is
        1, then the data can be mutated. If the reference count
        is >1, then the data may be shared, and so should be
        considered read-only.
    */
    unsigned receiveMessages(
        bool block, //! Wait until at least one messages is available
        unsigned nMaxMessages,
        recieve_buffer_t *pBuffers
    );
};
