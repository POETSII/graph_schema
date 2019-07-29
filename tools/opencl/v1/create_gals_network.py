#!/usr/bin/env python3

import io
import sys
from typing import *

n=32
max_t=100

class Input:
    def __init__(self, owner:"Dev", index):
        self.owner=owner
        self.index=index
        self.degree=0

class Output:
    def __init__(self, owner:"Dev", index):
        self.owner=owner
        self.index=index
        self.outgoing=[] # type: List[Tuple(Input,int)]

class Dev:
    def __init__(self, id, address, nInputs, nOutputs):
        self.id=id
        self.address=address
        self.inputs=[ Input(self,i) for i in range(nInputs) ]
        self.outputs=[ Output(self, i) for i in range(nOutputs) ]
        self.incoming=[] # List[Tuple[Input,Output,int]]

def connect(dst_pin, src_pin):
    incoming_index=len(dst_pin.owner.incoming) # unique slot within the destination pin
    outgoing_index=len(src_pin.outgoing)
    dst_pin.owner.incoming.append( (dst_pin,src_pin,outgoing_index) )
    dst_pin.degree+=1
    src_pin.outgoing.append( (dst_pin,incoming_index) )


def build_graph(n:int) -> List[Dev]:
    devs=[]
    xy={} # (x,y) -> Dev
    for y in range(0,n):
        for x in range(0,n):
            dev=Dev(f"n_{x}_{y}", len(devs), 1, 1)
            devs.append(dev)
            xy[(x,y)]=dev
    
    for y in range(0,n):
        for x in range(0,n):
            if x!=n-1:
                connect( xy[(x,y)].inputs[0], xy[(x+1,y)].outputs[0])
                connect( xy[(x+1,y)].inputs[0], xy[(x,y)].outputs[0])
            if y!=n-1:
                connect( xy[(x,y)].inputs[0], xy[(x,y+1)].outputs[0])
                connect( xy[(x,y+1)].inputs[0], xy[(x,y)].outputs[0])

    return devs

def render_graph(devs:List[Dev], dst:io.TextIOBase):
    dst.write('#include "kernel_template.cl"\n')
    dst.write('#include "kernel_type.cl"\n\n')

    #####################################################################
    # Write out all the landing zones
    for d in devs:
        dst.write(f"""
message_payload_t {d.id}__payloads[{len(d.outputs)}];
atomic_uint {d.id}__ref_counts[{len(d.outputs)}] = {{ { ",".join(["ATOMIC_VAR_INIT(0)" for i in range(len(d.outputs))]) } }};     

""")

    ######################################################################
    # Do incoming edges
    for d in devs:
        make_incoming_entry=lambda ei: f"""
        {{
            {ei[0].index}, //uint pin_index;
            {ei[1].owner.id}__payloads + {ei[1].index}, //message_payload_t *payload;
            {ei[1].owner.id}__ref_counts + {ei[1].index}, //uint *ref_count;
            0, //const void *edge_properties;
            0 //void *edge_state;
        }}
        """

        dst.write(f"""
incoming_edge {d.id}__incoming_edges[{len(d.incoming)}] =
{{
    {
        ",".join([ make_incoming_entry(ie) for ie in d.incoming ])
    }
}};
uint {d.id}__incoming_landing_bits[{len(d.incoming)}] = {{ 0 }};
""")

    ####################################################################
    # do outgoing edges and ports
    for d in devs:
        make_outgoing_entry=lambda oe: f"""{{ {oe[0].owner.id}__incoming_landing_bits, {oe[1]} }}\n"""
        for op in d.outputs:
            dst.write(f"""
    outgoing_edge {d.id}_p{op.index}__outgoing_edges[] = {{
        {",".join(make_outgoing_entry(oe) for oe in op.outgoing)}
    }};
    """)

        make_outgoing_port=lambda op: f"""
        {{
            {len(op.outgoing)}, //unsigned num_outgoing_edges;
            {d.id}_p{op.index}__outgoing_edges, //outgoing_edge *outgoing_edges;
            {d.id}__payloads+{op.index}, //message_payload_t *payload;
        }}
        """

        dst.write(f"""
output_pin {d.id}__output_pins[{len(d.outputs)}] =
{{
    {",".join(make_outgoing_port(op) for op in d.outputs)}
}};
        """)

    ##################################################################################
    ## Properties and state
    for d in devs:
        dst.write(f"device_properties_t {d.id}__properties={{ {d.inputs[0].degree} }};\n")
        dst.write(f"device_state_t {d.id}__state={{ 0, 0 }};\n")

    #####################################################################################
    ## Device info

    dst.write("__global device_info devices[]={\n")
    for (i,d) in enumerate(devs):
        if i!=0:
            dst.write(",\n")
        dst.write(f"""
    {{
        {d.address}, // address
        {len(d.incoming)}, //uint num_incoming_edges;
        {d.id}__incoming_edges, //incoming_edge *incoming_edges;  // One entry per incoming edge
        {d.id}__incoming_landing_bits, //uint *incoming_landing_bit_mask; // One bit per incoming edge (keep globally mutable seperate from local)

        {len(d.outputs)}, //unsigned num_output_pins;
        {d.id}__output_pins, //const output_pin *output_pins;  // One entry per pin
        {d.id}__ref_counts, //uint *output_ref_counts; // One counter per pin (keep globally mutable seperate from local )

        0, //unsigned device_type_index;
        &{d.id}__properties, //const void *device_properties;
        &{d.id}__state //void *device_state;
    }}    
""")
    dst.write("};\n")

    dst.write(f"""
    graph_properties_t G_properties={{ {max_t} }};

    __kernel void kinit()
{{
    init(&G_properties, devices);
}}

__kernel void kstep(unsigned count)
{{
    for(unsigned i=0; i<count;i++){{
        step(&G_properties, devices);
    }}
}}

const uint __TOTAL_DEVICES__={len(devs)};
""")


if __name__=="__main__":
    g=build_graph(n)
    render_graph(g,sys.stdout)
