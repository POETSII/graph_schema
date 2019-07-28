
#define assert(cond) (void)0

const int g_log_level=4;


// For shared code.
// This will be shadowed when we can actually tell which thread it is.
__constant uint _dev_address_ = -1;

#define handler_log_0(LL, msg) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_); \
    }

#define handler_log_1(LL, msg, a0) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0); \
    }

#define handler_log_2(LL, msg, a0, a1) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0, a1); \
    }

#define handler_log_3(LL, msg, a0, a1, a2) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0, a1, a2); \
    }

#define handler_log_4(LL, msg, a0, a1, a2, a3) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0, a1, a2, a3); \
    }

#define handler_log_5(LL, msg, a0, a1, a2, a3, a4) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0, a1, a2, a3, a4); \
    }

#define handler_log_6(LL, msg, a0, a1, a2, a3, a4, a5) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0, a1, a2, a3, a4, a5); \
    }

#define handler_log_7(LL, msg, a0, a1, a2, a3, a4, a5, a6) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0, a1, a2, a3, a4, a5, a6); \
    }

#define handler_log_8(LL, msg, a0, a1, a2, a3, a4, a5, a6, a7) \
    if(LL<= g_log_level){ \
        printf("dev %u : " msg "\n", _dev_address_, a0, a1, a2, a3, a4, a5, a6, a7); \
    }

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

#define POETS_ATOMIC_INT int
#define POETS_ATOMIC_VAR_INIT(x) x


#define POETS_ATOMIC_STORE(p,x) *(__global int *)p=x; mem_fence(CLK_GLOBAL_MEM_FENCE);
#define POETS_ATOMIC_LOAD(x,p)  mem_fence(CLK_GLOBAL_MEM_FENCE); x=*(__global int *)p;
#define POETS_ATOMIC_OR(p,v)   atomic_or((__global int *)p, v);
#define POETS_ATOMIC_AND(p,v)  atomic_and((__global int *)p,v);
#define POETS_ATOMIC_SUB(p,v)  atomic_sub((__global int *)p,v);


typedef struct 
{
    POETS_ATOMIC_INT *landing_bit_mask;
    uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
}outgoing_edge;

typedef struct 
{
    uint dest_pin_index; // Useful to have here, so we can scan a single list of edges
    message_payload_t *payload;
    POETS_ATOMIC_INT *ref_count;
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
    POETS_ATOMIC_INT *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

    unsigned num_output_pins;
    const output_pin *output_pins;  // One entry per pin
    POETS_ATOMIC_INT *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

    unsigned device_type_index;
    const void *device_properties;
    void *device_state;
} device_info;

void calc_rts(uint dev_address, uint dev_type_index, const void *graph_properties, const void *device_properties, void *device_state, uint *ready_to_send);
void do_init(uint dev_address, uint dev_type_index, const void *graph_properties, const void *device_properties, void *device_state);
void do_send(uint dev_address, uint dev_type_index, uint pin_index, const void *graph_properties, const void *device_properties, void *device_state, int *sendIndex, int *doSend, void *payload);
void do_recv(uint dev_address, uint dev_type_index, uint pin_index, const void *graph_properties, const void *device_properties, void *device_state, const void *edge_properties, void *edge_state, const void *payload);

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

    //printf("Step %u\n", di->address);

    // Try to send on each output pin
    uint rts;
    calc_rts(di->address, di->device_type_index, gp, dp, ds, &rts);
    //printf(" %u : rts=%0x\n", di->address, rts);

    for(unsigned output_index=0; output_index < di->num_output_pins; output_index++){
        if((rts>>output_index)&1){
            uint ref_count;
            POETS_ATOMIC_LOAD(ref_count, di->output_ref_counts+output_index);
            //printf("d%u :    ref_count %u, %p=p\n", di->address, ref_count, di->output_ref_counts+output_index);
            if(ref_count==0){
                // We can send, as there is no remaining user of the buffer
                const output_pin *pin=di->output_pins+output_index;
                // Dispatch to the mega-handler
                int doSend=1;
                int sendIndex=-1;
                do_send(di->address, di->device_type_index, output_index, gp, dp, ds, &sendIndex, &doSend, pin->payload);
                assert(sendIndex==-1);
                if(doSend){
                    POETS_ATOMIC_STORE((di->output_ref_counts+output_index), pin->num_outgoing_edges);
                    for(unsigned i=0; i<pin->num_outgoing_edges; i++){
                        const outgoing_edge *oe=pin->outgoing_edges+i;
                        uint offset=oe->landing_bit_offset;
                        POETS_ATOMIC_OR( oe->landing_bit_mask+(offset>>5), 1<<(offset&0x1f) );
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
            //printf("  %u : recv\n", di->address);
            uint bit_mask_todo=bit_mask;
            unsigned offset=0;
            while(bit_mask_todo){
                if(bit_mask_todo&1){
                    incoming_edge *ie=di->incoming_edges+offset;
                    //printf("   %u : address=%u, device_type_index=%u, dest_pin_index=%u\n", di->address, di->address, di->device_type_index, ie->dest_pin_index);
                    do_recv(di->address, di->device_type_index, ie->dest_pin_index, gp, dp, ds, ie->edge_properties, ie->edge_state, ie->payload);
                    //printf("   %u : decrement ref at %p\n", di->address, ie->ref_count);
                    POETS_ATOMIC_SUB( ie->ref_count, 1 ); // TODO: consistency?
                }
                bit_mask_todo>>=1;
                offset++;
            }
            // TODO: IS there a subtle ordering problem between the ref counts and the bit-mask?
            // This needs more thought.
            POETS_ATOMIC_AND( di->incoming_landing_bit_mask+edge_index_base/32, ~bit_mask );
        }
    }

}

