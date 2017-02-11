
typedef void (*receive_handler_func)(
    const void *graphType,
    const void *deviceProperties,
    void *deviceState,
    const void *edgeProperties,
    void *edgeState,
    const void *message
);

typedef bool (*send_handler_func)(
    const void *graphType,
    const void *deviceProperties,
    void *deviceState,
    void *message
);

typedef void (*compute_handler_func)(
    const void *graphType,
    const void *deviceProperties,
    void *deviceState
);

typedef bool (*ready_to_send_func)(
    const void *graphType,
    const void *deviceProperties,
    const void *deviceState,
);

typedef bool (*ready_to_compute_func)(
    const void *graphType,
    const void *deviceProperties,
    const void *deviceState,
);

struct input_port
{
    const char *name;
    unsigned edgePropertiesSize;
    unsigned edgeStateSize;
    unsigned messageSize;
    receive_handler_func receive_handler;
};

struct output_port
{
    const char *name;
    unsigned messageSize;
    send_handler_func send_handler;
    ready_to_send_func ready_to_send;
};

struct device_type
{
    const char *name;
    unsigned inputCount;
    ready_to_compute_func ready_to_compute;
    const input_port *inputPorts;
    unsigned outputCount;
    const output_port *outputPorts;
};

struct graph_type
{
    const char *name;
    unsigned propertiesSize;
    unsigned deviceTypeCount;
    const device_type *deviceTypes;
};


struct device_instance
{
    const char *id;
    const device_type *type;
    const void *deviceProperties;
};

struct edge_instance
{
    const device_instance *srcDevice;
    const input_port *srcPort;
    const device_instance *dstDevice;
    const input_port *dstPort;
    const void *edgeProperties;
};

struct graph_instance
{
    unsigned deviceCount;
    const device_instance *devices;
    unsigned edgeCount;
    const edge_instance *edges;
};



template<class DeviceType, class InputPort>
extern "C" void generic_on_receive(
    const void *graphProperties,
    const void *deviceProperties,
    void *deviceState,
    const void *edgeProperties,
    void *edgeState,
    const void *message
){
    InputPort::on_receive(
        (const typename DeviceType::graph_properties_t *)graphProperties,
        (const typename DeviceType::device_properties_t *)deviceProperties,
        (typename DeviceType::device_state_t *)deviceState,
        (const typename InputPort::edge_properties_t *)edgeProperties,
        (typename InputPort::edge_state_t *)edgeState,
        (const typename InputPort::message_t*)message
    );
}

template<class DeviceType, class OutputPort>
extern "C" bool generic_on_send(
    const void *graphProperties,
    const void *deviceProperties,
    void *deviceState,
    void *message
){
    InputPort::on_receive(
        (const typename DeviceType::graph_properties_t *)graphProperties,
        (const typename DeviceType::device_properties_t *)deviceProperties,
        (typename DeviceType::device_state_t *)deviceState,
        (typename InputPort::message_t*)message
    );
}

template<class DeviceType, class OutputPort>
extern "C" bool dispatch_ready_to_send(
    const void *graphProperties,
    const void *deviceProperties,
    const void *deviceState
){
    InputPort::on_receive(
        (const typename DeviceType::graph_properties_t *)graphProperties,
        (const typename DeviceType::device_properties_t *)deviceProperties,
        (const typename DeviceType::device_state_t *)deviceState
    );
}




struct forcing_segment_message_t
{
    static const char *name="forcing_segment";
    
    fixed_t vStart;     // Start with value vStart...
    fixed_t vStep;      // ...then increase by vStep each step 
    unsigned tBegin;    // This message applies over [tBegin,tEnd)
    unsigned tEnd;
};

struct boundary_cell
{  
    static const char *name="boundary_cell";
    
    typedef heat_graph_properties_t graph_properties_t;
    
    struct device_properties_t
    {
        unsigned nhoodSize;     // Number of neighbours
        unsigned dt;
    };
    
    struct device_state_t
    {
        unsigned t;
        unsigned cSeen;
        unsigned nSeen;
        
        fixed_t v;
        
        fixed_t vStep;        
        unsigned tEnd;
        
        fixed_t nextStart;
        fixed_t nextStep;
        unsigned nextEnd;
    };
    
    struct forcing{
        static const char *name="forcing";
        
        typedef forcing_segment_message_t message_t;
        
        struct edge_properties_t {};
        
        struct edge_state_t {};
        
        static void on_receive(
            const graph_properties_t *graphProperties,
            const properties_t *deviceProperties,
            state_t *deviceState,
            const edge_properties_t * /*edgeProperties*/,
            edge_state_t */*edgeState*/,
            const message_t *message
        ){
            assert( message->tBegin == deviceState->tEnd );
            assert( message->tBegin < message->tEnd );

            deviceState->nextStart = message->vStart;
            deviceState->nextStep = message->vStep;
            deviceState->nextEnd = message->tEnd;
        }
    };
    
    struct input{
        static const char *name="input";

        
        typedef update_message_t message_t;
        
        struct edge_properties_t {};
        
        struct edge_state_t {};
        
        static void on_receive(
            const graph_properties_t *graphProperties,
            const device_properties_t *deviceProperties,
            device_state_t *deviceState,
            const edge_properties_t *edgeProperties,
            edge_state_t */*edgeState*/,
            const message_t *message
        ){
            if(message->t == deviceState->t){
                deviceState->cSeen++;
            }else if(message->t == deviceState->t + deviceProperties->dt){
                deviceState->nSeen++;           
            }else{
                assert(0);
            }
        }
    };
    
    struct output{  
        static const char *name="output";
        
        typedef update_message_t message_t;
        
        static bool ready_to_send(
            const graph_properties_t *graphProperties,
            const properties_t *deviceProperties,
            const state_t *deviceState
        ){
            return (deviceProperties->nhoodSize == deviceState->cSeen)
                && (deviceState->t < deviceState->tEnd)
        }
        
        static bool on_send(
            const graph_properties_t *graphProperties,
            const properties_t *deviceProperties,
            state_t *deviceState,
            update_message_t *message
        ){
            assert(ready_to_send(graphProperties,deviceProperties,deviceState);
            
            message->t = deviceState->t;
            message->v = deviceState->cAcc;
            
            deviceState->t += deviceProperties->dt;
            deviceState->cSeen = deviceState->nAcc; 
            deviceState->cAcc  = deviceState->nAcc + fixed_mul(deviceState->cAcc, deviceProperties->wSelf);
            deviceState->nSeen = 0;
            deviceState->nAcc = 0;
            
            return true;
        }
    };
    
    BEGIN_DEVICE(boundary_cell)
        BEGIN_DEVICE_INPUTS()
            DEVICE_INPUT(forcing)
            DEVICE_INPUT(input)
        END_DEVICE_INPUTS()
        BEGIN_DEVICE_OUTPUTS()
            DEVICE_OUTPUT(heat)
        END_DEVICE_OUTPUTS()
    END_DEVICE(boundary_cell)
};
