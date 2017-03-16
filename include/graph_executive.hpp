#ifndef graph_executive_hpp
#define graph_executive_hpp

#include "graph_supervisor.hpp"

namespace POETS
{
      
    
    
/*! \page executive Executive Overview
    
    This is used by the executive that loaded the
    graph, so that it can examine and interact with it.
    
    The executive is whatever caused the graph to run,
    and is the source of input and output. Examples of
    executives could be:
    - A graph-independent command-line application that
      feeds output messages to a file.
    - An OpenCL application that translates mouse movements
      to messages injected into the graph, and converts
      exiltrated packets to displayed triangles.
    - A Gephi plugin that exports Gephi graphs as graph instances
      for analysis within POETS.
    - A matlab plugin that uses POETS to solve sparse equations.
    
    A key property of the executive is that it is in control,
    and is the thing that initiates the loading/execution
    of a graph, and is interested in the output. It may
    be an executable itself, or it may be a plugin DLL
    within an existing application, but from the user's
    point of view, it is the thing they interact with.
    The POETS backend engine is a black-box that sits
    behind the executive.
    
    A necessary assumption is that the executive understands
    the graph type that it is trying to load. Either it is
    specifically written for a particular graph type, or
    it will dynamically examine the graph type in order to
    work with the graph at run-time. Most "true" applications
    will need direct compiled-in knowledge of the graph type
    and the associated supervisor and device messages.
    Only debugging, visualisation, and automation tools are
    likely to examine the graph type at run-time.
    
    The executive interacts with the graph type through a
    graph independent API. This API allows it to load and
    instantiate a graph as a graph context. The graph context
    allows the executive to:
    - Discover the devices within the graph instance
    - Map device ids (application-level strings) to device addresses (physical locations)
    - Discover the supervisors within the graph
    - Map from device to the supervisor that manages that device and vice-versa
    - Establish and MPI link with the supervisors

    There is an assumption that the executive is running
    on a meaty machine with a lot of RAM, so it is feasible
    that it can contain a reflection of all the devices and
    all their properties. To make things easier, that information
    is managed as read-only data within the context. So the
    information available is:
    - All devices contained in the graph, including their ids, types, addresses, and properties
    - All supervisors in the graph, and which devices they manage
    
*/
class ExecutiveContext
{
public:
    /*! Describes a device within the graph. */
    struct device_info_type
    {
        const char *id;
        const char *deviceType;      //
        device_address_type address;    
        int supervisorRank;          // Used to determine who is managing that device
    
        const void *properties; 
    };
    
    /*! Contains information about a supervisor and the devices it manages */
    struct supervisor_info_type
    {
        unsigned index;                                 //! Index within the set of supervisors
        device_address_type address;                    //! Device address of the supervisor
        int rank;                                       //! MPI rank of the supervisor
        unsigned localDeviceCount;                      //! How many devices the supervisor manages
        const device_info_type *localDevicesById;       //! Local devices sorted by id
        const device_info_type *localDevicesByAddress;  //! Local devices sorted by address
    };    
    
public:
    virtual ~ExecutiveContext()
    {}
    
    //! Graph instance id that the supervisor is attached to
    const char *getGraphInstanceId() const=0;
    
    //! Graph instance type that the supervisor is attached to
    const char *getGraphTypeId() const=0;
    
    //! Number of supervisors that have been allocated
    unsigned getSupervisorCount() const=0;
    
    //! Returns info about a supervisor, and in particular the devices associated with a particular supervisor
    /*! \param index Should have 0<=index<getSupervisorCount() */
    const supervisor_info_type *getSupervisorInfoByIndex(unsigned index) const=0;
    
    //! Total number of devices that this graph contains
    unsigned getDeviceCount() const=0;
    
    //! Returns info about a particular device
    /*! \param index Should have 0<=index<getDeviceCount()
        \retval Returns null if the index is invalid
    */
    const device_info_type *getDeviceInfoByIndex(unsigned index) const=0;
    
    //! Returns info about a particular device by address
    /*! \retval Returns null if the address does not exist
    */
    const device_info_type *getDeviceInfoByAddress(device_address_type address) const=0;
    
    //! Returns info about a particular device by id
    /*! \retval Returns null if the id does not exist
    */
    const device_info_type *getDeviceInfoById(const char *id) const=0;
    
    /*! Log information back to a central logging area in some way. */
    virtual void log(int level, const char *msg, ...) =0;
    
    /*! Get the MPI communicator for the supervisors (rank>0) and executive (rank==0).
        The communicator size should be `getSupervisorCount()+1`.
    */
    virtual int getMPICommunicator() const=0;
    
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
    
    virtual void sendMessageToSupervisor(
        int supervisorIndex,    //! >=0 for a specific supervisor, -1 for broadcast
        unsigned messageSize,
        const void *messageData
    ) =0;
    
    virtual bool recieveMessageFromSupervisor(
        int supervisorIndex,  //! >=0 for a specific supervisor, -1 for any
        unsigned maxMessageSize,
        unsigned &messageSize,
        void *messageData
    ) =0;
    
    /*! All supervisors and the executive have to enter the barrier
        before any can leave. */
    virtual void barrier();
};

class ExecutiveEvents
{
protected:
    SupervisorDevice()
    {}
public:
    //! Called when one of the supervisors uses SupervisorContext::log.
    void onSupervisorLog(int supervisor, int logLevel, const char *msg);

    /*! Notify the supervisor that a graph instance is being attached

        When this event occurs it is not necessarily the case that
        any other supervisors or the executive are up and running yet.        
        
        \pre No graph is currently attached, or being attached.
    */
    virtual void onAttachGraph(
        ExecutiveContext *context     //! The context for this supervisor, which will remain fixed for entire graph
    )
    {}
    
    //! All supervisors are up, and ready to talk to the executive
    virtual void onAttachSupervisors(
        ExecutiveContext *context
    )
    {}

};

/*! Connects to a POETS provider, and loads the given graph into it.
    When the function returns, the graph will be loaded and the devices
    and supervisors will be running. The executive can then interact
    with the context to send messages to supervisors and receive
    message back.
*/
ExecutiveContext *executive_load_graph_from_file(
    const char *platformBinding,    //! A string identifying how to connect to the POETS provider
    const char *sourceFile,         //! Location of the graph instance file
    ExecutiveEvents *events         //! Callback for events that occur during loading and execution of graph
);

}; // POETS

#endif
