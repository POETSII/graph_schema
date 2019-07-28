This is a work-in-progress general-purpose compiler for turning graphs
into something that can run on an OpenCL device (i.e. GPUs).

Overall compilation approach
============================

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

The reason for this strategy is that (in principle) it allows for the
work-items to be free-running. So you can run the step function in a while
loop in the kernel, without needing multiple kernel executions. This is a good idea
if you have as many concurrent hardware threads as devices, but will deadlock if there
are more devices than hardware threads.

This approach can also be used with one step per kernel, in which case it scales
to any number of work-items without deadlock.

Alternatives
------------

There are many alternatives, most of which come down to avoiding
memory race conditions. Some optiones are:

- Double buffering: messages sent in one step are consumed in the next, with a fresh buffer
  to hold the new messages.

- Split send/recv: one kernel invocation only does sending, while another only does receiving.

- Graph colouring: colour the graph so that no two connected devices have the same colour,
  then run one kernel per colour. 

The above can probably be done without any atomics, which is the thing that
causes the most problems. They would also have determinstic execution,
unlike the currently one which is non-deterministic.
  
Current status
==============

The compiler can successfully translate a few graph instances into OpenCL
(clock_tree, gals_heat_fix, ising_spin). A few others should work as well,
but there are correctness bugs to fix first. Also, some graphs rely on
C++ internally, which won't work until OpenCL 2.2 is fully available.

At the moment ising_spin works pretty well on the GPU in my laptop, but
there are some slight consistency bugs, as it doesn't always make it to
the end of the graph. The other graphs are worse, with gal_heat_fix
petering out after a few steps, and clock_tree only getting a few sends
out before it stalls. So there is some kind of fundamental corretness
bug which eventually pops up. Possibilities are:
- Incorrect construction of the graph
- Errors in the use of atomic operators (should talk to John Wickerson)
- Bugs in the OpenCL implementation

Possible debug approaches:
- Create a pthread implementation to soak test it, then use it for visibility in debugging
- Use a better GPU hardware/runtime-provider with built-in debugging

Running it
==========

Compile the host helper:
```
g++ run_kernel_template.cpp -o run_kernel_template -lOpenCL
```

Compile a graph 'X.xml' into opencl:
```
./render_graph_as_opencl2.py X.xml > X.cl
```

Run the graph:
```
./run_kernel_template X.cl
```

At the moment the host runs for 1000 steps, so output is often quite a way up if it stalled.