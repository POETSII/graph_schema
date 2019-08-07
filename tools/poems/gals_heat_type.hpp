#ifndef gals_heat_type_hpp
#define gals_heat_type_hpp

#include "../sprovider/sprovider_types.h"
#include "../sprovider/sprovider_helpers.hpp"

#include "graph_persist_dom_reader.hpp"

#pragma pack(push,1)
struct gals_heat_properties_t
{
    uint32_t max_t;
};

struct cell_properties_t
{
    float wSelf;
    uint32_t degree;
    int8_t fixed;
};

struct cell_state_t
{
    uint32_t t;
    float v;
    float ca;
    float na;
    uint32_t cs;
    uint32_t ns;
    uint32_t rts;
};

struct cell_in_properties_t
{
    float w;
};

struct update_message_t
{
    uint32_t t;
    float v;
};
#pragma pack(pop)

using cell_in_state_t = empty_struct_tag;


SPROVIDER_ALWAYS_INLINE bool do_recv_cell_in(const void *gp, void *dp_ds, void *ep_es, const void *m)
{
    auto graphProperties=(const gals_heat_properties_t*)gp;
    auto deviceProperties=get_P<cell_properties_t,cell_state_t>(dp_ds);
    auto deviceState=get_S<cell_properties_t,cell_state_t>(dp_ds);
    auto edgeProperties=get_P<cell_in_properties_t,cell_in_state_t>(ep_es);
    auto edgeState=get_S<cell_in_properties_t,cell_in_state_t>(ep_es);
    auto message=(const update_message_t*)m;

    ///////////////////////////////////
    // Handler

    //fprintf(stderr, "  recv %p : t=%u, m.t=%u\n", dp_ds, deviceState->t, message->t);

    assert(deviceState->t == message->t or deviceState->t+1==message->t);

    if(deviceState->t == message->t){
        deviceState->cs++;
        deviceState->ca += edgeProperties->w * message->v;
        if(deviceState->cs==deviceProperties->degree){
            if(deviceState->t < graphProperties->max_t){
                deviceState->rts = 1;
            }
        }
    }else{
        deviceState->ns++;
        deviceState->na += edgeProperties->w * message->v;
    }
    return deviceState->rts!=0;
}


SPROVIDER_ALWAYS_INLINE active_flag_t calc_rts_cell(const void *gp, void *dp_ds, uint32_t *rts, bool *requestCompute)
{
    auto graphProperties=(const gals_heat_properties_t*)gp;
    auto deviceProperties=get_P<cell_properties_t,cell_state_t>(dp_ds);
    auto deviceState=get_S<cell_properties_t,cell_state_t>(dp_ds);
    
    ///////////////////////////////////////
    // Handler

    assert(deviceState->cs <= deviceProperties->degree);

    *rts=deviceState->rts;

    return *rts!=0;
}

static active_flag_t sprovider_calc_rts(void *_ctxt, unsigned _device_type_index, const void *gp, const void *dp_ds, unsigned *rts, bool *requestCompute)
{
    switch(_device_type_index){
    default: SPROVIDER_UNREACHABLE;
    case 0: return calc_rts_cell(gp, (void*)dp_ds, rts, requestCompute);
    }
}

SPROVIDER_ALWAYS_INLINE void do_compute_cell(const void *gp, void *dp_ds)
{
    // Nothing
}

SPROVIDER_ALWAYS_INLINE void do_init_cell(const void *gp, void *dp_ds)
{
    auto graphProperties=(const gals_heat_properties_t*)gp;
    auto deviceProperties=get_P<cell_properties_t,cell_state_t>(dp_ds);
    auto deviceState=get_S<cell_properties_t,cell_state_t>(dp_ds);

    ////////////////////
    
    deviceState->v=rand();
    deviceState->cs=deviceProperties->degree;
    deviceState->ca=deviceProperties->wSelf * deviceState->v;
    deviceState->ns=0;
    deviceState->na=0;
    deviceState->rts=1;
}

active_flag_t sprovider_do_init(void *_ctxt, unsigned _device_type_index, const void *gp, void *dp_ds)
{
    switch(_device_type_index)
    {
        default: SPROVIDER_UNREACHABLE;
        case 0: do_init_cell(gp, dp_ds); return true;
    }
}


SPROVIDER_ALWAYS_INLINE active_flag_t do_send_cell_out(const void *gp, void *dp_ds, bool *doSend, int *sendIndex, void *m)
{
    auto graphProperties=(const gals_heat_properties_t*)gp;
    auto deviceProperties=get_P<cell_properties_t,cell_state_t>(dp_ds);
    auto deviceState=get_S<cell_properties_t,cell_state_t>(dp_ds);
    auto message=(update_message_t*)m;

    /////////////////////////////////

    deviceState->t++;
    deviceState->v=deviceState->ca;
    deviceState->ca=deviceState->na + deviceProperties->wSelf*deviceState->v;
    deviceState->cs=deviceState->ns;    
    deviceState->na=0;
    deviceState->ns=0;

    message->t=deviceState->t;
    message->v=deviceState->v;

    deviceState->rts=0;
    if(deviceState->cs==deviceProperties->degree){
        if(deviceState->t < graphProperties->max_t){
            deviceState->rts = 1;
        }
    }

    //fprintf(stderr, "  devices[%p]... -> time %u\n", dp_ds, deviceState->t);
    return deviceState->rts!=0;
}

SPROVIDER_ALWAYS_INLINE bool do_send_cell(unsigned _pin_index, const void *gp, void *dp_ds, bool *doSend, int *sendIndex, void *m)
{
    assert(*doSend==true);
    switch(_pin_index){
    default: SPROVIDER_UNREACHABLE;
    case 0: return do_send_cell_out(gp, dp_ds, doSend, sendIndex, m); break;
    }
}


SPROVIDER_ALWAYS_INLINE active_flag_t try_send_cell(const void *gp, void *dp_ds, int *_action_taken, int *_output_port, unsigned *_message_size, int *_send_index, void *m)
{
    uint32_t rts=0;
    bool requestCompute=false;
    calc_rts_cell(gp, dp_ds, &rts, &requestCompute );
    if(!rts){
        if(!requestCompute){
            return false;
        }else{
            *_action_taken=-1;
            do_compute_cell(gp, dp_ds);
            return true;
        }
    }
    unsigned index=__builtin_ctz(rts);
    *_action_taken=index;
    bool doSend=true;
    switch(index){
    case 0: do_send_cell_out(gp, dp_ds, &doSend, _send_index, m); *_message_size=sizeof(update_message_t); break;
    default: assert(0); return false;
    }
    if(doSend){
        *_output_port=index;
    }
    return true; // We did something, regardless of if we sent
}

active_flag_t sprovider_do_send(void *_ctxt, unsigned _device_type_index, unsigned _pin_index, const void *gp, void *dp_ds, bool *doSend, int *sendIndex, void *m)
{
    switch(_device_type_index){
    default: assert(0);
    case 0: return do_send_cell(_pin_index, gp, dp_ds, doSend, sendIndex, m);
    }
}


SPROVIDER_ALWAYS_INLINE active_flag_t sprovider_try_send_or_compute(void *_ctxt, unsigned _device_index, const void *gp, void *dp_ds, int *_action_taken, int *_output_port, unsigned *_message_size, int *_send_index, void *m)
{
    switch(_device_index){
    default: SPROVIDER_UNREACHABLE;
    case 0: return try_send_cell(gp, dp_ds, _action_taken, _output_port, _message_size, _send_index, m);
    }
}


active_flag_t sprovider_do_recv(void *_ctxt, unsigned _device_type_index, unsigned _pin_index, const void *gp, void *dp_ds, void *ep_es, const void *m)
{
    switch(_device_type_index)
    {
        default: SPROVIDER_UNREACHABLE;
        case 0:   
        switch(_pin_index){
            default: SPROVIDER_UNREACHABLE;
            case 0: return do_recv_cell_in(gp, dp_ds, ep_es, m);
        }
    }
}

active_flag_t sprovider_do_hardware_idle(void *_ctxt, unsigned _device_index, const void *gp, void *dp_ds)
{
    fprintf(stderr, "Hardware idle.\n");
    exit(1);
    return false;
}

active_flag_t sprovider_do_device_idle(void *_ctxt, unsigned _device_index, const void *gp, void *dp_ds)
{
    return false;
}


/* Total number of device types */
SPROVIDER_GLOBAL_CONST int SPROVIDER_DEVICE_TYPE_COUNT = 1;

SPROVIDER_GLOBAL_CONST sprovider_graph_info_t SPROVIDER_GRAPH_TYPE_INFO = {
    "gals_heat_poems",
    true,
    false,
    false,
    sizeof(gals_heat_properties_t),
    (sizeof(gals_heat_properties_t)+3)&0xFFFFFFFCul,
};

SPROVIDER_GLOBAL_CONST sprovider_device_info_t SPROVIDER_DEVICE_TYPE_INFO[SPROVIDER_DEVICE_TYPE_COUNT] = {
    {
        "cell",
        false, // external
        true, // hw idle
        false, // dev idle
        sizeof(cell_properties_t),
        (sizeof(cell_properties_t)+3)&0xFFFFFFFCul,
        sizeof(cell_state_t),
        (sizeof(cell_state_t)+3)&0xFFFFFFFul,
        ((sizeof(cell_properties_t)+3)&0xFFFFFFFCul)+((sizeof(cell_state_t)+3)&0xFFFFFFFCul),
        1,
        1,
        {
            {"in", 0,
                sizeof(update_message_t),  (sizeof(update_message_t)+3)&0xFFFFFFFCul,
                sizeof(cell_in_properties_t),  (sizeof(cell_in_properties_t)+3)&0xFFFFFFFCul,
                0,  0,
                ((sizeof(cell_in_properties_t)+3)&0xFFFFFFFCul)+0
            }
        },{
            {"out", 0,
                sizeof(update_message_t),  (sizeof(update_message_t)+3)&0xFFFFFFFCul,
                false
            }
        }
    }
};

SPROVIDER_GLOBAL_CONST int SPROVIDER_MAX_PAYLOAD_SIZE = sizeof(update_message_t);




GraphTypePtr make_gals_heat_graph_type()
{
    auto mt=std::make_shared<MessageTypeImpl>(
        "update",
        makeTypedDataSpec({
            makeScalar("t", "uint32_t"),
            makeScalar("v", "float")
        })
    );

    std::shared_ptr<DeviceType> cell;

    auto get_dt=[&](){ return cell; };

    auto in = std::make_shared<InputPinDynamic>(
        get_dt,
        "in",
        0,
        mt,
        makeTypedDataSpec({
            makeScalar("w", "float")
        }),
        makeTypedDataSpec({})
        ,
        "None"
    );

    auto out=std::make_shared<OutputPinDynamic>(
        get_dt,
        "out",
        0,
        mt,
        "None",
        false
    );

    cell=std::shared_ptr<DeviceTypeDynamic>(
        new DeviceTypeDynamic(
            "cell",
            makeTypedDataSpec({
                makeScalar("wSelf", "float"),
                makeScalar("degree", "uint32_t"),
                makeScalar("fixed", "int8_t")
            }),
            makeTypedDataSpec({
                makeScalar("t", "uint32_t" ),
                makeScalar("v", "float"),
                makeScalar("ca", "float"),
                makeScalar("na", "uint32_t"),
                makeScalar("cs", "uint32_t"),
                makeScalar("ns", "uint32_t"),
                makeScalar("rts", "uint32_t")
            }),
            {in},
            {out},
            false,
            "None", "None", "None"
        )
    );

    auto gt=std::shared_ptr<GraphTypeDynamic>(
        new GraphTypeDynamic(
            "gals_heat_poems",
            makeTypedDataSpec({
                makeScalar("max_t", "float")
            }),
            rapidjson::Document(),  
            { "None" },
            {mt},
            {cell}
        )
    );

    return gt;
}


#endif