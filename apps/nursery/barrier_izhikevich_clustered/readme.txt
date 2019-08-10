/*
    Assumptions:
    - We are targetting around 1M neurons and 1B synapses
    - Networks are randomly connected
    - Spiking level is from 1%-10%
    - We want to scale to around 25 FPGAs
    - Each FPGA has around 1000 threads
    - The simulation is happy with a small latency between spike generation and
      deliver (e.g. a few steps), as long as it is deterministic

    Implications:
    - Each FPGA manages about 40000 neurons
    - Each thread manages about 40 neurons
    - Each neuron spike fans out to about 1000 other neurons
       (so naively this will take at least 1000 instructions)
    - Each spike is delivered to about 40 neurons in each FPGA
       (though because there are 25 FPGAs the cluster sizes will vary)
    - So each spike must be delivered to each FPGA
    - Total spikes _sent_ is about 10K-100K spikes per step
    - Total spikes _delivered_ is about 10M-100M spikes per step
    - Total spikes sent per FPGA is about 400-4000 spikes per step
    - Total _unique_ spikes received per FPGA is about 10K-100K spikes per step (i.e unique spikes from all other FPGAs)
    - Total _delivered_ spikes per FPGA is about 400K-4000K spikes per step

    The fanout of 1000 is problematic, as this means one thread with
    one spike takes 1000 messages to get it out the door. Those spikes
    will also be mostly cross FPGA, so are going to take a long time
    to send due to congestion.

    So strategy is:
    - Create one cluster of neurons per FPGA
    - When a neuron spikes, it sends it to one repeater in each cluster
    - Each spike then gets fanned out by the repeater within the cluster.

    We'll spread this over two steps:
    1 - Generation of spikes at time t:  neuron -> repeater
    2 - Fanout of spikes within cluster, and collection into I_{t+2}: repeater -> neuron

    So spikes that are generated at time t will have an effect
    on the integration process during t+2

    (Aside: we could include another level, which packs lots of spikes
    together to move between clusters. We can move at least 256 bits
    in a packet, so we could create cluster -> clusters "movers" that
    move dense collections of bits, to maximise FPGA to FPGA efficiency.)

    There is then the question of: how many repeaters?
    
    - We could create one repeater per neuron in each FPGA, resulting
      in ~25*1M = 25M repeaters versus 1M neurons, and about 1000 devices
      per thread. Each repeater only sends about 40 messages per spike,
      and only about 10-100 of the repeaters will be active in any cycle.
      The state size of each neuron is also tiny, as it is pretty much two
      bits of information, plus the implied cost of the edges.
      So this option works well if the softswitch is very efficient at managing
      device ready to send status (really we'd want a custom softswtich based
      on masks).
    
    - We could have one repeater manage k devices. For example with k=25
      we'd end up with 1M repeaters and 1M neurons, with 40K repeaters per FPGA.
      The repeater could manage spikes using two bit-masks (one for current
      step, one for next step), and fan-out each spike based on the bits.
      A mild draw-back is that we'd need to one output port per spike, but
      as long as k < 32 then this is feasible - the main cost would be in the
      instruction space for the repeated handler (unless the compiler collapses
      them somehow). As each repeater now maanges k spikes, on average they
      need to distribute k*(0.01..0.1) spikes, with each spike producing 40
      messages, so for k=25 that is 10..100 spikes per cycle per repeater
      (sanity check 10..100 spikes per repeater across 1M repeaters gives
      10M..100M total spikes per step. Checks out.)

    The k-repeater approach makes more sense here, as it seems more
    efficient to handle the bit-mask explicitly within the device rather
    than hoping the softswitch will manage 1000 devices well.

    In terms of how we map repeaters to neurons, we might anticipate
    some benefit in terms of placing routers closer to the place their
    messages are coming from. So probably (?) it makes sense to try
    to try to assign them spikes incoming from a single cluster.

    Making all this concrete:
    - n : Number of neurons
    - c : Number of clusters
    - k : Neurons per repeater
    - s = ceiling(n / c) : Target number of neurons per cluster

    We can't assume that c divides n, so the neurons per cluster
    will be slightly imbalanced, but that shouldn't matter as
    long as s is more than 100 or so (which is must be based on
    our assumptions)

    Neuron id format is:  "c{cluster}_n{neuron}", where:
      - neuron is the linear index from 0..n-1
      - cluster=floor(neuron/s)
      Neuron has two ports:
      - "s_in" : Receive a spike from time t-1
      - "s_out" : Generate a spike at time t
    
    Repeater id format is:  "c{cluster}_r{index}", where:
      - index is an index from [0..ceiling(n/k))
      - cluster is in [0..c)
      This repeater will manage neurons from [index*k..(index+1)*k)
      Each repeater has output pins s_out{0}..s_out{k-1}, with
      output pin s_out{i} corresponding to source neuron index*k+i
      Total pins are:
      - "s_in" : Receive a spike at time t (from any of the source neurons, with edge properties distuiguishing)
      - "s_out{i}" : Generate a spike at time t+1 (for a specific source index*k+i)

    We need to set up the following connections regardless:
    - for i in range(n):
        cluster_src=floor(i/s)
        for cluster_dst in range(c):
            "c{cluster_dst}_r{floor(i/k)}:s_in" <- "c{cluster_src}_n{i}:s_out", with ep={ offset = (i%k) } 

    For each top-level neuron connection src->dst, we need to add the following connections:
    - for (dst,src,w) in synapses:
        cluster=floor(dst/s)
        "c{cluster}_n{dst}:s_in" <- "c{cluster}_r{floor(src/k)}:s_out{src%k}", with ep={ w = w }

 */
