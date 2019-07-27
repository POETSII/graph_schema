#include "kernel_template.cl"

#include "kernel_type.cl"



//////////////////////////////////////////////////////////////
// Instance

// Note: creating persistent global data structures in this way is valid,
// but only in OpenCL 2. See:
// https://www.khronos.org/registry/OpenCL/sdk/2.0/docs/man/xhtml/global.html
//
// So these data structures are bound to the lifetime of the cl_program object.
// Presumably compiling a program twice would result in two copies.

message_payload_t A_out_payload;
uint A_out_ref_count = 0;

message_payload_t B_out_payload;
uint B_out_ref_count = 0;

message_payload_t C_out_payload;
uint C_out_ref_count = 0;


incoming_edge A_incoming_edges[] =
{
    {
        0, //uint pin_index;
        &B_out_payload, //message_payload_t *payload;
        &B_out_ref_count, //uint *ref_count;
        0, //const void *edge_properties;
        0 //void *edge_state;
    }
};
uint A_incoming_landing_bits[1] = { 0 };


incoming_edge B_incoming_edges[] =
{
    {
        0, //uint pin_index;
        &A_out_payload, //message_payload_t *payload;
        &A_out_ref_count, //uint *ref_count;
        0, //const void *edge_properties;
        0 //void *edge_state;
    },{
        0, //uint pin_index;
        &C_out_payload, //message_payload_t *payload;
        &C_out_ref_count, //uint *ref_count;
        0, //const void *edge_properties;
        0 //void *edge_state;
    }
};
uint B_incoming_landing_bits[1] = { 0 };

incoming_edge C_incoming_edges[] =
{
    {
        0, //uint pin_index;
        &B_out_payload, //message_payload_t *payload;
        &B_out_ref_count, //uint *ref_count;
        0, //const void *edge_properties;
        0 //void *edge_state;
    }
};
uint C_incoming_landing_bits[1] = { 0 };


outgoing_edge A_outgoing_edges[] = {
    {
        B_incoming_landing_bits, //uint *landing_bit_mask;
        0 //uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
    }
};
output_pin A_output_pin = 
{
    1, //unsigned num_outgoing_edges;
    A_outgoing_edges, //outgoing_edge *outgoing_edges;
    &A_out_payload, //message_payload_t *payload;
};


outgoing_edge B_outgoing_edges[] = {
    {
        A_incoming_landing_bits, //uint *landing_bit_mask;
        0 //uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
    },{
        C_incoming_landing_bits, //uint *landing_bit_mask;
        0 //uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
    }
};
output_pin B_output_pin = 
{
    2, //unsigned num_outgoing_edges;
    B_outgoing_edges, //outgoing_edge *outgoing_edges;
    &B_out_payload, //message_payload_t *payload;
};

outgoing_edge C_outgoing_edges[] = {
    {
        B_incoming_landing_bits, //uint *landing_bit_mask;
        1 //uint landing_bit_offset; // Offset from the start of the mask (i.e. may be more than 32)
    }
};
output_pin C_output_pin = 
{
    1, //unsigned num_outgoing_edges;
    C_outgoing_edges, //outgoing_edge *outgoing_edges;
    &C_out_payload, //message_payload_t *payload;
};



device_properties_t A_properties = {
    1 // degree
};

device_state_t A_state = {
    0, 0
};

device_properties_t B_properties = {
    2 // degree
};

device_state_t B_state = {
    0, 0
};

device_properties_t C_properties = {
    1 // degree
};

device_state_t C_state = {
    0, 0
};

graph_properties_t G_properties = {
    10
};


__global device_info devices[]={
    {
        0, // address
        1, //uint num_incoming_edges;
        &A_incoming_edges, //incoming_edge *incoming_edges;  // One entry per incoming edge
        A_incoming_landing_bits, //uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

        1, //unsigned num_output_pins;
        &A_output_pin, //const output_pin *output_pins;  // One entry per pin
        &A_out_ref_count, //uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

        1, //unsigned device_type_index;
        &A_properties, //const void *device_properties;
        &A_state //void *device_state;
    },{
        1, //address
        2, //uint num_incoming_edges;
        &B_incoming_edges, //incoming_edge *incoming_edges;  // One entry per incoming edge
        B_incoming_landing_bits, //uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

        1, //unsigned num_output_pins;
        &B_output_pin, //const output_pin *output_pins;  // One entry per pin
        &B_out_ref_count, //uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

        1, //unsigned device_type_index;
        &B_properties, //const void *device_properties;
        &B_state //void *device_state;
    },{
        2, //address
        1, //uint num_incoming_edges;
        &C_incoming_edges, //incoming_edge *incoming_edges;  // One entry per incoming edge
        C_incoming_landing_bits, //uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

        1, //unsigned num_output_pins;
        &C_output_pin, //const output_pin *output_pins;  // One entry per pin
        &C_out_ref_count, //uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

        1, //unsigned device_type_index;
        &C_properties, //const void *device_properties;
        &C_state //void *device_state;
    }
};

__kernel void kinit()
{
    printf("kinitB, id=%u\n", devices[get_global_id(0)].address);
    init(&G_properties, devices);
    printf("kinitE\n");
}

__kernel void kstep(unsigned count)
{
    for(unsigned i=0; i<count;i++){
        step(&G_properties, devices);
    }
}
