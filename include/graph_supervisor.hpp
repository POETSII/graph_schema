#ifndef graph_supervisor_hpp
#define graph_supervisor_hpp

#pragma push()
#pragma pack(1)
struct my_properties{
  uint8_t x;
};
#pragma pop()



namespace POETS
{
    
/*! \page supervisor Supervisor Overview

    
*/    
    

/*! Identifies a port within a device type.
    
    A device index is relative to a particular device
    type, so multiple device types may have a port with
    the same index.
    */
typedef uint16_t port_index_type;

/*! Identifies a specific device (normal or manager) within the entire graph,
    providing a global numeric address.
*/
typedef uint64_t device_address_type;

/*! An combination of a device address and a port index. This
    is equivalent to an IP address + port. */
struct endpoint_type
{
private:
    uint64_t bits;
public:
    endpoint_type(device_type _address, port_index_type _port)
        : bits( (_address<<16) | _port )
    {}
    
    device_address_type getAddress() const
    { return address>>16; }
    
    port_index_type getPort() const
    { return port&0xFFFF; }
};

class edge_info_type
{
    port_index_type port;
    const endpoint_type *endpoints;
};

class device_info_type
{
    const char *id;
    const char *deviceType;
    device_address_type address;
    int supervisorRank;     // Used to determine who is managing that device
    
    /*! Available for local (supervised) devices if "requiresLocalDeviceProperties"=1
        Available for global devices if "requiresGlobalDeviceProperties"=1 
    */
    const void *properties; 
    
    /*! Available for local devices if "requiresLocalEdgeEndpoints"=1
        Available for global devices if "requiresGlobalEdgeEndpoints"=1
    */
    const edge_info_type *edges
};


class SupervisorContext
{
    //! Graph instance id that the supervisor is attached to
    const char *getGraphInstanceId() const=0;
    
    //! Graph instance type that the supervisor is attached to
    const char *getGraphTypeId() const=0;
    
    //! Total number of devices that this supervisor is managing
    unsigned getLocalDeviceCount() const=0;
    
    //! Returns info about a particular device
    /*! The user must have requested "requiresLocalDeviceProperties"=1 and "requiresPersistentLocalDeviceInfo"=1
        \param index Should have 0<=index<getLocalDeviceCount()
    */
    const device_info_type *getLocalDeviceInfoByIndex(unsigned index) const=0;
    
    //! Returns info about a particular local device by address
    /*! The user must have requested "requiresLocalDeviceProperties"=1 and "requiresPersistentLocalDeviceInfo"=1
        \param index Should have 0<=index<getLocalDeviceCount()
        \retval Return nullptr if the address is not local
    */
    const device_info_type *getLocalDeviceInfoByAddress(device_address_type address) const=0;
    
    /*! Log information back to a central logging area in some way. */
    virtual void log(int level, const char *msg, ...) =0;
    
    /*! Get the MPI communicator for the supervisors (rank>0) and executive (rank==0).
        The number of supervisors is the communicators size, minus one for the executive.
    */
    virtual int getMPICommunicator() const=0;
    
    /*! Returns the device address of this supervisor device. */
    virtual device_address_type getSelfAddress() const=0;
    
    /*! Translate a deviceType+portName pair to an index. This
        will be constant for the entire execution.
        
        \param deviceType The named device type for a standard device, or null for the supervisor
        
        \code
        class MySupervisor : public SupervisorDevice
        {
        private:
            device_port_index m_devA_exfilRequestIndex;
            device_port_index m_exifilRecvIndex;
        
        public:
            void run(SupervisorContext *context) override
            {
                // Get name of a port called "send" on device type "devA"
                m_devA_exfilSendIndex=context->mapPortNameToIndex("devA", "send");
                m_exfilRecvIndex=context->mapPortNameToIndex("", "send");
                
                ...
            }
        }
        \endcode
    */
    virtual port_index_type mapPortNameToIndex(
        const char *deviceType,
        const char *portName
    );
    
    /*! This is able to map any id within the graph to
        a global address. It is expected to be faster for
        local devices, but should still complete
        for remote devices (and supervisors). This is more
        for initial setup, and less likely to be used while
        in the steady state.
    */
    virtual void mapDeviceIdToAddress(
        unsigned numDevices,
        const char **pIds,
        device_address_type *pAddresses
    ) const =0;
    

    /*! Send a single message to one or more endpoints.
        
        The message can be routed in either an ad-hoc form, where the 
        endpoints are explicitly given by the function call, or a routed
        form, where the endpoints are given by the graph
        topology. It is feasible that we might want to make an ad-hoc call
        but have zero endpoints, so the routed form is selected by choosing
        a null endpoint list.
        
        Sending to all devices connected to the "tick" port:
        \code
        MYGRAPH_TICK_T message;
        message.payload=4;
        
        context->sendDeviceMessage(
            context->mapPortNameToIndex(nullptr, "tick"),
            0, nullptr, //! indicate we use the graph topology
            sizeof(message), &message
        );
        \endcode
        
        Sending to two specific ports:
        \code
        MYGRAPH_TICK_T message;
        message.payload=4;
        
        endpoint_type destEndpoints[2]={
            context->mapDeviceIdToEndpoint("device22", "tock"), 
            context->mapDeviceIdToEndpoint("deviceZ", "notify")
        };
        
        context->sendDeviceMessage(
            context->mapPortNameToIndex(nullptr, "tick"),
            2, destEndpoints,
            sizeof(message), &message
        );
        \endcode
    */
    virtual void sendDeviceMessage(
        port_index_type sourcePort,
        unsigned numDestEndpoints,
        const endpoint_type *pDestEndpoints,
        unsigned messageSize,
        const void *messageData
    ) =0;
    
    /*! Receive messages if any are available. */
    virtual bool receiveDeviceMessage(
        endpoint_type &source,              //! Address and source the message came from
        port_index_type &destPort,          //! Which port on this supervisor it was targetted at
        unsigned maxMessageSize,            //! The size of the buffer, which should match or exceed the largest message in the system
        void *messageData,                  //! The caller-allocated message buffer (of size >= maxMessageSize)
        unsigned &messageSize               //! Actual received message size (which may be zero)
    ) =0;
    
    virtual void sendMessageToSupervisor(
        int supervisorIndex,    //! >=0 for specific supervisor, -1 for all
        unsigned messageSize,
        void *messageData
    ) =0;
    
    virtual bool receiveMessagefromSupervisor(
        int supervisorIndex,    //! >=0 for specific supervisor, -1 for any
        unsigned messageSize,
        void *messageData,
        int &sourceSupervisor   //! Which supervisor it came from
    ) =0;

    virtual void sendMessageToExecutive(
        unsigned messageSize,
        void *messageData
    ) =0;

    virtual bool receiveMessageFromExecutive(
        unsigned maxMessageSize,
        void *messageData,
        unsigned &messageSize,
    ) =0;
        
    /*! All supervisors and the executive have to enter the barrier
        before any can leave. */
    virtual void barrier();
};

/*!
    A supervisor device goes through a fixed lifecyle whenever
    it is attached to a graph:

    1 - onBeginAttachGraph : The supervisor is first given the context, which tells it
        about the graph it is being attached to, as well as how many devices it will
        be managing.

    2 - onAttachDevice : Informs the supervisor about one device instance, and the properties,
        id, and address of that instance. This will be called once for each of the attached
        devices.

    3 - onAttachExecutive : The executive is ready to talk to you over MPI (if desired)

    4 - onAttachSupervisors : All supervisors are up and running, and all have completed
        onAttachExecutive. At this point you can talk to other supervisors over MPI.

    5 - onCompleteAttachGraph : All supervisors are attached, all devices are running. At
        this point the supervisor should enter its steady state, communicating with devices,
        the executive, and other supervisors as necessary.
*/
class SupervisorDevice
{
protected:
    SupervisorDevice()
    {}
public:
    /*! Notify the supervisor that a graph instance is being attached

        When this event occurs it is not necessarily the case that
        any other supervisors or the executive are up and running.
        There should be no MPI or messaging calls, only the setting
        up of data-structures based on the graph properties and number of devices.
        
        \pre No graph is currently attached, or being attached.
    */
    virtual void onBeginAttachGraph(
        SupervisorContext *context     //! The context for this supervisor, which will remain fixed for entire graph
    );
        
    /*! Notify the supervisor that it will be managing this device,
        and giving the address and properties.

        The structure passed into this function may be persistent or
        ephemeral. By default it is ephemeral, so nothing in "info"
        can be re-used outside the function. However, if the supervisor
        sets "requiresPersistentDeviceInfo"=1, then the structure (and
        all members) are guaranteed to be fixed for the lifetime of
        the supervisor device. Persistent structures can be cached,
        but should not be modified.
        
        There is still no guarantee about the state of other supervisors
        or the executive.
    */
    virtual void onAttachDevice(
        SupervisorContext *context,
        const device_info_type *info
    )
    {}
    
    //! The executive is up, and ready to talk over MPI
    virtual void onAttachExecutive(
        SupervisorContext *context
    )
    {}
    
    //! All supervisors are up, and ready to talk over MPI
    virtual void onAttachSupervisors(
        SupervisorContext *context
    )
    {}
    
    /*! All software is attached, the hardware is ready to run.
    
        When this function is run, the supervisor should do whatever
        it is that happens when the graph runs. It should assume that
        all devices and other supervisors are also running.
    */
    virtual void onCompleteAttachGraph(
        SupervisorContext *context
    ) =0;
};

}; // POETS

#endif
