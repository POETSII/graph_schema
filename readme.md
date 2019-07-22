graph_schema - Version 3
========================

This repository is intended to describe the format for graphs via:

- Documentation : Overview of the graphs and how they are structured

- Schemas : Machine readable specifications for persisted graphs which specify
  overall structural correctness.

- Tools : Reference code for working with graphs, which can perform more
  detailed validation, as well as providing application independent ways of
  working with graphs.

- Examples : Compliant graph examples.

There will be many different kinds of graphs used, but for the moment
this repository focusses on _virtual_ graphs, i.e. graphs which do
not worry about physical implementation and layout, and only talk
about structure, functionality, and connectivity. The schemas for
other graphs could also be added here, but that is up for discussion.

The policy is that things should not be merged to master
unless `make test` works.

Version 3 features
------------------

* Optional OnInit handler added - Standardizes how individual device
initialisation is done.
* `<S>` tag added to DevI to allow individual setting of device instance states.
* Optional Default tag has been added to Scalar, Array, Tuple, and Union -
Takes JSON strings which are parsed to provide default values to instances
of these data types.
* Optional attribute "indexed" in OutputPin added:
    * Boolean used to indicate whether this pin broadcasts or sends to an
    index - default is broadcast.
* Optional attribute "sendIndex" for Edge instances added, accepting an
integer to identify this pin for indexed send Output pin.
* Application pins removed - They have been deprecated by Externals.
* DeviceSharedCode removed - SharedCode in DeviceType performs this task.
* Fix incorrect definition for TypeDef (typedDataSpec -> typedDataMember).
* TypeDef can now contain documentation.
* Dense or Sparse array initialisation provided for Arrays.
* Digest hashes for types and instances added.
* Added <SupervisorType>, renamed from <SupervisorDeviceType> in previous
versions.
* GraphInstance attribute "supervisorTypeId" changed from
"supervisorDeviceTypeId" to match name of SupervisorType.
* Updated SupervisorType to the specification discussed with Southampton.
* <OnCompute> and <OnIdle> tags are to be entirely removed.
    * These are replaced by optional <OnHardwareIdle>, <OnDeviceIdle> and <OnThreadIdle>.
    * This provides more detailed descriptions of when these handlers are executed.

An exemplar XML for version 3, featuring *one of everything* can be found in
`master/virtual-graph-schema-v3-exemplar-xml.xml`

**A conversion script from version 2 to version 3 is included in with this
new version of the schema. Information on it's usage is included
[here](#Tools)**

Requirements
============

The recommended way of setting things up (especially under windows) is by
using [vagrant](https://www.vagrantup.com/) in order to get a virtual machine
that is configured with all the libraries. The file [Vagrantfile](Vagrantfile)
contains setup instructions that should result in a fully configured Ubuntu 16
instance with all libraries.

So assuming you have vagrant installed, and some sort of virtualiser (e.g.
[VirtualBox](https://www.virtualbox.org/), then you should be able to
get going with:
````
git clone git@github.com:POETSII/graph_schema.git
cd graph_schema
vagrant up    # Wait as it downloads/starts-up/installs the VM
vagrant ssh
cd /vagrant   # Get into the repository
make test     # Run the built-in self tests
````

Manual installation
-------------------

A non-complete list of packages needed is:
- `libxml2-dev`
- `g++` (with c++11 support)
- `libboost-dev` and `libboost-dev`
- python3.4 (or higher)
- `zip`
- A java environment (e.g. `default-jre-headless`)
- `python3-lxml`
- `curl`
- `mpich`
- `rapidjson-dev`
- `libboost-filesystem-dev`
- `metis`
- `graphviz`
- `imagemagick`
- `ffmpeg`
- `python3-pip`
- `python3-numpy`
- `python3-scipy`
- pyamg (`pip3 install pyamg`)
- `octave` (and `octave-msh` `octave-geometry`)

Not officially tested on non Ubuntu 16 platforms, but it has worked
in the past in both Cygwin and Linux.

Schema
======

The structure of the application format is captured in a relax ng xml schema, which
checks a fair amount of the structure of the file, such as allowed/missing elements
and attributes. The schema is available at [master/virtual-graph-schema.rnc](master/virtual-graph-schema-v3.rnc).

For checking purposes, the schema can be validated directly using `tring`, or
is also available as an [xml schema (.xsd)](derived/virtual-graph-schema-v3.xsd)
which many XML parsers can handle.

To validate a file `X.xml` you can ask for the file `X.checked`. This
works for graph types:

    make apps/ising_spin/ising_spin_graph_type.checked

and graph instances:

    make apps/ising_spin/ising_spin_16_16.checked

If there is a high-level syntax error, it should point you towards it.

A slightly deeper check on graph instances is also available:

    python3.4 tools/print_graph_properties.py apps/ising_spin/ising_spin_16x16.xml

This checks some context-free aspects, such as id matching. Beyond this level
you need to try running the graph.

For reference, an example XML, which features one of everything in the schema,
has been included in `virtual-graph-schema-v3-exemplar-xml.xml`.

Usage
=====

For the top-level check, do:

    make test

This should check the schema, validate all examples/test-cases
against the schema, and also validate them by parsing them
and simulating them.

To look out at output, do:

    make demo

This will take longer and use more disk-space - it may also fail
as things evolve over time, so you may want to do `make demo -k`.
It will generate some sort of animated gifs in the `apps/*` directories.

Applications
============

There is a small set of basic applications included. These are not
amazingly complicated, but provide a way of testing orchestrators and
inter-operability. Models that are *Deterministic* should provide
the same application-level results independently of the orchestrator.
Models that are *Deterministic(FP)* should provide the same application-level
results on any implementation, except for errors introduced in floating-point
(see the later notes on fixed-point implementations). Models that are
*Chaotic* have a dependency on the exact execution order and/or message
order - they should always provide some kind of "correct" answer, but the answer
you get may vary wildly.

Models that are *Asynchronous* do not rely on any kind of internal clock,
so there is no central synchronisation point being built. These may be
sub-classed into *Async-GALS* which use the time-stepping approach,
and *Async-Event* which use the local next-event approach. Models that are 
*Synchronous* internally build some kind of central clock (hopefully
using logarithmic time methods), so there is an application barrier.
Models that are *Barrier* use the internal hardware idle detection
feature through `OnHardwareIdle`.

- [apps/ising_spin](apps/ising_spin) : *Deterministic(FP)*, *Async-Event* : This is an asynchronous ising spin model,
  which models magnetic dipole flipping. Each cell moves forwards using random
  increments in time, with the earliest cell in the neighbourhood advancing.
  Regardless of execution or message ordering, this should always give exactly
  the same answer (even though it uses floating-point, I _think_ it is
  deterministic as long as no denormals occur).

- [apps/ising_spin](apps/ising_spin_fix) : *Deterministic*, *Async-Event* : A
  fixed-point version of ising_spin which is fully deterministic. It has a self-checker
  to verify determinism versus reference software.

- [apps/gals_heat](apps/gals_heat) : *Deterministic(FP)*, *Async-GALS* : This models a
  2D DTFD heat equation, with time-varying dirichlet boundaries. All cells within
  a neighbourhood are at time t or t+1 - once the value of all neighbours at time t is
  known, the current cell will move forwards.

- [apps/gals_izhikevich](apps/gals_izhikevich) : *Deterministic(FP)*, *Async-GALS* : This is
  an Izhikevich spikiung neural network, using a loose form of local synchronisation
  to manage the rate at which time proceeds. However, it is not very good - it
  does lots of local broadcasting (whether neurons fire or not), and it doesn't
  work correctly with an unfair orchestrator.

- [apps/clocked_izhikevich](apps/clock_ishikevich) : *Deterministic(FP)*, *Synchronous* : Also
  an izhikevich neural network, but this time with a global clock device.
  The whole thing moves forward in lock-step, but needs 1:n and n:1 communiciation
  to the clock.

- [apps/clock_tree](apps/clock_tree) : *Deterministic*, *Synchronous* : A simple benchmark, with three devices:
  root, branch, and leaf. The root node is a central clock which generates ticks.
  The ticks are fanned out by the branches to the leaves, which reflect them back
  as tocks. Once the clock has received all the tocks, it ticks again.

- [apps/amg](apps/amg) : *Deterministic(FP)*, *Async-GALS* : A complete algebraic multi-grid solver,
  with many types of nodes and quite complex interactions. It should be self-checking.

- [apps/relaxation_heat](apps/relaxation_heat) : *Chaotic*, *Asynchronous* : This
  implements the chaotic relaxation method for heat, whereby each point in the
  simulation tries to broad-cast it's heat whenever it changes. This particular
  method uses two "interesting" features:

  - Network clocks: each message is time-stamped with a version, so old messages
    that are delayed are ignored. This ensures that time can't go backwards, and
    makes convergence much faster for out-of-order simulators.

  - Distributed termination detection: in parallel with the relaxation there is
    a termination system going on, which uses a logarithmic tree to sum up the
    total number of sent and received messages. Once the relaxation has stopped,
    it will detect this within two sweeps, so for a system of size n it detects
    termination in O(log n) time.

- [apps/betweeness_centrality](apps/betweeness_centrality) : *Chaotic*, *Asynchronous* :
  This calculates a measure of betweeness centrality using random walks over graphs.
  Each vertex (device) is seeded with a certain number of walks, and then each device
  sends walks randomly to a neighbouring vertex. Once a walk has stepped through a
  fixed number of verticies it expires. The number of walks passing through each
  node is then an estimate of centrality. This application is a show-case/test-case
  for indexed sends, as it only sends along one outgoing edge. There are some
  degenerate cases that can occur, so the devices use a data-structure to detect
  idleness in logarithmic time.

- [apps/storm](apps/storm): *Chaotic*, *Asynchronous* : This is not really
  a test-case, it is more to check implementations and explore behaviour - e.g. it
  was used for Tinsel 0.3 to look for messaging bottlenecks. The graph is
  randomly connected, with all nodes reachable. One node is the root, which is
  initially given all credit. Any node with credit splits and shares it randomly
  with it's neighbours in a way that is dependendent on scheduling. When credit
  comes back to the root it is consumed, and once all credit is back at the
  root it terminates. The run-time of this graph can be highly variable, particularly
  when high-credit messages get "stuck" behind low-credit ones, but (barring implementation
  errors) it is guaranteed to terminate in finite time on all implementations.

Currently all applications are designed to meet the abstract model, which means
that messages can be arbitrarily delayed or re-ordered, and sending may be
arbitrarily delayed. However, they are currently all interolerant to any
kind of loss or error - AFAIK they will all eventually lock-up if any message
is lost or any device fails.

### Fixed-point implementations

For some applications there will be a `*_fix` version, which is usually
just a fixed-point version of the application. This was originally
useful back when there was no floating-point support in the Tinsel
CPU. They are still sometimes useful now for testing purposes, as
it is generally much easier to make fixed-point versions bit-exact
deterministic under all possible executions. Some common problems that
can occur with repeatability and floating-point are:

- Adding together floating-point values that arrive in messages. Because
 the messages can arrive in different orders, the sum of the values
 may change slightly due to the non-associativity of floating-point addition.

- FPGA and CPU floating-point often behaves differently in the space just
  above zero, with FPGAs often flushing to zero, while CPUs support full
  denormals. While this is an obscure case, in a very long-running application
  it can occur, and even the slightest deviation destroys bit-wise equivalence.

- Special functions such as sqrt, exp, log need to be implemented from scratch
  for Tinsel, and should use algorithms optimised for it - for example, it is
  currently advantageous to use division-heavy algorithms. Even if these functions
  are faithfully rounded (i.e. correct to 1-ulp) they are very likely to provide
  different roundings to software libraries.

Tools
=====

### tools/convert_v2_graph_to_v3.py

Takes an XML graph type or graph instance which is written using version 2.*
of the schema, and converts this to version 3 of the schema. This converts
any `__init__` input pins to `<OnInit>` handlers, and removes any application
pin attributes.

Usage:
```
python tools/convert_v2_graph_to_v3.py <path-to-v2-XML> >> <path-to-new-XML-file>
```

If no path to a new XML file is provided, the XML will be printed to the
screen.

### tools/convert_v3_graph_to_v4.py

Takes an XML graph type or graph instance which is written using version 3
of the schema, and converts this to version 4 of the schema. It attempts to
preserve semantics, but currently documentation and meta-data are lost.

Usage:
```
python tools/convert_v3_graph_to_v4.py <path-to-v3-XML> > <path-to-new-XML-file>
```

### tools/convert_v3_graph_to_v4.py

Takes an XML graph type or graph instance which is written using version 4
of the schema, and converts this to version 3 of the schema. It should
convert all features of v4 correctly (correct as of the very first v4 spec).

Usage:
```
python tools/convert_43_graph_to_v3.py <path-to-v4-XML> > <path-to-new-XML-file>
```


### bin/epoch_sim

This is an epoch based orchestrator. In each epoch, each device gets a chance to
send from one port, with probability `probSend`. The order in which devices send
in each round is somewhat randomised. All devices are always ready to recieve, so
there is no blocking or transmission delay. Note that this simulator tends to
be very "nice", and makes applications that are sensitive to ordering appear
to work. epoch_sim is good for initial debugging and dev, but does not  guarantee
it will work in hardware.

Example usage:
```
make demos/ising_spin_fix/ising_spin_fix_16_2.xml
bin/epoch_sim apps/ising_spin/ising_spin_16x16.xml
```

Environment variables:

- `POETS_PROVIDER_PATH` : Root directory in which to search for providers matching `*.graph.so`. Default
  is the current working directory.

Parameters:

- `--log-level n` : Controls output from the orchestrator and the device handlers.

- `--max-steps n` : Stop the simulation after this many epochs. It will also stop
  if all devices are quiescent.

- `--snapshots interval destFile` : Store state snapshots every `interval` epochs to
  the given file.

- `--prob-send probability` : Control the probability that a device ready to send
  gets to send within each epoch. Default is 0.75.

- `--log-events destFile` : Log all events that happen into a complete history. This
  can be processed by other tools, such as `tools/render_event_log_as_dot.py'.

### bin/graph_sim

This is a much more general simulator than epoch_sim, and is useful
for finding more complicated protocol errors in applications. The
usage is mostly the same as epoch_sim, though it has a slighly different
usage. The biggest change is that it supports the idea of messages
being in flight, so a message which is sent can spend some arbitrary
amount of time in the network before it is delivered. This also
applies to individual destinations of a source message, so the
messages in-flight are actually (dst,msg) pairs which are delivered
seperately.

Usage:
```
make demos/ising_spin_fix/ising_spin_fix_16_2.xml
bin/graph_sim apps/ising_spin/ising_spin_16x16.xml
```

The value of graph_sim is that it allows you to simulate more hostile
environments, particularly with respect to out of order delivery of
messages. To support this there are two main features:

- *prob-send* : this is a number between 0.0 and 1.0 that affects whether
  the simulator prefers to send versus receive. It is somewhat similar to
  the send-vs-receive flag in POETS ecosystem, but happens at a global 
  rather than local level. It is often a useful way of detecting applications
  that have infinite send failure modes (by setting *prob-send* to 1.0), or
  those which have starvation modes (set *prob-send* to 0.0, or very low).

- *strategies*: these influence the order in which messages are delivered,
  with the following approaches currently supported:

  - `FIFO` : This is similar to epoch_sim, in that it delivers messages in
    the order in which they were sent. However, unlike epoch sim the different
    message parts are delivered seperately, so while parts of one message
    remain undelivered a new message might enter the system. This slight difference
    can uncovered interesting failure modes and infinite loops.

  - `LIFO` : This is the exact opposite, and whenever there are multiple
    messages in flight the oldest one will be delivered. This often uncovers
    assumptions about ordering in applications, where the writer imposes
    some assumed ordering that isn't there in practise.

  - `Random` : As it sounds, this picks a random message in flight. This is
    not especially effective at finding bugs in one run, but is useful for
    soak tests where you run huge numbers of times with the same seed. It
    can also uncovered non-determinism when you expected it to be deterministic.

Environment variables:

- `POETS_PROVIDER_PATH` : Root directory in which to search for providers matching `*.graph.so`. Default
  is the current working directory.

Parameters:

- `--log-level n` : Controls output from the orchestrator and the device handlers.

- `--max-events n` : Stop the simulation after this many events. It will also stop
  if all devices are quiescent.

- `--prob-send probability` : The closer to 1.0, the more likely to send. Closer to 0.0 will prefer receive.

- `--log-events destFile` : Log all events that happen into a complete history. This
  can be processed by other tools, such as `tools/render_event_log_as_dot.py'.

Limitations:

- Currently graph_sim does not support OnHardwareIdle or OnDeviceIdle.

- Currently graph_sim does not support externals of any type.


### tools/render_graph_as_dot.py

This takes a graph instance and renders it as a dot file for visualisation. Given
a snapshot file it can generate multiple graphs for each snapshot.

Parameters:

- `G` : The name of the graph instance file, or by default it will read from stdin.

- `--snapshots SNAPSHOTS` : Read snapshots from the given file. If snapshots are
  specified then it will generate a graph for each snapshot, using the pattern `OUTPUT_{:06}.dot`.

- `--bind-dev idFilter property|state|rts name attribute expression` : Used to bind
  a dot property to part of the graph.

  - `idFilter` : Name of a device type, or `*` to bind to all device types.

  - `property|state|rts` : Controls which part of the device to bind to.

  - `name` : Name of the thing within the property of state.

  - `attribute` : Graphviz attribute that will be affected (e.g. `color` or `bgcolor`)

  - `expression` : Python expression to use for attribute. `value` will be available, containing the
    requested value from the graph.

- `--bind-edge idFilter property|state|firings name attribute expression` : Same as `--bind-dev`
  but for edges.

- `--output OUTPUT` : Choose the output name, or if snapshots are specified the output prefix.

### tools/render_event_log_as_dot.py

Takes an event log (e.g. generated by `bin/epoch_sim`) and renders it as a graph.
_Beware_: don't try to render lots of events (e.g. 1000+) or large numbers of devices (30+),
as the tools will take forever, and the results will be incomprehensible.

Example usage:
````
bin/epoch_sim apps/ising_spin/ising_spin_8x8.xml --max-steps 20 --log-events events.xml
tools/render_event_log_as_dot.py events.xml
dot graph.dot -Tsvg -O
````
Should produce an svg called `graph.svg`.

### tools/preprocess_graph_type.py

Some apps and graphs within graph_schema use pre-processing to embed
or genericise the contents of graph types. Any expansions needed to create
a standard graph type are done by this tool, which takes a graph type
and produces an expanded/pre-processed graph type. Note that the pre-processing
done here is in addition to (actually before) any standard C pre-processing.

This tool can be run on any graph type, and will pass through graph
types that don't have any graph_schema pre-processing features.

Example usage:
```
tools/preprocess_graph_type.py apps/ising_spin_fix/ising_spin_fix_graph_type.xml > tmp.xml
```
If you run the above you'll find that the local header file referenced from the
graph type has been embedded into the output xml.

#### Direct embedding of include files

It is often useful to seperate functionality out into an external header,
particularly when you want shared functionality or logic between a
graph type and a sequential software implementation. However, it is
desireable to create self-contained graph types which don't rely on
any non-standard system header files, so that the graph files can be
removed to remote machines and run there. The direct embedding pragma
allows certain `#include` statements to be expanded in place when
pre-processing the graph type, making the source code self-contained.

Given an include statement:
```
#include "some_local_header.hpp"
```
we can prefix it with a graph_schema-specific pragma:
```
#pragma POETS EMBED_HEADER
#include "some_local_header.hpp"
```
When the graph is pre-processed the code will be re-written into:
```
/////////////////////////////////////
//
#pragma POETS BEGIN_EMBEDDED_HEADER "some_local_header.hpp" "/the/actual/absolute_path/to/some_local_header.hpp"

void actual_contents_of_some_local_header()
{
}

#pragma POETS END_EMBEDDED_HEADER "some_local_header.hpp" "/the/actual/absolute_path/to/some_local_header.hpp"
//
////////////////////////////////////
```
The pre-processed code will then turn up in the headers.

The pragma is allowed in an place where source code appears in the graph type.

Note that this feature is not currently recursive, so if there is a `#include`
in the file being embedded it won't get expanded.

