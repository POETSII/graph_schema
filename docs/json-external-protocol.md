

Assumptions & Definitions
--------------------------

This document tends to view things from the point of view of externals,
so certain naming conventions have a bias. However, it is also
defines the behaviour of orchestrator-style components like simulators.

### Clients and servers

For the JSON protocol we do not assume full network routing,
and in particular do not allow routing graphs. As a consequence,
in each connection there will be two partners:

- server : the thing which is connected to. If working with numeric
  adress, this will control mapping of ids to addresses. The server
  is always an active component.

- client : the thing which connects in, and (if it wants them) receives
  mappings of ids to addresses. A client may be a passive component (e.g. a
  stream from or to a file), or an active component.

A server may have many client connections, and if necessary will
route messages between clients. In principle, a component could
act as a client on one connection, and a server on another, as
long as there is a strict tree topology (no cycles!). However,
we will explicitly not consider this for now:

> **This document only considers star topologies, with the server at the centre, and clients round the edge.**

A message which is moving from a client to a server is moving _upwards_
(i.e. up the tree to the root), while the other direction is _downwards_
(down the tree towards leaves).

From the point of view of a given client, devices can be classified
as two types: _local_, or _non-local_. For non-local devices the client
doesn't really care whether they are in the server or in a different
client - either way they are somewhere else on the other side of a channel.
We can similarly classify devices as local or non-local from the point
of view of the server.

### Uni-cast and multi-cast

A POETS message on the wire might be uni-cast or multi-cast:

- multi-cast (source routed) : a message which is source-routed has not yet
  been fanned out according to the edges in the graph instance. A single
  multi-cast packet might eventually result in the delivery of zero or
  more uni-cast packets.

- uni-cast (destination routed) : a destination-routed message represents
  a single message which must be delivered to a single address. Each uni-cast
  message corresponds to some original multi-cast message plus an edge
  instance from the multi-cast message source to the uni-cast message's dest.

If we combine the two directions and two types of messages, then we end
up with four combinations travelling over a channel :

- **upwards+multi-cast** : The multi-cast message originated in the client,
  and the server is requested to fan it out to other clients. On receiving
  the message the server will deliver to any server-local destinations,
  and create downwards + multi-cast messages to any non-local devices
  residing in clients. To simplify the logic, the server will also send
  a downwards+multi-cast message to the original client - if the client
  decided to deliver locally, they will need to include logic to ignore
  the repeated message. It is not required that there exist any non-local
  destinations for the message, but it is reccommended that clients do not create
  upwards + multi-cast messages if there are no non-local clients.

- **downwards+multi-cast** : The multi-cast message either originated in the
  server or in a client (possible this client), and the client should fan the message
  out to any local destinations. It is not required that there exist any
  local destinations for the message, but it is reccommended that servers
  only send messages to clients where there is a local client.

- upwards + uni-cast : The message represents a single message to a non-local
  device. These are not currently needed or supported at the channel level

- downwards + uni-cast : The message represents a single message to a local
  device. These are not currently needed or supported at the channel level.

### Interaction styles for external devices

We can classify external devices in three ways:

- Source: the external device only has output ports.

- Sink: the external device only has input ports.

- Bi-directional: the external device has both input and
  output ports, but there is no dependency between the
  messages that go into the input port and the messages
  that come out of the output port.

- Interactive: the external device has both inputs and outputs,
  and there is a dependency of some sort between the messages
  going into the device and the messages coming out.

This classification is made based on the device type and handler logic,
rather than on the connections made in a particular graph instance. All devices
except for interactive can be handled by dumb input and output
streams, and it may be possible to model an interactive device
using naive input/output streams too. But sufficiently complicated
interactive devices must be modelled by a genuine piece of compute,
if only to make sure that certain output messages are not produced
until required input messages are received.

### Establishing the local device set

Multiple client connections might be established to a server, with each
client managing a different set of external devices. When the server starts
it does not always know who is going to manage a particular external
device, nor in some cases how many client connections there might be. To
allow the server to map devices to clients, the clients must establish
a local-set at start-up, before the messages start flying.

In every case a client has to establish the set of devices that it
is managing, even if it is not a full interactive device. That means
that even for a pure sink device the client has to send some information
in to establish the connection, or the server must provide some explicit
or implicit means for establishing who is managing what (e.g. command
line parameters). 

Protocol
--------------

### Framing

The overall framing uses JSON-RPC framing protocol. In order to
allow for possible transports that are not really bidirectional (e.g. HTTP),
we'll stick with client to server calls. The notion of JSON-RPC client and server
map directly to our notions of client and server, regardless of which side intiated
the connection at the transport level.

In the examples we'll use the following conventions:
- `-->` : Message from client to server
- `<--` : Message from server to client
- `  # something` : Comments for expositions. Note that these are not valid in JSON (JSON has no comments)

#### Stream transport framing (TCP/files/pipes)

JSON-RPC doesn't say anything about how JSON objects are moved between client
and server, but a very common approach is to send objects one after another
down a byte-stream. For efficiencies sake in certain line-oriented clients
we **require** that objects are terminated by a new-line `\n`. Additional
white-space could occur between objects in the stream, and there could be
internal line-feeds and whitespace within objects, but there must be
at least one new-line following each object. The following new-line should be
flushed in order to consider the preceding object flushed.

Another problem with stream transports is that in many clients it is very
difficult to detect the difference between "no data ready" and "end of
stream" in a non-blocking way (looking at you, python). As a consequence,
stream transports **should** be terminated with a single literal  string `"eof"`.
This string is not part of JSON-RPC, and simply guarantees that no more
data will come down the channel. Both ends of a channel should attempt
to write `"eof"` down the channel whenever terminating the stream (whether
due to successfull or errored termination), as it gives the other end the
best chance of terminating without hanging or waiting for a timeout.

### Security

The protocol is not designed to provide security, so this must be provided at the channel
layer:
- All inter-machine connections should be wrapped in a secure encrypted
    channel such as SSL, or tunneled over a secure channel such as SSH or VPN.
- Inter-machine connections should perform authentication when the channel is
    established. The "owner" mechanism in the protocol should not be considered
    a security measure.
- No client or server should allow non-local un-authenticated connections. Opening
    short-lived local sockets and web-servers is fine, but these should always bind to
    loopback (`127.0.0.1`) and *not* an open address (e.g. `0.0.0.0`).
- Where possible, direct passing of unix sockets is preferred to listening using
    TCP or other protocols.
- The `owner_cookie` parameter of `bind` should *not* be a password. It is intended
    to allow discrimination between connections, graphs, and users, and is not
    intended to be secure.

### States

Each connection goes through the following states:

- CONNECTED : the channel has been established, but connect has not been called.

- BOUND : bind has been called to establish owned devices, but run has not yet been called.

- RUNNING : run has been called, and messages can now flow via send/poll.

- FINISHED : execution has finished due to a halt.

- ERRORED : some situation has caused the channel to move into an unrecoverable error state. No further
    calls can be made, and probably any in-flight calls will fail eventually.

### Procedures

- `bind` : (CONNECTED->BOUND) Establishes the connection, and tells the server which devices
  it is managing. The server will respond with a set of incoming edges for
  that set of devices.

- `run` : (BOUND->RUNNING) Used to indicate that the client has finished initialising; once
  this call completes the client can start using the 

- `send` : (RUNNING) Inject one or more upstream+multicast messages

- `poll`: (RUNNING->[RUNNING,FINISHED]) Mainly used to get downstream+multicast messages, plus potentially
  other types of events such as halt.

- `halt` : (RUNNING) Stops the execution of the devices, and will disconnect this and all
    other externals. The connection is not officially finished until the server returns a halt
    message via `poll`.

All state transitions are from the point of view of the server, as the client has
imperfect knowledge of state when multiple messages are in flight.

Any call could lead to the connection transitioning to the ERRORED state.

### Standard errors

- `-1` : ConnectionFinished : this method failed because the connection has previously moved into the FINISHED state.

- `-2` : ConnectionErrored : this method failed because the connection has previously encountered an unrecoverable error.

- `-3` : InvalidDevice : a method referenced a device which is not known to the server.

- `-4` : InvalidEndpoint : a method referenced an endpoint which does not appear to exist.

- `-5` : InvalidDirection : a method used an endpoint in the wrong direction (e.g. send from an input endpoint).

- `-6` : GraphTypeMismatch : client tried to connect to a graph that doesn't match what the server has.

- `-7` : GraphInstanceMismatch : client tried to connect to a graph instance that doesn't match what the server has.

- `-8` : GraphInstanceMismatch : client tried to connect to a graph instance that doesn't match what the server has.

- `-9` : InvalidOwner : The owner passed to `bind` is invalid or unknown.

- `-10` : InvalidCookie : The owner_cookie passed to `bind` is invalid or unknown.



### Shared types

We will use the following types as short-hand for the procedures:

- `endpoint : str` : a string containing the name of a device and the name of a port, seperated by a colon. No whitespace is allowed.

- `message : { "src":endpoint, "data"?:JSON, "type"?:"msg" }` : a source-routed multi-cast message. `src` is the endpoint where the
    message originated, and data is a delta against the default values. The data member does not need to be
    present if the message has no payload, or if the message has all default values.

- `event : { "type"?:str, ...}` : A generic event that has occured. The default value for "type" is "msg", so any event without
    a type is implicitly a message. Defined events are:

    - `message : { "type"?:"msg", "src":endpoint, "data"?:JSON }` : A message event...

    - `halt : { "type":"halt", "code":int, "message"?:str }` : Event received when execution has been halted.
  

### `bind` : Bind a connection to a set of devices

The parameters will be a dictionary, with the following fields:

- "magic" : `str` must contain the string `"POETS-external-JSON-client"`.

- "owner" : `str` A string giving the owner (in an orchestrator sense).

- "owner_cookie" : `str` An optional cookie used to disambiguate connections. This is orchestrator
    specific. This is _not_ a password, and any security-based authentication
    will be applied at the channel level. Default is None if field is not present.

- "graph_type" : `str` A string stating the graph type the client is trying to connect
    to, or "*" if it can connect to anything. Default is "*" if field not present.

- "graph_instance" : `str` A string stating the graph instance id the client is expecting
    to connect to, or "*" if it can connect to anything.  Default is "*" if field not present.

- "owned_devices" : `[ str ]` an array of strings, giving the device ids that the client owns.

The result will be a dictionary, with the following fields:

- "magic" : `str` must contain the string `"POETS-external-JSON-server"`.

- "graph_type" : `str` Name of the graph type.

- "graph_instance" : `str`  Name of the graph instance.

- "incoming_edges" : `{ endpoint : [ endpoint ] }`   A mapping from source endpoints to all destination
    endpoints on owned devices. The set of edges will only include those where a destination is
    in the managed set. If there are edges between two managed devices, these will also be included
    in the set.

Possible errors:

- Graph type mismatch
- Graph instance mismatch
- Device is already owned by another connection
- Device does not exist in graph

#### Example

```
--> {"jsonrpc":"2.0", "id":"id5", "method":"bind", "params":{
      "magic":"POETS-external-JSON-client",
      "owner":"dt10",
      "graph_type":"example_type",                                   # Only graphs of this type
      "graph_instance":"*",                                          # Bind to any graph instance
      "owned_devices":["sensorA","actuatorB","controlC","controlD"]
    }}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{
      "magic":"POETS-external-JSON-server",
      "graph_type":"example_type",
      "graph_instance":"example_instance_1",
      "incoming_edges":[
        "sensorA:out" : [ "controlC:in" ],
        "deviceX:update" : [ "controlC:wibble", "controlD:wobble"],   # Multiple recipients of one source
        "controlC:valve" : [ "actuatorB:valve" ]                      # Connection within client
      ]
    }}
```

### `run` : Complete initialisation

This function call tells the server that the client has finished initialisation,
and is ready to start sending and receiving messages.

#### States

Only valid in BOUND state, and causes a transition to the RUNNING state on completion.

#### Parameters

There are currently no defined parameters.

#### Results

A successful result returns nothing.

#### Errors

Possible errors:
- User abort
- Server-imposed timeout while waiting for other externals

#### Example

```
--> {"jsonrpc":"2.0", "id":"id5", "method":"run"}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{}}     # Note that empty result is needed for successful response
```

### `send` : Send one or more upstream+multicast messsages

This procedure sends a bundle of messages to the server for delivery. There is
no ordering on delivery of the messages within a bundle, nor does completion of
the call mean that the messages have actually been delivered. The only guarantee
is that each message will _eventually_ be delivered.

#### Parameters

- "messages" : `[ message]`  A set of messages that should be delivered at some point. 

#### Results

A successful result returns nothing.

#### Errors

Possible errors
- User abort
- Halted

#### Example

```
--> {"jsonrpc":"2.0", "id":"id5", "method":"send", "params":{
      "messages" : [
        { "src":"dev01:out",  "data":{ "wibble":10 } },
        { "src":"dev02:out",  "data":{ "wibble":20 }, "type":"msg" },  # It's legal (though pointless) to explicitly tag event as a message
        { "src":"dev12:ping", "data":{ "vals":[1,2,3,4] } },
        { "src":"dev15:plop" }                                         # Data can be omitted
      ]
    }}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{}}
```



### `poll` : Retrieve messages or other events that have occurred.

In the RPC mode; it is necessary for clients to request messages,
as there must be a request in flight to allow for a response containing
the messages. The set of things that can be returned is more general 
than just messages, though currently the only other defined event is the
halt event.

To cater for channel delay, clients may issue asynchronous
polls, which means the server can hold the call open until messages
are available. However, not all servers are guaranteed to support this,
and they may choose to complete asynchronous calls synchronously.
If a server does choose to complete such calls asynchronously, it should
complete asynchronous calls in the order in which they were issued. If a client
mixes asynchronous and synchronous calls, then the server is allowed to
complete all outstanding async calls before responding to the synchronous
one, or it can service the synchronous call while leaving the async
calls open.


There is no particular guarantee on event ordering, either within the
calls to poll, or between calls to poll. So it is possible that one
call to poll might contain an event that was caused by an event that
is not received until a later call to poll. If such things are important,
then the application needs to inject time-stamps or use other mechanisms
to resolve ordering. That said, servers are encouraged to deliver events
in a way that respects event timing and causality where feasible.

The halt event has a particular meaning, as the halt event means that
there will be no more events produced by the system (nor can new messages
be sent to the system). The halt event may appear in the result along with
other events, or may appear by itself. The halt event will cause all
outstanding asynchronous calls to be flushed, and any future calls to
`poll` (whether synchronous or async) will fail.

#### Parameters

- "async" : `bool=false` If false, then the call should return immediately with 
    the currently available events.
    If true, then the server _may_ hold the request open and continue to process
    other requests. In this scenario the response to the `poll` request will be
    delivered at a future point, possibly inserted amongst other responses.

- "max_events" : `int=0` Allows for a cap on the number of events received. If this
    is missing or 0 then the number of events is not limited.

#### Results

- "events"? : `[ event ]` An array of zero or more events. The order of events in the
    array does not need to correspond to real-time or causal ordering. If there are no
    events then this field may be omitted.
  
- "halt"? : {  }


#### Errors

- Halted (If the system had previously delivered halt)

#### Example

Synchronous request:
```
--> {"jsonrpc":"2.0", "id":"id5", "method":"poll"}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{
    "events" : [
      { "src":"dev001:out", "data":{ "val":5 } }
    ]
  }}
```

Synchronous request with limit:
```
--> {"jsonrpc":"2.0", "id":"id9", "method":"poll", "params":{
    "max_messages":3    # Set a limit on the number of events in the respons
  }}
<-- {"jsonrpc":"2.0", "id":"id9", "result":{
    "events" : [
      { "src":"dev001:out", "data":{ "val":5 } },
      { "src":"dev002:out", "data":{ "val":9 }, "type":"msg" },         # Allowed, but redundant to indicate event is a message
      { "type":"halt", "code":-1, "message":"Pre-condition failed." }   # Halt message is delivered, will move to halt state
    ]
  }}
--> {"jsonrpc":"2.0", "id":"id10", "method":"poll" }
<-- {"jsonrpc":"2.0", "id":"id9", "error":{           # Following calls to poll will fail
      "code":-1, "message":"ConnectionFinished : this method failed because the connection has previously moved into the FINISHED state."
  }}
```

Asynchronous polls with interleaved send:
```
--> {"jsonrpc":"2.0", "id":"id5", "method":"poll", {   # First request
    "async":true
  }}
--> {"jsonrpc":"2.0", "id":"id6", "method":"poll", {   # Second request
    "async":true
  }}
--> {"jsonrpc":"2.0", "id":"id7", "method":"poll", {   # Third request
    "async":true
  }}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{           # Response to first request
    "events" : [
      { "src":"dev001:out", "data":{ "val":5 } }
    ]
  }}
--> {"jsonrpc":"2.0", "id":"id8", "method":"poll", {   # Fourth request
    "async":true
  }}
--> {"jsonrpc":"2.0", "id":"id9", "method":"send", {   # Send interleaved with poll
    "messages":[ { "src":"ext0:out", "data":{ "x":1 } }  ]
  }}
<-- {"jsonrpc":"2.0", "id":"id6", "result":{           # Response to second request
    "events" : []           # It's legal (though sub-optimal) to return no messages for async
  }}
  # Remaining responses elided
```



### `halt` : Request termination of the computation

The halt procedure allows an client to halt execution of the graph, and is roughly
equivalent to calling `exit`. Any external connection can call `halt`, and it is
associated with the channel rather than any device. All connections will eventually
receive a `halt` event via `poll`, including the client that made the original call.
There is an integer code and optional message associated with halt, which will
be delivered to all connections. The expectation is that a zero halt code is
success, and non-zero is failure, though this is only a convention.

The timing of `halt` is impossible to guarantee, whether that's between each external connection and
the devices it's connected to, or between different external connections. There is
no well-defined global time, so we can only order the call to `halt` with respect
to other calls on the same channel. As a consequence, the call to halt does not
immediately halt the computation, and the connection may still receive a lot more
events before the caller actually receives the halt event back.

It is possible that multiple external connections call halt, in which case the
server will pick one single unique halt, and deliver that halt back to all
clients. So if one client halts with `code=0` and another client halts with
`code=-1` then the server will pick at random, but all clients will observe
the same halt code.

Once a client has issued a `halt` request, it is an error to issue any more `send`
or `halt` calls, and they will return `ConnectionFinished`. However, clients may issue more `poll` requests, as they need
to keep draining until they get the `halt` event back.

Once a `halt` event has been return by the server, there will be no further
events delivered, and any following `poll` requests (both new ones, and
those still outstanding) will fail with `ConnectionFinished`.

#### Parameters

- "code" : `int` Numeric code associated with halt. Zero is success, non-zero is some kind of error.

- "message"? : `str` Optional textual messages describing reason for halt.

#### Results

None

#### Errors

- Already halted

#### Example

Halt and then just drop the connection:
```
--> {"jsonrpc":"2.0", "id":"id5", "method":"halt", "params":{
    "code":10
  }}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{}}
```

Halt and then keep pumping messages until done:
```
--> {"jsonrpc":"2.0", "id":"id5", "method":"halt", "params":{
    "code":10, "message":"Much failure"
  }}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{}}
--> {"jsonrpc":"2.0", "id":"id6", "method":"poll"}
<-- {"jsonrpc":"2.0", "id":"id6", "result":{
    "events": [
      {"src":"wibble:out", "data":{"x":5} }  # Halt might not be immediate, could get other messages
    ]
  }}
--> {"jsonrpc":"2.0", "id":"id7", "method":"poll"}
<-- {"jsonrpc":"2.0", "id":"id7", "result":{
    "events": {"type":"halt", "code":10, "message":"Much failure"}  # Halt message will be delivered back
  }}
```

Halt and then erroneously try to send:
```
--> {"jsonrpc":"2.0", "id":"id5", "method":"halt", "params":{
    "code":10
  }}
--> {"jsonrpc":"2.0", "id":"id6", "method":"send", "params":{
    "messages":[ {"src":"dev11:out"} ]      # Shouldn't be sent following a halt
  }}
<-- {"jsonrpc":"2.0", "id":"id5", "result":{}}
<-- {"jsonrpc":"2.0", "id":"id6", "error":{
    "code":-1, "message":"ConnectionFinished : this method failed because the connection has previously moved into the FINISHED state."
  }}
```

