graph_schema
============

![Heat equation](rendered/gals_heat_128.gif)

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

Requirements
============

This needs python3.4, and python libraries lxml, and pillow.

It also needs c++11, and libxml++-2.6.

I haven't fully tried this on non OS X platforms, but it has worked
in the past in both Cygwin and Linux. TODO : provisioning scripts.

Schema
======

The structure of the application format is captured in a relax ng xml schema, which
checks a fair amount of the structure of the file, such as allowed/missing elements
and attributes. The schema is available at [master/virtual-graph-schema.rnc](master/virtual-graph-schema.rnc).

For checking purposes, the schema can be validated directly using `tring`, or
is also available as an [xml schema (.xsd)](derived/virtual-graph-schema.xsd)
which many XML parsers can handle.

To validate a file `X.xml` you can ask for the file `X.checked`. This
works for graph types:

    make apps/ising_spin/ising_spin_graph_type.checked

and graph instances:

    make apps/ising_spin/ising_spin_16_16.checked

If there is a high-level syntax error, it should point you towards it.

A slightly deeper check is also available:

    python3.4 tools/print_graph_properties.py apps/ising_spin/ising_spin_16x16.xml

This checks some context-free aspects, such as id matching. Beyond this level
you need to try running the graph.

Usage
=====

For the top-level check, do:

    make test

This should check the schema, validate all examples/test-cases
against the schema, and also validate them by parsing them
and simulating them.

To look out at output, do:

    make demo

This will take longer and use more disk-space. It will
generate some sort of animated gifs in the `apps/*` directories.

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

Currently all applications are designed to meet the abstract model, which means
that messages can be arbitrarily delayed or re-ordered, and sending may be
arbitrarily delayed. However, they are currently all interolerant to any
kind of loss or error - AFAIK they will all eventually lock-up if any message
is lost or any device fails.

Tools
=====

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

