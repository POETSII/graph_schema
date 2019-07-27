#define assert(cond) (void)0

/*

    For each outgoing _pin_ we have one outgoing packet buffer and
    an associated ref count.

    For each incoming _edge_ we have a pointer back to the origin packet buffer,
    plus a bit indicating whether the edge is full.

    The process for sending on a pin is:
    - Check the pin's ref count is zero, otherwise you can't send
    - Call the send handler to get the message
    - For each outgoing edge:
      - Atomically "or" the bit into the landing zone bit-mask
    
    The receive process is:
    - For each input edge:
      - If the landing zone pointer is non-zero:
         - call the receive handler
         - zero out the landing zone bit (but _only_ that bit)
         - <atomic consistency barrier>
         - Decrement the outgoing packet buffer count

    Pros:
    - We delay sending messages until it is guaranteed that they will be delivered
      - Actually makes the case for the bit-mask, just as I argue for dumping them...
    - Checking if anyone can send is O(output pins), as we need to look at the RTS bit-mask and the corresponding counters

    Cons:
    - Checking for receives is Theta(input edges), as we have to look at them all. Mitigating factors are:
      - We can read 32-bits at once for high fan-in edges.
      - It is reasonable to do round-robin through all input edges, which makes things a bit fairer
*/

const int MAX_PAYLOAD_SIZE=64;

typedef char message_payload_t[MAX_PAYLOAD_SIZE];

typedef struct 
{
    uint *landing_bit_mask;
    uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
}outgoing_edge;

typedef struct 
{
    uint pin_index;
    message_payload_t *payload;
    uint *ref_count;
    const void *edge_properties;
    void *edge_state;
} incoming_edge;


typedef struct 
{
    unsigned num_outgoing_edges;
    outgoing_edge *outgoing_edges;
    message_payload_t *payload;
} output_pin;

typedef struct 
{
    uint address;
    uint num_incoming_edges;
    incoming_edge *incoming_edges;  // One entry per incoming edge
    uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

    unsigned num_output_pins;
    const output_pin *output_pins;  // One entry per pin
    uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

    unsigned device_type_index;
    const void *device_properties;
    void *device_state;
} device_info;

void calc_rts(uint dev_address, unsigned dev_type_index, const void *graph_properties, const void *device_properties, void *device_state, uint *ready_to_send);
void do_init(uint dev_address, unsigned dev_type_index, const void *graph_properties, const void *device_properties, void *device_state);
void do_send(uint dev_address, unsigned dev_type_index, unsigned pin_index, const void *graph_properties, const void *device_properties, void *device_state, int *sendIndex, int *doSend, void *payload);
void do_recv(uint dev_address, unsigned dev_type_index, unsigned pin_index, const void *graph_properties, const void *device_properties, void *device_state, const void *edge_properties, void *edge_state, void *payload);

void init(
    __global const void *graph_properties,
    __global device_info *devices
){
    device_info *di=devices+get_global_id(0);
    printf("init(gid=%u, di=%p)\n", get_global_id(0), di);

    const void *dp=di->device_properties;
    void *ds=di->device_state;

    do_init(di->address, di->device_type_index, graph_properties, dp, ds);
}

void step(
    __global const void *graph_properties,
    __global device_info *devices
){
    const void *gp=graph_properties;

    device_info *di=devices+get_global_id(0);
    const void *dp=di->device_properties;
    void *ds=di->device_state;

    printf("d%u : step\n", di->address);

    // Try to send on each output pin
    uint rts;
    calc_rts(di->address, di->device_type_index, gp, dp, ds, &rts);
    printf("d%u :   rts=%x\n", di->address, rts);

    for(unsigned output_index=0; output_index < di->num_output_pins; output_index++){
        if((rts>>output_index)&1){
            uint ref_count=di->output_ref_counts[output_index];
            printf("d%u :    ref_count %u\n", di->address, ref_count);
            if(ref_count==0){ // TODO: do we need atomic coherence spec here?
                // We can send, as there is no remaining user of the buffer
                const output_pin *pin=di->output_pins+output_index;
                // Dispatch to the mega-handler
                int doSend=1;
                int sendIndex=-1;
                do_send(di->address, di->device_type_index, output_index, gp, dp, ds, &sendIndex, &doSend, pin->payload);
                assert(sendIndex==-1);
                if(doSend){
                    di->output_ref_counts[output_index] = pin->num_outgoing_edges; // TODO: Should be atomic with spec?
                    for(unsigned i=0; i<pin->num_outgoing_edges; i++){
                        const outgoing_edge *oe=pin->outgoing_edges+i;
                        uint offset=oe->landing_bit_offset;
                        atomic_or( (__global int*)(oe->landing_bit_mask+(offset>>5)), 1<<(offset&0x1f) );
                    }
                }
                // Must re-calculate rts if we called send handler, but can skip for the last one
                if(output_index+1 != di->num_output_pins){
                    calc_rts(di->address, di->device_type_index, gp, dp, ds, &rts);
                }
            }
        }
    }

    // Try to receive on all input edges.
    for(unsigned edge_index_base=0; edge_index_base < di->num_incoming_edges; edge_index_base+=32){
        uint bit_mask=di->incoming_landing_bit_mask[edge_index_base/32];
        if(bit_mask){
            uint bit_mask_todo=bit_mask;
            unsigned offset=0;
            while(bit_mask_todo){
                if(bit_mask_todo&1){
                    incoming_edge *ie=di->incoming_edges+offset;
                    do_recv(di->address, di->device_type_index, ie->pin_index, gp, dp, ds, ie->edge_properties, ie->edge_state, ie->payload);
                    atomic_sub( (__global int *)ie->ref_count, 1 ); // TODO: consistency?
                }
                bit_mask_todo>>=1;
                offset++;
            }
            // TODO: IS there a subtle ordering problem between the ref counts and the bit-mask?
            // This needs more thought.
            atomic_and( (__global int *)(di->incoming_landing_bit_mask+edge_index_base/32), ~bit_mask );
        }
    }
}



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

void calc_rts(uint dev_address, unsigned dev_type_index, const void *graph_properties, const void *device_properties, void *device_state, uint *readyToSend)
{
    assert(dev_type_index==0);

    graph_properties_t *graphProperties=(graph_properties_t*)graph_properties;
    device_state_t *deviceState=(device_state_t*)device_state;
    device_properties_t *deviceProperties=(device_properties_t*)device_properties;

    *readyToSend=0;
    if(deviceState->cs==deviceProperties->degree && deviceState->t < graphProperties->max_t){
        *readyToSend=RTS_FLAG_out;
    }
}

void do_init(uint dev_address, unsigned dev_type_index, const void *graph_properties, const void *device_properties, void *device_state)
{
    assert(dev_type_index==0);

    const graph_properties_t *graphProperties=(const graph_properties_t*)graph_properties;
    device_state_t *deviceState=(device_state_t*)device_state;
    const device_properties_t *deviceProperties=(const device_properties_t*)device_properties;

    printf("d%u : init\n", dev_address);

    assert(deviceState->cs==deviceProperties->degree);

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

    printf("d%u : send,  t=%u, cs=%u, ns=%u\n", dev_address, deviceState->t, deviceState->cs, deviceState->ns);

    deviceState->t++;
    deviceState->cs=deviceState->ns;
    deviceState->ns=0;

    message->t=deviceState->t;
}

void do_recv(uint dev_address, unsigned dev_type_index, unsigned pin_index, const void *graph_properties, const void *device_properties, void *device_state, const void *edge_properties, void *edge_state, void *payload)
{
    assert(dev_type_index==0);
    assert(pin_index==RTS_FLAG_out);

    const graph_properties_t *graphProperties=(const graph_properties_t*)graph_properties;
    device_state_t *deviceState=(device_state_t*)device_state;
    const device_properties_t *deviceProperties=(const device_properties_t*)device_properties;
    const message_t *message=(const message_t*)payload;

    printf("d%u : recv,  t=%u, cs=%u, ns=%u,  msg->t=%u\n", dev_address, deviceState->t, deviceState->cs, deviceState->ns, message->t);

    deviceState->cs += deviceState->t==message->t;
    deviceState->ns += deviceState->t!=message->t;
}




//////////////////////////////////////////////////////////////
// Instance

message_payload_t A_out_payload;
uint A_out_ref_count = 0;

incoming_edge A_to_B_incoming_edge =
{
    0, //uint pin_index;
    &A_out_payload, //message_payload_t *payload;
    &A_out_ref_count, //uint *ref_count;
    0, //const void *edge_properties;
    0 //void *edge_state;
};

uint A_to_B_landing[1] = { 0 };


message_payload_t B_out_payload;
uint B_out_ref_count = 0;

incoming_edge B_to_A_incoming_edge =
{
    0, //uint pin_index;
    &B_out_payload, //message_payload_t *payload;
    &B_out_ref_count, //uint *ref_count;
    0, //const void *edge_properties;
    0 //void *edge_state;
};

uint B_to_A_landing[1] = {0};


outgoing_edge A_to_B_outgoing_edge =
{
    A_to_B_landing, //uint *landing_bit_mask;
    0 //uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
};

output_pin A_output_pin = 
{
    1, //unsigned num_outgoing_edges;
    &A_to_B_outgoing_edge, //outgoing_edge *outgoing_edges;
    &A_out_payload, //message_payload_t *payload;
};


outgoing_edge B_to_A_outgoing_edge =
{
    B_to_A_landing, //uint *landing_bit_mask;
    0 //uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
};

output_pin B_output_pin = 
{
    1, //unsigned num_outgoing_edges;
    &B_to_A_outgoing_edge, //outgoing_edge *outgoing_edges;
    &B_out_payload, //message_payload_t *payload;
};


device_properties_t A_properties = {
    1 // degree
};

device_state_t A_state = {
    0, 0
};

device_properties_t B_properties = {
    1 // degree
};

device_state_t B_state = {
    0, 0
};

graph_properties_t G_properties = {
    10
};


__global device_info devices[]={
    {
        0, // address
        1, //uint num_incoming_edges;
        &B_to_A_incoming_edge, //incoming_edge *incoming_edges;  // One entry per incoming edge
        B_to_A_landing, //uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

        1, //unsigned num_output_pins;
        &A_output_pin, //const output_pin *output_pins;  // One entry per pin
        &A_out_ref_count, //uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

        1, //unsigned device_type_index;
        &A_properties, //const void *device_properties;
        &A_state //void *device_state;
    },{
        1, //address
        1, //uint num_incoming_edges;
        &A_to_B_incoming_edge, //incoming_edge *incoming_edges;  // One entry per incoming edge
        A_to_B_landing, //uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

        1, //unsigned num_output_pins;
        &B_output_pin, //const output_pin *output_pins;  // One entry per pin
        &B_out_ref_count, //uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

        1, //unsigned device_type_index;
        &B_properties, //const void *device_properties;
        &B_state //void *device_state;
    }
};

__kernel void kinit()
{
    printf("kinitB, id=%u\n", devices[get_global_id(0)].address);
    init(&G_properties, devices);
    printf("kinitE\n");
}

__kernel void kstep( )
{
    step(&G_properties, devices);
}