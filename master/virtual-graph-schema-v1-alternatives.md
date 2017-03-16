 # Alternative graph formats

 Most graph description formats are mainly interested
 in topology, with structure and format being secondary.
 So here we briefly consider how other graph formats would
 do the same thing.

 Consider describing the following graph, containing
 two devices n0 and n1 of type "cell":

 ````
                        distance=2
                          :
        +--------------+  :       +--------------+
        |      in      |<---------|      out     |----
        |--------------|          |--------------|     \
     n0:|  weight=2.0  |       n0:|  weight=1.0  |     |
        |--------------|          |--------------|     /
        |      out     |--------->|      in      |<----
        +--------------+        : +--------------+  :
                                :                   :
                                distance=3        distance=1
 ````

 In the proposed format it would be:
 
 ````
   <GraphInstance>
    <DeviceInstances>
     <DevI id="n0" type="cell">
      <P>"weight":2.0</P>
     </DevI>
     <DevI id="n1" type="cell">
      <P>"weight":2.0</P>
     </DevI>
    </DeviceInstances>
    <EdgeInstances>
     <EdgeI path="n1:in-n0:out">
      <P>"distance":2.0</P>
     </EdgeI>
     <EdgeI path="n0:in-n1:out">
      <P>"distance":3.0</P>
     </EdgeI>
    </EdgeInstances>
   </GraphInstance>
 ````

 The absolute minimum required for a device instance is:
 
 ````
    <DevI id="?" type="?" />
    <DevI id="?" type="?"><P>?</P></DevI>
 ````

 So all instances require `22+len(id)+len(type)` characters with no properties,
 or `34+len(id)+len(type)+len(properties)` with properties.

 For an edge type the minmum is:

 ````
    <EdgeI path="?:?-?:?" />
    <EdgeI path="?:?-?:?"><P>?</P></EdgeI>
 ````

 So `20+len(dstId)+len(dstPort)+len(srcId)+len(srcPort)` or
 `35+len(dstId)+len(dstPort)+len(srcId)+len(srcPort)+len(properties)`.
   
 ## <a href="http://graphml.graphdrawing.org/">GraphML</a>

 GraphML allows one to describe graph instances in a
 fairly flexible way, and supports most of the things
 that we need:
 - ports on nodes
 - properties on edges
 - properties on nodes
 - it has a schema for validating (XSD)

 There is an extensibility mechanism due to the use of
 XSD, so we could also add in a mechanism for supporting
 graph types. Alternatively, we could use some other
 format to describe the graph types, then use GraphML
 to describe the instances. We'd still have to specify
 the other instances.

 The main argument against GraphML is space and parsing
 speed. For example, ignoring constant overhead parts, here
 is the main part of a graph in GraphML with two nodes
 and two edges:

 ````
     <node id="n0">
      <port name="in"/>
      <port name="out"/>
      <data key="deviceType">cell</data>
      <data key="weight">1.0</data>
     </node>
     <node id="n1">
      <port name="in"/>
      <port name="out"/>
      <data key="deviceType">cell</data>
      <data key="weight">2.0</data>
     </node>
     <edge source="n0" sourceport="out" target="n1" targetport="in">
      <data key="distance">2.0</data>
     </edge>
     <edge source="n1" sourceport="out" target="n0" targetport="in">
      <data key="distance">3.0</data>
     </edge>
 ````
 
 It is a bit bigger than the POETS form, and requires more work
 from the XML parser per device or edge read.
     
 We are also able to trivially import/export GraphML
 instances if we ever need to (though only the topology).

 ## Spinnaker (UIF)

 UIF supports all the features we want, as it is a container
 format like XML or JSON. Any structural requirements are
 going to have to be enforced through load-time validation,
 as there is no schema language. So the graph structure and
 semantics will need to be specified by English and example,
 and presumably with a reference validator.

 There are existing well-tested C++ parsers for the format
 with error checking. Presumably other languages are supported
 as well?

 Unlike the other formats mentioned here, there is a format
 for describing device types, based on positional parameters.
 The existing format uses input ports, but can be extended to
 output ports as well.

 ````
    *device : cell(%f) 
    *input  : in(%f)
    *output : out
    n0      : cell(2.0)
    n1      : cell(2.0)
    n0(out) : n1(out(2.0))
    n1(out) : n0(out(3.0))
 ````

 This doesn't make use of prototype/default parameters, so
 could be shorter.

 The concept of MetaData as used here is incorporated with
 the use of "invisible types", which is data that is truncated
 off by the loader.

 The obvious advantage of this format are:
 - extreme terseness.
 - fast parsing (presumably?).
 - existing knowledge.
 - limitless flexibility.
 - very effective for connection-oriented parts of data.

 The disadvantages are:
 - not very human readable for graph definitions. Ok for graph
   instances once you know what they look like.
 - leads to a flat structure for data-types (while it supports nested
 - data-types, they are mostly structural rather than named).
 - Unclear portability across languages (also, is there a formal grammar?)
 - Structural checking up to the parser
 - Difficult to embed larger amounts of data such as code (though it
 - can be done).

 ## <a href="https://en.wikipedia.org/wiki/Graph_Modelling_Language">Graph Markup (or Meta) Language</a>

 This is the file format mainly used by Gephi (I think). It
 is quite a nice simple format, and relatively easy to
 parse. However, it doesn't appear to support ports, so
 we would need to fake attachment ports for edges. A
 rough comparison of the previous graph would be:

 ````
    node
    [
     id n0
     type "cell"
     weight 2.0
    ]
    node
    [
     id n1
     type "cell"
     weight 2.0
    ]
    edge
    [
     source n0
     sourcePort "out"
     target n1
     targetPort "in"
    ]
    edge
    [
     source n1
     sourcePort "out"
     target n0
     targetPort "in"
    ]
 ````

 In this example the "sourcePort" and "targetPort" properties
 are not natively understood in GML world, and would be
 discarded by most readers.

 This format is actually pretty clean, and due to it's line-oriented
 nature it is fairly easy to parse. It is pretty verbose
 when compared to XML formats, while being much easier to
 understand than UIF. There are also a number of parsers
 already in existence, and it gives us compatibility with
 Gephi.

 As with GraphML, this is still mainly dealing with topology,
 so any types either have to be forced into this format,
 or handled in a different file.
 But in the same way as GraphML, there is nothing stopping
 us importing or exporting the graph instances from this
 format.
