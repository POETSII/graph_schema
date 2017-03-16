### Why not use Schematron, XML Schema (xsd), or DTD?

The compact version of RELAX NG is not itself in
XML, and so is easier to read than Schematron or
XSD. Schematron is particularly difficult for those
who don't know XPath, while XSD is quite verbose.
In comparison, anyone who has done a compilers course
can understand BNF, and very little knowledge is
needed to interpret the schema. It is also possible
to losslessly convert RELAX NG to XSD, so we can
take advantage of validating XML parsers such as
Apache Xerces.

Disadvantages are that RELAX NG is not quite as well
supported by IDEs as XSD, but this is unlikely to
matter as we don't need to edit the schema much.
Another disadvantage is that tooling for RELAX NG
is a little old, but that is because it is essentially
mature, as the <a href="http://www.thaiopensource.com/relaxng/trang.html">Trang</a> toolkit has been stable
for a long time.

### Why not just use YACC to define a grammar?

This comes back to the question of "why not use a true DSL?".
If everyone uses C++ then a YACC parser + library is
fine, but it is problematic as soon as you try to
load it in any interpreted language. Languages that
are currently in active use are Python and Typescript/Javascript
and it is very likely that we need to round-trip,
rather than just generating output. YACC is not cross-language,
so we would have to write parsers and validators for
each language in whatever parser toolkit seems best.

### Why can device and edge instances not be interleaved?

There is a deliberate design decision which splits devices and
edges into distinct sections:

    <DeviceInstances>
       <DevI ... />
    </DeviceInstances>
    <EdgeInstances>
       <EdgeI ... />
    </EdgeInstances>
    
This could have been enforced by sequencing in the schema, but
making it explicit means accidental interleaving is less likely
to happen.

There are arguments for and against interleaving, but when
it comes down to it they are mainly todo with the balance
between ease of use for the person creating the graph,
and ease of use for the person consuming the graph. When
creating the graph it is often useful to emit some of
the edges associated with the node at the same time as
the node itself, but this makes life harder for the
consumer as they need to create edges which connect
to unknown nodes. If we were to see the following:

    <DevI id="a" type="devA" />
    <EdgeI path="a:x-b:y" />
    <DevI id="b" type="devB" />

Then at the point that the edge "a:x-b:y" is encountered
we don't know what the type of `b` is. If one is building
a minimal data-structure of pointers, we can't allocate
`b` until that point, so we need to maintain a dictionary
of unconnect pointers. Obviously there are ways of dealing
with this, but if we want a simple loader and a low memory-footprint
consumer it is better to have all devices known before the edges
are encountered.

The complexity on the generation side could be handled
by just generating two data-streams, one of which
contains the devices, the other of which contains the edges.
These can then be concatenated together with the surrounding
XML very efficiently using byte copies.

### Why use JSON in the middle of the XML?

Properties are embedded as JSON fragments, so we might say
an edge instance like this:

    <EdgeI id="a:in-b:out"><P>"x":0.5,"y":[1,2,3],"z":{"a":4.0,"b":6}</P></EdgeI>

This corresponds to JSON properties looking like this:
   
    {
        "x":0.5,            // a single number called x
        "y":[1,2,3],        // an array called y
        "z":{"a":4.0,"b":6}     // a tuple called z
    }        

However, we will need to invoke a JSON parser to get that data out
of the string.

A possible XML representation would be:

    <EdgeI id="a:in-b:out"><P>
        <S n="x">0.5</S>
        <A n="y">1 2 3</S>
        <T n="z">
            <S n="a">4.0</S>
            <S n="b">6</S>
        </T>
    </P></EdgeI>

One avantage is that the JSON is more compact, but another
advantage is that keeping the data as a string reduces the
XML parsing complexity. The JSON version requires one element
open, one text string, and one element close, regardless
of how many properties there are. The XML version requires
two elements, one attribute, and one text string for every
element, significantly increasing memory usage in the
DOM, and increasing overhead in a SAX parser.

A natural response is that the overhead is just moved
around, from XML parser to JSON parser, which is true.
However, there are still advantages:

- We at least gain the ability to shift parsing the JSON
  onto a different thread from the XML parser.

- JSON parsers tend to be more lightweight in memory
  footprint, and can be faster due to the simple grammar
  as JSON is context-free while XML is context-sensitive (Though
  in practise there is not much speed difference).

- If we don't want to look at some JSON data we can
  just skip it. For example, we can skip over meta-data
  elements without bothering to parse the data into JSON.

These arguments may not be compelling, but to the 
designer they made a compelling case. Disadvantages
are:

- You now need two parsers. However, JSON is easily available
  in all modern languages.
  
- Increased cognitive load. This is mainly a problem for the
  people writing parsers and serialisers though, less so
  for most users.

### Why use speech marks in JSON property names?

The JSON spec says that we have to do this:

    {
        "x":0.5,            // a single number called x
        "y":[1,2,3],        // an array called y
        "z":{"a":4.0,"b":6}     // a tuple called z
    }        

rather than this:

    {
        x:0.5,            // a single number called x
        y:[1,2,3],        // an array called y
        z:{a:4.0,b:6}     // a tuple called z
    }        

Some parsers will accept unquoted property names, but it is not
supported cross-platform.

### Why not use JSON rather than XML?

Given JSON is already used in the format, and has many similar
features to XML, we could choose to do the whole thing in
JSON. Problems that arise with JSON are:

- Reduced error-checking, as mis-matched elements are not
  obvious: a `}` will close any object. This becomes particularly
  problematic with massive files.
  
- Better tooling for XML, such as schema definitions and stylesheets.
  JSON has been gaining these things, but is much behind XML.

- Poor extensibility and maintenance story due to the lack of
  namespaces. 

### Why put the handlers in the files?

Embedding the event handlers in the files is slightly awkward, as
it requires a `CDATA` section, 
