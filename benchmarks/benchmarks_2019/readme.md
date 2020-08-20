Benchmarks / tests
==================

This directory contains a number of pre-generated XML files which can be
used for testing. Each directory represents a class of graph, with each
class representing a particular type of application instantiated for some
increasing scale parameter S. The relationship between S and the number
of devices, edges, and run-time will very, but both the size of the graph
and number of messages sent should always increase with increasing S.
The values of S are also intended to be somewhat evenly spaced, so are
expected to be used on the x-axis of graphs.

Each graph is self-checking to some extent, and at the very least knows when
it has "finished". Some graphs also self-validate against pre-calculated results,
though others only do sanity checks on the protocol and value. If a graph
finishes correctly it will call:
```
handler_log(0,`_HANDLER_EXIT_SUCCESS_9be65737_`);
```
so you can grep for that to check that it finished correctly.

If a graph knows it has failed it some way, it will call:
```
handler_log(0,_HANDLER_EXIT_FAIL_9be65737_);
```
Failure may be because of protocol problems, or because the final values are
incorrect. Graphs usually also contain many asserts to look for invalid
protocol states or values.

When executing a graph in an environment the following statuses are defined:

- `success` : The graph explicitly reported success through `_HANDLER_EXIT_SUCCESS_9be65737_`.

- `fail` : The graph explicitly reported failure through `_HANDLER_EXIT_FAIL_9be65737_`.

- `error` : Some kind of problem occured in compilation or execution.

- `timeout` : Executing the graph exceeded the time budget for execution.

- `timeout(transitive)` : A graph at a smaller scale timed out, so this graph was skipped.


The files that can appear are:

- `{CLASS}/` : Each directory represents a class of graph

- `{CLASS}/{CLASS}_s{S}.*` : All the files relating to the particular class at scale S,
  where S is a decimal integer.

- `{CLASS}/{CLASS}_s{S}.v3.xml.gz` : Version 3 instance of the XML, gzipped up.

- `{CLASS}/{CLASS}_s{S}.v4.xml.gz` : Version 4 instance of the XML, gzipped up. This _should_
  be structurally and functionally identical to the v3 version.

- `{CLASS}/{CLASS}_s{S}.base85.xml.gz` : Base 85 (version 4) instance of the XML, gzipped up. This _should_
  be structurally and functionally identical to the v3 version.

- `{CLASS}/{CLASS}_s{S}.v3.static_properties.json` : Some summary statistics for the graph, which
    describes numbers of devices, edges, degree distribution, edge type count, and so on. It is
    calculated from the v3 xml, but should be the same for v4.

- `{CLASS}/{CLASS}_s{S}.v3.epoch_sim.status` : The status of simulating the v3 graph with epoch_sim
    (i.e. an in-order simulator).

- `{CLASS}/{CLASS}_s{S}.v3.graph_sim.status` : The status of simulating the v3 graph with graph_sim
    (i.e. an out-of-order randomised simulator).

- `{CLASS}/{CLASS}_s{S}.v3.graph_sim.status` : The status of simulating the v4 graph with `tools/pyparser/simulate.sh`.
    This is very slow, and mainly intended to sanity check v4 graphs at small scales.

Benchmark class
===============




