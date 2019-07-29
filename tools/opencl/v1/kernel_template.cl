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
    atomic_uint *landing_bit_mask;
    uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
}outgoing_edge;

typedef struct 
{
    uint dest_pin_index; // Useful to have here, so we can scan a single list of edges
    message_payload_t *payload;
    atomic_uint *ref_count;
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
    atomic_uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

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

    //printf("d%u : step\n", di->address);

    // Try to send on each output pin
    uint rts;
    calc_rts(di->address, di->device_type_index, gp, dp, ds, &rts);
    //printf("d%u :   rts=%x\n", di->address, rts);

    for(unsigned output_index=0; output_index < di->num_output_pins; output_index++){
        if((rts>>output_index)&1){
            uint ref_count=atomic_load(di->output_ref_counts+output_index);
           // printf("d%u :    ref_count %u\n", di->address, ref_count);
            if(ref_count==0){
                // We can send, as there is no remaining user of the buffer
                const output_pin *pin=di->output_pins+output_index;
                // Dispatch to the mega-handler
                int doSend=1;
                int sendIndex=-1;
                do_send(di->address, di->device_type_index, output_index, gp, dp, ds, &sendIndex, &doSend, pin->payload);
                assert(sendIndex==-1);
                if(doSend){
                    atomic_store(di->output_ref_counts+output_index,  pin->num_outgoing_edges);
                    for(unsigned i=0; i<pin->num_outgoing_edges; i++){
                        const outgoing_edge *oe=pin->outgoing_edges+i;
                        uint offset=oe->landing_bit_offset;
                        atomic_fetch_or( oe->landing_bit_mask+(offset>>5), 1<<(offset&0x1f) );
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
                    do_recv(di->address, di->device_type_index, ie->dest_pin_index, gp, dp, ds, ie->edge_properties, ie->edge_state, ie->payload);
                    atomic_fetch_sub( ie->ref_count, 1 ); // TODO: consistency?
                }
                bit_mask_todo>>=1;
                offset++;
            }
            // TODO: IS there a subtle ordering problem between the ref counts and the bit-mask?
            // This needs more thought.
            atomic_fetch_and( di->incoming_landing_bit_mask+edge_index_base/32, ~bit_mask );
        }
    }
}

