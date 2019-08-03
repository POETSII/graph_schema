#ifndef sprovider_hpp
#define sprovider_hpp

#ifndef SPROVIDER_GLOBAL_CONST
#if __OPENCL__
#define SPROVIDER_GLOBAL_CONST __constant
#else
#define SPROVIDER_GLOBAL_CONST const
#endif
#endif

#ifndef SPROVIDER_ENABLE_FUNCTION_POINTERS
#if __OPENCL__
#define SPROVIDER_ENABLE_FUNCTION_POINTERS 0
#else
#define SPROVIDER_ENABLE_FUNCTION_POINTERS 1
#endif
#endif

#if __OPENCL__
#define SPROVIDER_UNREACHABLE (void)0
#else
#define SPROVIDER_UNREACHABLE {assert(0); __builtin_unreachable();}
#endif

/////////////////////////////////////
// These are intended to allow for independent compilation of
// compilers for testing purposes. In a simulator they would
// be determined by the surrounding environment

#ifndef handler_log
void sprovider_handler_log(int level, const char *msg, ...);
#define handler_log(level, ...) sprovider_handler_log(level, __VA_ARGS__)
#endif

#ifndef assert
#include <cassert>
#endif

//
///////////////////////////////////////////

#ifndef __OPENCL__
#include <cstdint>
#endif

typedef bool active_flag_t;

/*
    The handler functions return a boolean "active" flag which
    describes what happened and the future activity that might happen:
    
    - try_send : Returns true if any activity was completed, and false if nothing
      happened. If something happened, then it is possible that more activity
      might be possible. If nothing happened then no more activity will occur until
      an external event happens:
      - true: something definitely happened (precise); another event might happen (approx)
      - false: nothing happened (precise); no further event will happen (precise)
    
    - do_* : By definition some activity must happen, so this returns a flag indicating
      whether there _might_ be more activity. If false is returned then the device
      was not active, and will remain inactive until some other event occurs to the device.
      If true is returned then there might be more activity, but we don't know.
      - true: something definitely happened (precise); another event might happen (approx)
      - false: something definitely happened (precise); no further event will happen (precise)
    
    Despite the slightly different meanings, the impact is the same:
    - true: you need to eventually execute try_send in order to check if the device is idle.
    - false: the device is definitely idle until some external event happens.


/*
    All *_padded refer to the size padded up to 4 bytes.
 */

struct sprovider_input_info_t
{
    const char *name;
    unsigned index;
    unsigned message_size;
    unsigned message_size_padded;
    unsigned properties_size;
    unsigned properties_size_padded;
    unsigned state_size;
    unsigned state_size_padded;
    unsigned properties_state_size_padded;
};

struct sprovider_output_info_t
{
    const char *name;
    unsigned index;
    unsigned message_size;
    unsigned message_size_padded;
    bool is_indexed;
};

struct sprovider_device_info_t
{
    const char *id;
    bool is_external;
    int has_hardware_idle;
    int has_device_idle;
    unsigned properties_size;
    unsigned properties_size_padded;
    unsigned state_size;
    unsigned state_size_padded;
    unsigned properties_state_size_padded;
    unsigned input_count;
    unsigned output_count;
    sprovider_input_info_t inputs[32];
    sprovider_output_info_t outputs[32];
};

struct sprovider_graph_info_t
{
    const char *id;
    bool has_any_hardware_idle; // True if any device uses hardware idle
    bool has_any_device_idle; // True if any device uses device idle
    bool has_any_indexed_send; // True if there is any output on any device that is indexed
    unsigned properties_size;
    unsigned properties_size_padded;
};


//////////////////////////////////////////
// This is section is handlers that will be defined later


/*! Execute the specified receive handler . */
static active_flag_t sprovider_do_recv(void *_ctxt, unsigned _device_type_index, unsigned _pin_index, const void *gp, void *dp_ds, void *ep_es, const void *m);


/* This sends at most one message or does a compute step. Internally it calculates rts, and then
    if it finds a bit will call the handler. If nothing is rts, then it will
    try to call the compute handler.
    If there is a message to send, then *output_port should be set to the index, otherwise
    it should be left un-changed.
    If compute was done then output_port is un-changed.
*/
static active_flag_t sprovider_try_send(void *_ctxt, unsigned _device_type_index, const void *gp, void *dp_ds, int *output_port, unsigned *message_size, int *sendIndex, void *m);

/* It is the caller's responsibility to make sure that the given handler is
value for the current device state. */
static active_flag_t sprovider_do_send(void *_ctxt, unsigned _device_type_index, unsigned _pin_index, const void *gp, void *dp_ds, int *output_port, unsigned *message_size, int *sendIndex, void *m);


/* This must return a completely precise active flag, so it
  must return (*rts) !=0 or *requestCompute */
static active_flag_t sprovider_calc_rts(void *_ctxt, unsigned _device_type_index, const void *gp, const void *dp_ds, unsigned *rts, int *requestCompute);

/* It is the caller's responsibility to ensure that this only gets called if the
    device is actually idle. */
static active_flag_t sprovider_do_hardware_idle(void *_ctxt, unsigned _device_type_index, const void *gp, void *dp_ds);

/* It is the caller's responsibility to ensure that this only gets called if the
    device has requestCompute high. */
static active_flag_t sprovider_do_device_idle(void *_ctxt, unsigned _device_type_index, const void *gp, void *dp_ds);

static active_flag_t sprovider_do_init(void *_ctxt, unsigned _device_type_index, const void *gp, const void *dp_ds);



////////////////////////////////////////////////////////////////////
// This section is stuff that must be generated
#if 0

/* Total number of device types */
SPROVIDER_GLOBAL_CONST int SPROVIDER_DEVICE_TYPE_COUNT = 0;

SPROVIDER_GLOBAL_CONST sprovider_graph_info_t SPROVIDER_GRAPH_TYPE_INFO;

SPROVIDER_GLOBAL_CONST sprovider_device_info_t SPROVIDER_DEVICE_TYPE_INFO[SPROVIDER_DEVICE_TYPE_COUNT];

SPROVIDER_GLOBAL_CONST int SPROVIDER_MAX_PAYLOAD_SIZE = 64;

#endif

#endif