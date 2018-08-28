

## Assumptions

This document tends to view things from the point of view of externals,
so certain naming conventions have a bias. However, it is also implicitly
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
long as there is a strict tree topology (no cycles!).

A message which is moving from a client to a server is moving _upwards_
(i.e. up the tree to the root), while the other direction is _downward_
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

- upwards + multi-cast : The multi-cast message originated in the client,
  and the server is requested to fan it out to any non-local devices 
  (where non-local is w.r.t. the client). The server should _not_ send
  the message back to the original client, as the assumption is that any
  local devices will have the message directly delivered. It is not
  required that there exist any non-local destinations for the message,
  but it is reccommended that clients do not create upwards + multi-cast
  messages if there are no non-local clients.

- downwards + multi-cast : The multi-cast message either originated in the
  server or in a different client, and the client should fan the message
  out to any local destinations. It is not required that there exist any
  local destinations for the message, but it is reccommended that servers
  only send messages to clients where there is a local client. Note that
  it _is_ required that a client never receives back any message that
  it created.

- upwards + uni-cast : The message represents a single message to a non-local
  device. It is required that any local messages are delivered directly,
  without asking the server to send them back.

- downwards + uni-cast : The message represents a single message to a local
  device. Uni-cast messages to non-local devices should not be sent to the
  client.

### Establishing the local device set

Multiple client connections might be established to a server, with each
client managing a different set of external devices. When the server starts
it does not, in general, know who is going to manage a particular external
device, nor in some cases how many client connections there might be. To
allow the server to map devices to clients, the clients must establish
a local-set at start-up, before the messages start flying.

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

This classification is made based on the device type, rather than on
the connections made in a particular graph instance. All devices
except for interactive can be handled by dumb input and output
streams, and it may be possible to model an interactive device
using naive input/output streams too. But sufficiently complicated
interactive devices must be modelled by a genuine piece of compute,
if only to make sure that certain output messages are not produced
until required input messages are received.

In every case a client has to establish the set of devices that it
is managing, even if it is not a full interactive device. That means
that even for a pure sink device the client has to send some information
in to establish the connection, or the server must provide some explicit
or implicit means for establishing who is managing what (e.g. command
line parameters). 

### Connection establishment

A connection is started by each partner sending a connection
header that identifies the stream type. These headers must be
the very first bytes that appear, in order to allow clients or
servers to sniff the connection type. For the JSON protocol
the headers are always strings, and the very first character
in the stream must be the speech mark. There must also be a
new-line after the closing quotation mark to allow for line-oriented
readers.

Defined headers are:

- `"POETS-external-JSON-v0-client"` - Sent by a client.

- `"POETS-external-JSON-v0-server"` - Sent by a server.

All following messages, from either side, will be JSON
objects. It is also strictly required that there is
at least one line-break following each JSON object (there
may be line-breaks inside objects too).

The message immediately following the header string will be
a dictionary used to establish further particulars. The server
must immediately send out it's connection dictionary, while the
client may (if it wishes), wait until it receives the connection
dictionary from the server.

Properties which are required for both sides are:

- `"type":"connect-begin"` - Indicates that this establishing the start of the connection.

Properties required for the server connection dictionary are:

- `"graph-type-id":str` - The id for the graph type being managed.

- `"graph-instance-id":str` - The id for the graph instance being managed.

Properties required for the client are:

- `"local-device-ids":[deviceId*]` - An array containing the set of 
  device ids that this client is going to manage.

A client which doesn't have full access to the graph instance might
request the properties of the devices it is managing and/or the
connectivity of those devices. For example, if 

To complete connection each side must send a dict with the type
`"type":"connect-complete"`. Once this dict has been received then
the connection is fully open for standard messages.


### Message exchange

Once both the client and server have exchanged connection dictionaries they
can start exchanging messages. However, it is acceptable for a client to
start blasting messages in without waiting for the client server, as would
happen if the client is simply a dumb file stream.

Messages have one of the following forms:

- `{"type":"uni", "srcDev":str, "srcPort":str, "payload":*}` : A uni-cast message.

- `{"type":"multi", "srcDev":str, "srcPort":str, "payload":*, "dstDev":str, "dstPort":str, }` : A multi-cast message.
