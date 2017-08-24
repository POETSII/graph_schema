Compilation strategy

- Every set is turned into a device type called "set_{id}"
  - Each dat is mapped to a state member of the relevant device called "set_{set}::dat_{id}"
  
- There is a single "global" device type that contains mutable globals
  - Each mutable global is mapped to a state member called "global::global_{id}"
  
- Constant globals are mapped to graph properties called "global_{id}"

For a given invocation (i.e. ParFor object, which specifies kernels,
arguments, maps), we have:

{invocation} : A unique invocation name of the invocation. Usually just the
               name of the kernel function, but if the same kernel is used multiple
               times then it needs to be uniquified as the arguments may change.

- indirect_read_sets : Set of all op2.Sets which provide an indirect input argument
  - indirect_read_dats : Set of all dats which are read by any argument
- indirect_write_sets : Set of all op2.Sets which receive an indirect output argument
  - indirect_write_dats : Set of all dats which are written by any argument
  
RW variables will appear on both the read and write sets.
INC variables appear only on the write set


## Argument storage and parameter passing

### Kernel function

Each kernel function is mapped to a single function placed in
shared code called `{kernel}`, which is shared amongst all invocations of the
kernel. The kernel function is completely stateless, and only
has access to the arguments explicitly passed, so there are no
ambient globals: all reading/writing is via parameters.

NOTE : in principle it would make sense for kernels, and possibly
direct dats, to be accessed via a single pointer. This would reduce
function call overhead and code size. For now we'll keep things
absolutely regular.

The kernel will be executed in a handler called `{invocation}_execute`,
and somewhere in the middle will be a function called that looks like:
```
{kernel}(
  arg0,
  arg1,
  ...
  argN-1
);
```
Each of the arguments could come from a variety of places, depending
on the type of argument.

### Constant global arguments

Constant global arguments are mapped to the graph properties.
```
{kernel}(
  ...
  graphProperties->global_{global}
  ...
);
```

### Length arguments

Length arguments are mapped to values in the graph properties:
```
{kernel}(
  ...
  graphProperties->length_{set}
  ...
);
```

### Mutable global arguments

Each mutable global argument has a standard landing pad, shared amongst all
invocations, called:

   {iter_set}::global_{name}
   
The landing pad is re-used for all access modes. The behavioud for
different access modes is:
- Before {invocation}_execute:
  - READ : Written by {invocation}_begin
  - INC : Zero initialised at start of {invocation}_execute
- After {invocation}_execute:
  - READ : Nothing
  - INC : Sent by {invocation}_end

```
if( {access_mode} == INC){
    zero_value(deviceState->global_{global});
}
{kernel}(
  ...
  deviceState->global_{global}
  ...
);
```

### Indirect arguments  

Each indirect argument has a landing pad in the iter set device state called:

   {iter_set}::{invocation}_arg{index}_buffer
   
This is used for all types of indirect arguments. The behaviour for
different access_modes is:
- Before {invocation}_exec
  - READ : Written by {invocation}_arg{index}_read_recv
  - RW : Written by {invocation}_arg{index}_read_recv
  - WRITE : Nothing
  - INC : Zero initialised at start of {invocation}_exec
- After {invocation}_exec
  - READ : Nothing
  - RW : Sent by {invocation}_arg{index}_write_send
  - WRITE : Sent by {invocation}_arg{index}_write_send
  - INC : Sent by {invocation}_arg{index}_write_send

```
if( {access_mode} == INC){
    zero_value(deviceState->{invocation}_arg{index}_buffer);
}
{kernel}(
  ...
  deviceState->{invocation}_arg{index}_buffer
  ...
);
```

## Direct arguments

A direct argument already exists as `dat_{dat}` in the deviceState. Most
modes are not that difficult:
- READ : Implicitly all other uses must also be READ, so can used directly
- RW : There can only be one RW use of argument, so can use directly
- WRITE : Again, there can only be on WRITE use, so can use directly
- INC : A bit trickier.

For INC arguments, as well as this direct use, there might also be indirect
uses (AFAICT), as it makes sense. It might also be the case that the same
dat is used directly twice (which I think doesn't make sense in the RW and WRITE cases).
We can assume that:
- An INC should never be read (i.e. _using_ the value)
- An INC should never be written (i.e. straight assignment)
- The only valid usage is += and -=
- Whenever an INC needs to be zeroed, it will be explicitly zeroed in a
  way that cannot intefere with others. i.e. it must be zeroed in a
  context where it is accessed as RW or WRITE, and there are no
  concurrent INC accesses.
then we can decide that:
- direct INC is handled by using += / -= on the state directly
- indirect INC is applied in the {invocation}_arg{index}_write_recv

So unlike indirect INC arguments, we do _not_ zero out the INC
variables, as:
- It should keep its value from any previous parallel invocations
- It is possible that we may receive on {invocation}_arg{index}_write_recv
  before we actually execute {invocation}_execute, as another device
  may have got ahead of us.

This means that in all access modes, we pass the dat_{dat} member directly:
```

```{kernel}(
  ...
  deviceState->dat_{dat}
  ...
);
```

## Message flow

### Overall execution flow

The `{invocation}_begin` message is sent _once_ to all sets involved in the
reading, execution, and writing of a particular invocation, including:
- Devices on the iteration set
- Devices involved in an indirect read
- Devices involved in an indirect write
A device may be involved in an invocation in many ways (e.g. provides
multiple arguments, and is on the iter set), but it will still receive
exactly one `{invocation}_begin` message.

NOTE: _Sending `{invocation}_begin` to devices which are only on the write
set may seem a waste of messages, but it is to make it easier to reason
about. There are possibilities of dangling devices, so devices that are
in an indirect write sets, but that particular device is not written to.
Making sure we still get `{invocation}_end` from that device requires
quite a lot of work from the topology generator, so for now we will
send `{invocation}_begin` to everything in `{ iteration_set } | indirect_read_sets | indirect_write_sets`.
At the end, we then know that we should be getting exactly as many
`{invocation}_end` responses.

Once a device on the home set has received all mutable constants and
indirect inputs, it will execute `{invocation}_execute`. After it has executed
it will send any indirect writes via `{invocation}_arg{index}_write_send`.

A device on the home set has completed it's invocation once:
- It has executed `{invocation}_execute`
- All expected `{invocation}_arg{index}_write_recv` messages have arrived
Once those both hold, it will send `{invocation}_end` back to the controller.
This message also contains values for any INC variables.

Devices on the indirect write set will also send back `{invocation}_end`
messages, but with zeros for any INC globals in the message.

So the controller expects to receive `{invocation}_begin` messages
from all devices that are involved in execution and writing, including:
- Devices involved in an indirect read
- Devices on the iteration set
- Devices involved in an indirect write
As with the begin messages, any device produces exactly one `{invocation}_end`
message, regardless of how many indirect read/write sets it is involved in.

_NOTE: Devices that are only on the indirect read set must still send a message,
as there could be devices which have a dat that is part of the invocation,
but their specific value is not consumed by anyone. If indirect reads did
not send a completion message, then the rest of the system could race
onwards, and then at some random point the pending read is released.

_TODO: Arguably, if they are not connected to anyone then it doesn't matter, but for ease
of thinking we currently expect competion from everyone. In future we might
think about not connecting device instances where they are:
- only on the indirect read set; and
- the instance mapping means no-one uses their value.
In such case we could simple not connect up their `{invocation}_begin`, as
we already know they play no role._

NOTE : The arrival of certain messages conveys the same information as
`{invocation}_begin`. For example, receiving `{invocation}_arg{index}_read_recv`
or `{invocation}_arg{index}_write_recv` tells us that some other part
of the graph has already raced ahead on the invocation. In principle
we could use this to implicitly start sending any indirect read parameters
that this device control, but we would need to make sure we did not
send `{invocation}_end` until we had received `{invocation}_start`.
In some cases there will be mutable globals that are carried by
`{invocation}_start`, so that creates a natural ordering. However,
we could imagine an execution order such as:

- Recv:   `{invocation}_dat{datA}_write_recv` : Some kernel is sending us the result of their execution
- Recv:   `{invocation}_arg{index1}_read_recv` : Get the value of an input dat
- "Send": `{invocation}_execute` : No dependencies on mutable constants, so can do the execution.
- Send:   `{invocation}_arg{index2}_write_send` : Send our output value on to it's destination
- Send:   `{invocation}_dat{datB}_read_send` : We send the input value for an indirect dat that we host
- Recv:   `{invocation}_begin` : Have to wait until we are "officially" told that invocation has started
- Send:   `{invocation}_end` : Acknowledge completion, so that controlled knows the round has finished.

### Indirect reads

Each dat in the `indirect_read_dats` set will be sent once for a given
invocation, using an output pin called:

   {to_set}.{invocation}_{dat}_read_send

It is up to the topology builder to add edges from the single dat
pin to the zero or more consumers of that value within the invocation.

Each indirect read argument is exposed as a port on the iteration set device:

   {iter_set}.{invocation}_arg{index}_read_recv

The {index} is the index of the argument within the ParFor statement,
and is needed because the same dat value may be used multiple times
for different arguments. We handle this by sending the value once,
but then routing it to potentially multiple argument ports. The
handler simply writes the message value to
the state member called `{iter_set}::{invocation}_arg{index}_buffer`.


## Indirect writes

Each output arg in the kernel will result in an output pin on the
iter set device called:

    {iter_set}.{invocation}_arg{index}_write_send

The send handler will copy the value of `{iter_set}::{invocation}_arg{index}_buffer`
into the message.

The iter set device uses a counter `{iter_set}::{invocation}_write_send_pending` to track the
number of indirect writes still left to be done. Each value > 0 represents an argument
still to be sent:
- `{invocation}_write_send_pending > 0` : Will send the (`{invocation}_write_pending`) indirect write argument next.
- `{invocation}_write_send_pending == 1` : Ready to send `{invocation}_end`.

The output will be sent to a pin on the to_set called:

    {to_set}.{invocation}_dat{dat}_write_recv
    
Which will take the value in the message and do:
- INC : `inc_value(deviceState->dat_{dat}, message->value);`
- RW : `copy_value(deviceState->dat_{dat}, message->value);`
- WRITE : `copy_value(deviceState->dat_{dat}, message->value);`

The to set device uses a counter `{to_set}::{invocation}_write_recv_pending` to track
the number of indirect reads which have not been received. Each value>0 represents a
message we need to wait for before sending `{invocation}_end`.

The number of dat writes could vary between devices, as an INC argument could
be the target of many (or no) devices, depending on the map. Similarly, while
a given device's dat's can only appear once as RW or WRITE, they may appear
zero times as well. The total number of incoming messages expected, across
all arguments of an invocation, is recorded as a device property:

    {invocation}_write_recv_expected


## Invocation lifecycle

As noted earlier, we don't have to wait for {invocation}_begin to
know we are in an invocation. In fact it is quite likely it won't
be the first message for some devices, so the first message received
in an invocation could be any of:
- {invocation}_begin
- {invocation}_arg{index}_read_recv
- {invocation}_arg_{dat}_write_recv

We will use the variable `{invocation}_in_progress` to track whether
we know that we are in an invocation in not. So the states are:
- {invocation}_in_progress goes high : Occurs during receipt of one of the above three messages
- {invocation}_in_progress==1 : We have not yet sent a corresponding `{invocation}_end`
- {invocation}_in_progress goes low : Occurs during {invocation}_end.
- {invocation}_in_progress==0 : All activity related to the invocation has completed.

Because the global controller cannot start a new round until we have indicated
completion of this round, we are guaranteed that no new messages will arrive. So
we just have to count up the total received, and know that when it hits the
expected number of indirect reads from and indirect writes to this device then
there will be no more receives.

The final {invocation}_end message can only be sent when:
- All indirect reads housed at this device have been sent
- All indirect writes housed at this device have been received
- If on the iteration set: {invocation}_execute has been called

Overall this gives us the condition that:
- if {invocation}_in_progress==0,
- then {invocation}_read_recv_pending==0
- and {invocation}_write_recv_pending==0

So the life-cycle of the device is:

- {invocation}_in_progress=0
- Receive one of the three start messages
  - {invocation}_in_progress=1
  - {invocation}_read_recv_count++
  - {invocation}_read_send_mask = {invocation}_{set}_read_send_mask_all
 - Receive one of the three input
  - {invocation}_read_recv_count++
- Wait until:
    -{invocation}_read_recv_count == deviceProperties->{invocation}_read_recv_total
  then do {invocation}_execute:
  - {invocation}_read_recv_count=0
  - {invocation}_write_recv_count++  // execute counts as an "indirect" write
  - {invocation}_write_send_mask = {invocation}_{set}_write_send_mask_all
- Wait until:
    - {invocation}_write_send_mask==0
    - {invocation}_read_send_mask==0
    - {invocation}_write_recv_count== deviceProperties->{invocation}_write_recv_total
  then send {invocation}_end:
    - {invocation}_write_recv_count=0
    - {invocation}_in_progress=0
    
For now _all_ involved devices go through every step. This means that everyone
has an {invocation}_execute, even those which are only indirect readers or writers.

## Message summary


Device Type         | Direction | Pin Name
--------------------------------------------------------------------
{iter_set}          | input     | {invocation}_begin                
{indirect_read_set} | output    | {invocation}_dat_{dat}_read_send  
{iter_set}          | input     | {invocation}_arg{index}_read_recv 
{iter_set}          | output    | {invocation}_execute
{iter_set}          | output    | {invocation}_arg{index}_write_send
{indirect_write_set}| input     | {invocation}_dat_{dat}_write_recv

State                             | Meaning
------------------------------------------------------------------------------------------------
{invocation}_in_progress          | Tracks whether we are currently in an invocation
{invocation}_read_send_mask       | RTS flags for indirect reads we need to broadcast
{invocation}_read_recv_count      | How many indirect reads we have seen. Includes {invocation}_begin
{invocation}_write_recv_count     | How many indirect writes we have seen. Includes {invocation}_execute
{invocation}_write_send_mask      | RTS flags for indirect writes we need to send

 
"""
