
/*  The overall idea is that we want to capture the
    heat at particular space points S'=s_1..s_n, and
    at particular time points T'=t_1,t_2,... This
    is usually going to be a subset of the full T x S
    space (due to bandwidth constraints). So we
    have:
    
        S' \subset S
        T' \subset T
    
    giving:
    
        T' x S' \subset T x S.

    From the root device's point of view, it wants to capture
    a sequence of timeslices or views:

       V_i = { (t_i,s_j) : s_j \in S' )
       
    We want to move through the views sequentially, so
    capture V_1, V_2, V_3, ... The root device can then
    write them to an export format, display them as
    frames, encode them as video, or whatever.
    
    
    Each supervisor device manages a sub-set of the space
    points (devices), and will receive the heat points for
    exfiltration as messages. In order to manage bandwidth,
    only those devices which are part of the capture space
    should send, and they should only do it at the appropriate
    time points.
    
    Let's assume that each supervisor 1<=i<=m has a set S_i of
    the workers, where:
       
       S = \union_{i=1}^m S_i
       
    and
    
       \forall 1<=i<j<=m : \empty = S_i \intersection S_j
       
    Each supervisor can then work out which of its devices
    it will be receiving messages from:
    
        S_i' = S' \intersection S_i
        
    For each time index j the supervisor will create a partial
    view:
    
        V_{j,i}' = { (t_j,s_k) : s_j \in S_i' }
        
    As each supervisor completes a time-slice, they will send
    it to the root device, who can combine them into the final
    time slice:
    
        V_{j} = \union_{i=1}^m V_{j,i}'
        
    
    In this model the devices can run ahead, and the only things
    that stop them are back-pressure from each other, and back-pressure
    from the supervisor. Similarly, the supervisors can run ahead,
    and need to rely on back-pressure from the root node. The
    whole process forms a pipeline, which needs a certain amount
    of memory to operate efficiently, otherwise the devices end
    up constrained by the root node. However memory is limited
    on the supervisor nodes, and also on the root node - even
    though the root node can be big, it needs to manage input
    from m supervisors.
    
    For the device-supervisor buffer, we would like enough
    buffering that the slowest device in S_i' is not stopping
    the fastest device in S_i', or at least no more than is
    necessary for correct neighbour synchronisation. Let's
    assume that the supervisor device has enough storage
    for b copies of the view, so the storage required is O( b |S_i'| ).
    At any point in time, there will be some earliest
    view that is not yet complete, call it w (for working).
    So the buffer is occupied with views V_{i,w}..V_{i,w+b-1}.
    
    The supervisor starts with w=1, and cannot advance until
    it has received all data-points in V_{i,1}. Some
    devices may generate the points for this view, and then
    run on to generate points in the next view. Other devices
    may be slower, or their messages might be delayed. Even
    if all devices have produced the view, there may still
    be congestion in terms of the supervisor shipping things
    back to the root device. So we must have some way of
    stopping the devices running beyond the end of the view
    buffer.
    
    Assume that all devices in S' have a variable h, which
    represents the maximum view that they can advance to (this
    is getting a bit sloppy in terms of view sequence number
    and simulation time). If they reach the point when they
    want to exfiltrate for a time t>h, they must wait. The
    supervisor is then responsible for making sure that
    h <= w+b for all the devices in S_i'.
    
    If we choose b=1, there is only one buffer. This means
    that devices will run until they generate the exfiltration
    message for view w, and then can continue to run up until
    the point that they want to exfiltrate the next slice,
    at which point they wait.
    
    The supervisor can then wait until they have received
    everything in V_{i,w}', send it to the root device, and
    discard V_{i,w}'. At that point it can move to w=w+1,
    and broadcast the new h=w. At this point the devices
    will immediately send their messages, and the whole
    thing rattles on. There is actually a fair amount of
    potential concurrency even if b=1, as long as we are
    not exfiltrating every time step.
    
    The grid can also "shear" around the points
    in S', before blocking. So if we have a 2D
    grid and sub-sample S every fifth step, then
    we would have a grid that looks like:
    
    + . . . + . . . +
    . . . . . . . . . 
    . . . . . . . . .
    . . . . . . . . .
    + . . . + . . . +
    
    Where + is in S', . is not in S'.
    
    If we block exfiltration at time 1, then the
    grid will shear to something like:
    
    1 2 3 2 1 2 3 2 1
    2 3 4 3 2 3 4 3 2
    3 4 5 4 3 4 5 4 3
    2 3 4 3 2 3 4 3 2
    1 2 3 2 1 2 3 2 1
    
    before blocking for an updated h.
    
    As we increase b, we allow for more concurrency, which
    provides space for the transmission of the view to the
    root device, as well as some amount of delay and
    re-ordering in the network. However, it seems unlikely
    that we'd want a very high b, as eventually there will
    be blocking, and it's just using loads of RAM. So
    presumably something like b=4 would make sense:
    - 1 view being transmitted to root
    - 1 view complete or very near complete
    - 1 view which is actively filling up
    - 1 view to catch the advance devices
*/

struct gals_setup
{
  
};


class GALSHeatRootDevice
    : public RootDevice
{
    
};

class GALSHeatMotherDevice
    : public MotherDevice
{
private:
    std::unordered_map<device_address_t,unsigned> m_deviceAddressToSnapshotIndex;

    /*! Used to tell the root node what the binding between
        snapshot locations and device ids is. */
    struct snapshot_setup
    {
        
    };

    struct snapshot_instance
    {
        unsigned timestep;  // Which timestep this is associated with
        unsigned received;  // How many values have been received
        std::vector<float> value;
    };
protected:    
    
    void onAttachGraph(typed_data_ptr_t pGraphProperties) override
    {
    }

    void onAttachChildDevice(
        const char *instanceId,
        const char *graphType,
        device_address_t address,
        typed_data_ptr_t pDeviceProperties
    ) override    
    {
        if(
    }
        
public:


};

