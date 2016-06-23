graph_schema
============

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

There are a few parts to the repository:

- 


Requirements
============

This needs python3 and lxml.

    

Usage
=====

For a top-level check, do:

     make validate-virtual

This should check the schema, validate all examples/test-cases
against the schema, and also validate them by parsing them
and generating graphs.
