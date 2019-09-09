graph_schema - Version 3
========================

This repository is intended to describe the format for graphs via:

- Documentation : Overview of the graphs and how they are structured

- Schemas : Machine readable specifications for persisted graphs which specify
  overall structural correctness.

- Tools : Reference code for working with graphs, which can perform more
  detailed validation, as well as providing application independent ways of
  working with graphs.

- Applications : Compliant applications and graph generators.

There will be many different kinds of graphs used, but for the moment
this repository focusses on _virtual_ graphs, i.e. graphs which do
not worry about physical implementation and layout, and only talk
about structure, functionality, and connectivity. The schemas for
other graphs could also be added here, but that is up for discussion.

The policy is that things should not be merged to master
unless `make test` works.

Current TODO list
-----------------

- Add externals to epoch_sim
- Add externals to graph_sim
- Correctly hook up __halt__ if it is present (then deprecate fake_handler_exit)

Current BUG list
----------------

- None known

Version 4.1 features
--------------------

* Python tools can save in v3 (default) or v4 format. Set the environment variable
  `GRAPH_SCHEMA_DEFAULT_XML_OUTPUT_VERSION` to `3` or `4` to explicitly choose.
* C++ tools can load v4 and will automatically switch between v3 and v4.

Version 4 features
------------------

* The XML v4 format is supported via v3 to v4 and v4 to v3 tools. XML v4 is an alternative to V3 which has been
  simplified to make parsing easier. See PIP0020.
* Python tools can ingest v3 or v4, and auto-magically switch between them.


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

pypy
----

The python tools should all work under [pypy](https://pypy.org/), which is a JIT compiling
version of python. Many of the graph_schema tools get a decent speed boost of around 2-3x,
and nothing seems to run any slower. pypy is particularly useful when generating larger
graphs using python, as you usually get about twice the generation rate. There is a provisioning
script in `provision_ubuntu18.sh` which _should_ setup a pypy3 [virtualenv](https://virtualenv.pypa.io/en/latest/),
though it is a bit fragile and may require some hand-holding.

To setup to the pypy virtualenv, from the root of the graph_schema repo:
```
./provision_ubuntu18_pypy.sh
```
Look at the messages going past, as it is possible something goes slightly wrong.

Assuming it is setup, then do:
```
source pypy3-virtualenv/bin/activate
```
to enter the virtual environment. If this succeeds, then you should find
that your command prompt is prefixed by `(pypy3-virtualenv)`. From this
point on the `python3` command is redirected to `pypy3`, which you can
check with `python3 -V`. To leave the virtualenv, use `deactivate`.

For example, on my current machine I get this:
```
dt10@LAPTOP-0DEHDEQ0:/mnt/c/UserData/dt10/external/POETS/graph_schema
$ source pypy3-virtualenv/bin/activate

(pypy3-virtualenv) dt10@LAPTOP-0DEHDEQ0:/mnt/c/UserData/dt10/external/POETS/graph_schema
$ python3 -V
Python 3.6.1 (7.1.1+dfsg-1~ppa1~ubuntu18.04, Aug 09 2019, 16:05:55)
[PyPy 7.1.1 with GCC 7.4.0]

(pypy3-virtualenv) dt10@LAPTOP-0DEHDEQ0:/mnt/c/UserData/dt10/external/POETS/graph_schema
$ deactivate

dt10@LAPTOP-0DEHDEQ0:/mnt/c/UserData/dt10/external/POETS/graph_schema
$
```

It's not a big deal if it doesn't work, as everything still works in standard python (i.e. cpython).

Manual installation
-------------------

The packages needed for ubuntu 18.04 are given in the provisioning
script `provision_ubuntu18.sh`.

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

### bin/calculate_graph_static_properties

Takes a graph instance and calculates a set of basic static properties
about the graph, including:

- Device/edge instance count
- Degree distribution
- Counts of edges between different device/port pairs.

Usage
-----

```
bin/calculate_graph_static_properties path-to-xml [metadata.json]
```
The path-to-xml must always be supplied.

If a metadata.json is supplied, then the data calculated by this
program will be inserted into the given document (i.e. it will
inherit those properties).


Example usage:
````
$ bin/calculate_graph_static_properties apps/ising_spin/ising_spin_8x8.xml > wibble.json
$ less wibble.json
{
    "graph_instance_id": "heat_8_8",
    "graph_type_id": "ising_spin",
    "total_devices": 64,
    "total_edges": 256,
    "device_instance_counts_by_device_type": {
        "cell": 64
    },
    "edge_instance_counts_by_message_type": {
        "update": 256,
        "__print__": 0
    },
    "edge_instance_counts_by_endpoint_pair": {
        "<cell>:in-<cell>:out": 256
    },
    "incoming_degree": {
        "min": 0.0,
        "max": 4.0,
        "mean": 2.0,
        "median": 1.3333333333333333,
        "stddev": 2.0
    },
    "outgoing_degree": {
        "min": 4.0,
        "max": 4.0,
        "mean": 4.0,
        "median": 4.0,
        "stddev": 0.0
    }
}
````

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

### tools/render_graph_as_field.py

Takes a snapshot log (generated by `bin/epoch_sim`), treats it as a 2D field, and renders
it back out as a sequence of bitmaps.

Note that this tool assumes two pieces of meta-data exist:

- On the graph-type: `"location.dimension": 2`. Used to verify it is in fact
  a planar "thing".

- On each device instance: `"loc":[x,y]`. Gives the x,y co-ordinate of each
  device instance. Only device instances you want to plot as pixels need this
  property.

Example usage:
```
# Create a fairly large gals heat instance
apps/gals_heat/create_gals_heat_instance.py 128 128 > tmp.xml
# Run a graph and record snapshots at each epoc into a file graph.snap
bin/epoch_sim tmp.xml --max-steps 100 --snapshots 1 graph.snap --prob-send 0.5
# Render the snapshots as pngs, rendering the time as colour
tools/render_graph_as_field.py tmp.xml  graph.snap   --bind-dev "cell" "state" "t" "blend_colors( (255,255,0), (255,0,255), 0, 10, (value%10))"
# Convert it to a video
ffmpeg -r 10 -i graph_%06d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 gals_time.mp4
```
Should produce an mp4 called `gals_time.mp4` which shows the per-device evolution of time.
Note that you'll need a decent video player like mediaplayerclassic or VLC to handle
it; as the built-in windows things often don't work.

Parameters:

- `G` : The name of the graph instance file.

- `S` : The name of the snapshot file.

- `--bind-dev idFilter property|state|rts name expression` : Used to bind
  a feature of the graph to the colour of the field.

  - `idFilter` : Name of a device type, or `*` to bind to all device types.

  - `property|state|rts` : Controls which part of the device to bind to.

  - `name` : Name of the thing within the property of state.

  - `expression` : Python expression to use for attribute. `value` will be available, containing the
    requested value from the graph.

- `--output_prefix OUTPUT` : The prefix to stick in front of all the png file-names

You can apply different filters for different device types, and so ignore devices
which don't represent pixels.

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


### tools/poems

POEMS is a tool which is supposed to exectute a graph instance as fast
as possible across multiple cores. POEMS is a more advanced simulator
than the others, and is really more of a full-on execution environment
rather than a simulator.In some ways it is the successor to `bin/queue_sim`,
but it shares no code with it.

Unlike epoch_sim and friends, POEMS compilers a single standalone executable
for each graph type. The executable can load any instance for that graph
type, but cannot adapt to other graph types. There are two stages to executing
a graph in POEMS:

1 - Compile the graph into an executable using `tools/poems/compile_poems_sim.sh`,
    usually producing an executable called `./poems_sim`.

2 - Execute the resulting executable on a graph instance by running the simulator
    and passing the name of the graph.

For example, to build and execute `apps/ising_spin/ising_spin_8x8.xml`:
```
tools/poems/compile_poems_sim.sh apps/ising_spin/ising_spin_8x8.xml
./poems_sim apps/ising_spin/ising_spin_8x8.xml
```

#### Simulation approach

POEMS is designed to scale across multiple threads of execution, and should have reasonably
good weak scaling up to 64 or so cores - certainly on 8 cores you'll see a big benefit as
long as there are 10K+ devices. The general approach is to decompose the graph into
clusters of devices, where the amount of intra-cluster edges is hopefully quite large.
Messages between devices in the same cluster are _local_ messages, while messages between
devices in different clusters are _non-local_. Each cluster maintains a lock-free bag
(technically I think it is wait-free) of incoming non-local messages, and any thread can
post a non-local message to any cluster. The incoming bag of messages is completely un-ordered,
and in order to make it lock-free and O(1) it is pretty much guaranteed that messages will
always arrive out of order, and are actually quite likely to be eventually processed
in LIFO order.

Each cluster is processed in a round-robin fashion be a pool of threads. The overall
cluster step process is:

1 - If we have just completed an idle barrier, then execute the idle handler on all
    local devices.

2 - Grab the current bag of incoming non-local messages in O(1) time by atomically
    swapping with an empty bag. Other threads can still post to this cluster while this happens.

3 - For each incoming message in the non-local bag, deliver it to the local device,
    destroying the bag as you go. Other threads will be posting into a different bag
    by this point.

4 - For each device in the cluster, try to execute one send handler, or (lower priority)
    execute the device idle handler. If a device sends a message, then deliver any
    local edges by directly calling the send handler. Any non-local edges are handled
    by posting a copy of the message into the input bag of the target cluster.

The stepping process is optimised to try to skip over inactive devices and clusters
in a reasonably efficient way (though it would probably be better to have more
than one level of clusters).

Idle detection is somewhat complicated due to the need to atomically commit on
things amongst multiple threads. The approach used here has two levels:

1 - an initial heuristic level, which tries to guess when all clusters have gone
    idle.

2 - a precise verifical level, where all the threads start blocking until there
    is only one left which can verify that the system is completely idle.

There are probably still some threading bugs left in that process, though it is
usually quite reliable.

There are a quite a lot of optimisations in the system to try to reduce
overheads, so this is less of a simulator and more a genuine execution
platform. Optimisations include:

- Each graph type is compiled directly into a simulator, with no virtual dispatch.
  Wherever possible the compiler is given enough information to allow it to
  eliminate branches. For example, if a device type only has one input handler,
  then that handler will be called for any incoming message without any branches.

- Non-local messages are handled using a distributed pool, so there will be no
  malloc of frees happening once the graph reaches a steady state.

- Properties and state are packed together to increase locality, and to reduce
  number of arguments when calling handlers.

- Clusters are formed using edge information in order to maximise the number of
  local edges.

- Bit-masks are used to reduce the cost of skipping over in-active devices, and counters
  are used to skip in-active clusters. This speeds things up when approaching the
  global idle point, and also makes things faster in graphs with very sparse
  activity.


#### Compiler options

When compiling the poems simulator you can pass a number of options to control output
and optimisation level:

- `-o <file_name>` : Sets the name of the simulation execution. Default is ./poems_sim
- `--working-dir <dir>` : Where to place temporary files from compilation. Default comes from mktemp.
- You can choose one optimisation level:
  - `--release` : Attempt to create fastest possible executable with no safety (default).
  - `--release-with-asserts` : Attempt to create fastest possible executable, but keep run-time checks.
  - `--debug` : Debuggable executable with all run-time checks.

#### Run-time options

When you invoke the simulate you get a number of options to control how it executes:

- `--threads n` : How many threads to use for simulation. The default comes from `std::thread::hardware_concurrency`,
   and will usually equal the number of virtual cores available. The execution engine varies slightly depending
   on the relationship between number of clusters and number of threads; if you see crashes it may be worth running
   with just one thread.

- `--cluster-size n` : Target number of devices per cluster (default is 1024). Performance depends on a complicated relationship
  between topology, application logic, number of threads, and cluster size. Sometimes a smaller cluster works
  better, down to around 32, while sometimes a larger cluster is better. The default of 1024 is on the larger
  end, and works ok for most applications.

- `--use-metis 0|1` : Whether to cluster using metis (default is 1). It is almost always worth clustering the graph,
  even if it is quite irregular, as it tends to increase cache locality. For regular graphs it can greatly increase
  the number of local (intra-thread) deliveries, which makes multi-threaded execution much faster.

- `--max-contiguous-idle-steps n` : How many no-message idle steps before aborting (default is 10). In most applications
  a sequence of idle steps where nothing happens usally means the application has dead-locked or other-wise expired.
  However, some wierd applications may be doing a lot of compute in the idle handler which means there are long gaps,
  so for those applications you'll need to increase this number.

###

## Suggestions on how to write and debug an application/graph

Many of the tools and features in graph_schema are intended to
make the development and debugging of graphs easier, as getting
functional correctness is often so difficult. The general idea
here is to try to get a particular graph working on all possible
platforms, rather than on one particular piece of hardware or one
particular simulator. Proving that a graph is correct under
all possible platforms is the same as proving that the graph
is correct under all possible execution paths, which is often
impossible due to the combinatorial explosion of possible
execution states. However, we can get some confidence that
a graph is correct by running it under many different execution
patterns to look for failures. If we can the replay those
failures it is possible to understand and debug them.

### General approach for writing a graph

While each application is different, a general flow for getting
an application functionally correct is:

1 - Write a reference software model in a sequential language. Trying
    to write a concurrent graph for something you don't understand
    sequentially is usually a waste of time.

2 - Design the set of state machines on paper. Typically you want
    two or three sets of sketches:

    a - A set of state-machine diagrams, capturing the individual
        device types in the diagram. Each state is a bubble, and
        the edges between states represents messages arriving or
        leaving the device. You probably need to annotate the edges
        with:

        - Whether it is a send or receive (often using a different style for
          each is a good idea).

        - Which pin it is sending/receiving on.

        - Any guards which control whether an arc is taken or not.
    
    b - An example topology diagram, showing how a simple graph instance
        would be constructed from those devices. You usually don't want
        more than 20 or so nodes, but you do want something big enough
        that it isn't trivial.
    
    c - (Optional) A state sequence diagram showing one possible evolution
        of your graph instance over time. These are quite time consuming,
        but a partial state sequence diagram can really help with complex
        graphs.

    It is tempting to skip these sketches, but for any reasonably complex
    application you are just wasting time as you'll need to draw them
    later when debugging.

3 - Convert your state machine diagrams into a graph-type. You should be
    extremely liberal with the use of `assert` when doing this, and
    ideally assert device state invariants at the top of every handler.
    If you don't know of any device state invariants, that suggests
    you haven't fully thought through the state machine diagram.

4 - Manually construct a device instance that exactly matches your example
    topology diagram. If you have up to 20 devices it doesn't take that
    long, and even the act of constructing it sometimes shows up problems.
    It may be tempting to construct a generator program at this point,
    but that may be premature - when manually constructing the instance
    you might find problems that invalidate your original design.

5 - Simulate your manual instance using `epoch_sim`. This is the friendliest
    simulator, which tends to expose the simplest bugs. If you get errors
    here then debug them (see later).

6 - Simulate your manual instance using `graph_sim` using a number of
    strategies. It is a good idea to run it for an hour or so with
    the random execution strategy and event logging turned on. If you get
    any errors, then debug the event log (see later). You really want
    to do this with simple graphs if you want a hope of debugging it, so
    the earlier you find problems the better.

7 - (Optional) Run it in hardware. There is little point to going
    to hardware at this point apart from warm fuzzies, but it is nice to do.

8 - Create an instance generator. This should almost always be configurable
    in size/scale, and in most cases the preference should be for a random
    generator rather than ingesting real problems (you can always come back
    to that). If at all possible, you should also design the instance generator
    to embed self-check information in the generator graph. For example, if
    there is a defined "end" for device instances, you can embed the final
    value expected as a property. At run-time each device can assert it is
    correct at the end state, so you find out where the application has failed.

9 - Check that the application works at a bunch of scales. Think 2^n+-1, primes,
    and so on.

10 - Move to hardware! There is still a good chance it won't work it
     hardware, but at least any errors will be obscure and hard to find.

### Debugging applications

You have three main tools in your debugging tool-box:

1 - Assertion failures: Try to make the application assert as soon as possible.
  As noted above, you want to assert any possible program variants you can
  at the start of each handler. If necessary/possible, augment your device states and/or
  messages to carry extra debug information that allows you to assert stronger
  invariants. The fewer send and receive steps that happen before an assertion,
  the easier it is to work out why it is happening.

2 - Event logs: Some of the graph_scema tools allow you to capture the set of events
  within a failing program. You can then use them to try to find out what went wrong
  by looking at the messages sent and the state changes. As with assertions, you 
  want to try to find the shortest event log that will find an error - more than
  about 100 events becomes very difficult to follow.

3 - Handler logs: print out information about the internal state of devices
  to try to recover what went wrong. This is a powerful technique for simple
  bugs, but a weak technique for complex bugs.

If you're more dedicated/desperate you can also bring to bear some more advanced methods:

4 - Checkpointing against predicted device state at key points. There used to
    be support for this in graph_schema, but it was removed as part of the
    standardisation for v3 (probably a mistake).

5 - Model checking. There is a tool called 'bin/hash_sim2' which will perform model
    checking of a graph using a depth-first search. While this tool can be very
    useful, you have to deal with the standard model checking problems of state
    explosion. Probably you'll need to adapt your graph to reduce the state-space
    and encourage merging of states, and you might need to hack the simulator a bit.

5 - Formal methods. Not discussed here.
