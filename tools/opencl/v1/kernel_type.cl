//////////////////////////////////////////////////////////////////
// Application type

typedef struct 
{
    uint max_t;
} graph_properties_t;

typedef struct 
{
    uint degree;
} device_properties_t;

typedef struct 
{
    uint t;
    uint cs;
    uint ns;
} device_state_t;

typedef struct 
{
    uint t;
} message_t;

const uint RTS_FLAG_out = 1;
const uint RTS_FLAG_in = 1;

const int log_level=1;

void calc_rts(uint dev_address, unsigned dev_type_index, const void *graph_properties, const void *device_properties, void *device_state, uint *readyToSend)
{
    assert(dev_type_index==0);

    graph_properties_t *graphProperties=(graph_properties_t*)graph_properties;
    device_state_t *deviceState=(device_state_t*)device_state;
    device_properties_t *deviceProperties=(device_properties_t*)device_properties;

    *readyToSend=0;
    if(deviceState->cs==deviceProperties->degree && deviceState->t <= graphProperties->max_t){
        *readyToSend=RTS_FLAG_out;
    }

    if(log_level>2){
        printf("d%u : rts, cs=%u, deg=%u, t=%u, rts=%x\n", dev_address, deviceState->cs, deviceProperties->degree, deviceState->t, *readyToSend);
    }
}

void do_init(uint dev_address, unsigned dev_type_index, const void *graph_properties, const void *device_properties, void *device_state)
{
    assert(dev_type_index==0);

    const graph_properties_t *graphProperties=(const graph_properties_t*)graph_properties;
    device_state_t *deviceState=(device_state_t*)device_state;
    const device_properties_t *deviceProperties=(const device_properties_t*)device_properties;

    printf("d%u : init\n", dev_address);

    deviceState->cs = deviceProperties->degree;
}

void do_send(uint dev_address, unsigned dev_type_index, unsigned pin_index, const void *graph_properties, const void *device_properties, void *device_state, int *sendIndex, int *doSend, void *payload)
{
    assert(dev_type_index==0);
    assert(pin_index==RTS_FLAG_out);

    const graph_properties_t *graphProperties=(const graph_properties_t*)graph_properties;
    device_state_t *deviceState=(device_state_t*)device_state;
    const device_properties_t *deviceProperties=(const device_properties_t*)device_properties;
    message_t *message=(message_t*)payload;

    assert(deviceState->cs==deviceProperties->degree);

    if(log_level>2){
        printf("d%u : send,  t=%u, cs=%u, ns=%u\n", dev_address, deviceState->t, deviceState->cs, deviceState->ns);
    }

    deviceState->t++;
    deviceState->cs=deviceState->ns;
    deviceState->ns=0;

    message->t=deviceState->t;

    if(deviceState->t == graphProperties->max_t){
        *doSend=false;
        printf("d%u : terminate\n", dev_address);
    }
}

void do_recv(uint dev_address, unsigned dev_type_index, unsigned pin_index, const void *graph_properties, const void *device_properties, void *device_state, const void *edge_properties, void *edge_state, void *payload)
{
    assert(dev_type_index==0);
    assert(pin_index==RTS_FLAG_out);

    const graph_properties_t *graphProperties=(const graph_properties_t*)graph_properties;
    device_state_t *deviceState=(device_state_t*)device_state;
    const device_properties_t *deviceProperties=(const device_properties_t*)device_properties;
    const message_t *message=(const message_t*)payload;

    if(log_level>2){
        printf("d%u : recv,  t=%u, cs=%u, ns=%u,  msg->t=%u\n", dev_address, deviceState->t, deviceState->cs, deviceState->ns, message->t);
    }

    deviceState->cs += deviceState->t==message->t;
    deviceState->ns += deviceState->t!=message->t;
}
