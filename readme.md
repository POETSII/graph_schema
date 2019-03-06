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
the same application-level results independently of the orchestrator. Models that are
*Asynchronous* do not rely on any kind of global clock - note that
global clock refers to something created by the application, not
the orchestrator.

- [apps/ising_spin](apps/ising_spin) : *Deterministic*, *Asynchronous* : This is an asynchronous ising spin model,
  which models magnetic dipole flipping. Each cell moves forwards using random
  increments in time, with the earliest cell in the neighbourhood advancing.

- [apps/gals_heat](apps/gals_heat) : *Deterministic*, *Asynchronous* : This models a
  2D DTFD heat equation, with time-varying dirichlet boundaries. All cells within
  a neighbourhood are at time t or t+1 - once the value of all neighbours at time t is
  known, the current cell will move forwards.

- [apps/gals_izhikevich](apps/gals_izhikevich) : *Asynchronous* : This is
  an Izhikevich spikiung neural network, using a loose form of local synchronisation
  to manage the rate at which time proceeds. However, it is not very good - it
  does lots of local broadcasting (whether neurons fire or not), and it doesn't
  work correctly with an unfair orchestrator.

- [apps/clocked_izhikevich](apps/clock_ishikevich) : *Deterministic* : Also
  an izhikevich neural network, but this time with a global clock device.
  The whole thing moves forward in lock-step, but needs 1:n and n:1 communiciation
  to the clock.

- [apps/clock_tree](apps/clock_tree) : *Deterministic* : A simple benchmark, with three devices:
  root, branch, and leaf. The root node is a central clock which generates ticks.
  The ticks are fanned out by the branches to the leaves, which reflect them back
  as tocks. Once the clock has received all the tocks, it ticks again.

- [apps/amg](apps/amg) : *Deterministic* : A complete algebraic multi-grid solver,
  with many types of nodes and quite complex interactions. It should be self-checking.

Currently all applications are designed to meet the abstract model, which means
that messages can be arbitrarily delayed or re-ordered, and sending may be
arbitrarily delayed. However, they are currently all interolerant to any
kind of loss or error - AFAIK they will all eventually lock-up if any message
is lost or any device fails.

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

### bin/epoch_sim

This is an epoch based orchestrator. In each epoch, each device gets a chance to
send from one port, with probability `probSend`. The order in which devices send
in each round is somewhat randomised. All devices are always ready to recieve, so
there is no blocking or transmission delay.

Example usage:

    make ising_spin_provider
    bin/epoch_sim apps/ising_spin/ising_spin_16x16.xml

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
