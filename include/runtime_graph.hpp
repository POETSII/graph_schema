

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

struct graph_properties_t
{};

struct device_properties_t
{};

struct edge_properties_t
{};

struct device_state_t
{};

struct edge_state_t
{};

struct message_t
{};

struct packet_t
{};


typedef void (*receive_handler_t)(
				  const graph_properties_t *graphProperties,
				  const device_properties_t *deviceProperties,
				  device_state_t *deviceState,
				  const edge_properties_t *edgeProperties,
				  edge_state_t *edgeState,
				  const message_t *message, 
				  bool *requestSend // An array of bools, one per output port
				  );
 
typedef void (*send_handler_t)(
			       const graph_properties_t *graphProperties,
			       const device_properties_t *deviceProperties,
			       device_state_t *deviceState,
			       message_t *message,
			       bool *abortSend,
			       bool *requestSend // An array of bools, one per output port
			       );

struct device_type_t;
struct edge_instance_t;


struct device_type_t
{
  unsigned num_inputs;
  receive_handler_t *input_handlers;

  unsigned num_outputs;
  send_handler_t *output_handlers;
};

struct input_port_t
{
  unsigned num_edges;
  receive_handler_t
  incoming_instance_t *edges;
};

struct device_instance_t
{
  const device_type_t *device_type;
  const device_properties_t *properties;
  device_state_t *state;
  
  struct input_port_t *beginInputs, *endInputs;
  struct output_port_t *beginOutputs, *endOutputs;
};

