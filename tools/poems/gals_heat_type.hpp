#ifndef gals_heat_type_hpp
#define gals_heat_type_hpp

#include "poems.hpp"

#include "graph_persist_dom_reader.hpp"

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
    uint32_t na;
    uint32_t cs;
    uint16_t ns;
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

using cell_in_state_t = empty_struct_tag;


POETS_ALWAYS_INLINE void do_recv_cell_in(const void *gp, void *dp_ds, void *ep_es, const void *m)
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

    if(deviceState->t == message->t){
        deviceState->cs++;
        deviceState->ca += edgeProperties->w * message->v;
    }else{
        deviceState->ns++;
        deviceState->na += edgeProperties->w * message->v;
    }
}


void provider_do_recv(uint32_t handler_index, const void *gp, void *dp_ds, void *ep_es, const void *m)
{
    switch(handler_index)
    {
        case 0: return do_recv_cell_in(gp, dp_ds, ep_es, m);
        default: assert(0);
    }
}

POETS_ALWAYS_INLINE void calc_rts_cell(const void *gp, void *dp_ds, uint32_t *rts, bool *requestCompute)
{
    auto graphProperties=(const gals_heat_properties_t*)gp;
    auto deviceProperties=get_P<cell_properties_t,cell_state_t>(dp_ds);
    auto deviceState=get_S<cell_properties_t,cell_state_t>(dp_ds);
    
    ///////////////////////////////////////
    // Handler

    *rts=0;
    if(deviceState->t < graphProperties->max_t){
        if(deviceState->cs==deviceProperties->degree){
            *rts=(1<<0); // Cheating
        }
    }
}

POETS_ALWAYS_INLINE void do_compute_cell(const void *gp, void *dp_ds)
{
    // Nothing
}

POETS_ALWAYS_INLINE void do_init_cell(const void *gp, void *dp_ds)
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
}

void provider_do_init(unsigned device_type_index, const void *gp, void *dp_ds)
{
    switch(device_type_index)
    {
        case 0: do_init_cell(gp, dp_ds); break;
        default: assert(0); break;
    }
}


POETS_ALWAYS_INLINE void do_send_cell_out(const void *gp, void *dp_ds, unsigned &size, bool &doSend, int &sendIndex, void *m)
{
    auto graphProperties=(const gals_heat_properties_t*)gp;
    auto deviceProperties=get_P<cell_properties_t,cell_state_t>(dp_ds);
    auto deviceState=get_S<cell_properties_t,cell_state_t>(dp_ds);
    auto message=(update_message_t*)m;

    size=sizeof(update_message_t);

    /////////////////////////////////

    deviceState->t++;
    deviceState->v=deviceState->ca;
    deviceState->ca=deviceState->na + deviceProperties->wSelf*deviceState->v;
    deviceState->cs=deviceState->ns;    
    deviceState->na=0;
    deviceState->ns=0;

    message->t=deviceState->t;
    message->v=deviceState->v;

    //fprintf(stderr, "  devices[%p]... -> time %u\n", dp_ds, deviceState->t);
}

POETS_ALWAYS_INLINE bool do_send_cell(const void *gp, void *dp_ds, int &output_port, unsigned &size, int &sendIndex, void *m)
{
    uint32_t rts=0;
    bool requestCompute=false;
    calc_rts_cell(gp, dp_ds, &rts, &requestCompute );
    if(!rts){
        if(!requestCompute){
            return false;
        }else{
            do_compute_cell(gp, dp_ds);
            return true;
        }
    }
    unsigned index=__builtin_ctz(rts);
    bool doSend=true;
    switch(index){
    case 0: do_send_cell_out(gp, dp_ds, size, doSend, sendIndex, m); break;
    default: assert(0); return false;
    }
    if(doSend){
        output_port=index;
    }
    return true; // We did something, regardless of if we sent
}

bool provider_do_send(uint32_t device_index, const void *gp, void *dp_ds, int &output_port, unsigned &size, int &sendIndex, void *m)
{
    switch(device_index){
    case 0: return do_send_cell(gp, dp_ds, output_port, size, sendIndex, m);
    default: assert(0); return false;
    }
}

unsigned provider_get_device_type_index(const char *device_type_id)
{
    if(!strcmp(device_type_id, "cell")){
        return 0;
    }
    throw std::runtime_error("Unknown device type.");
}

unsigned provider_get_receive_handler_index(unsigned device_type_index, unsigned input_pin_index)
{
    if(device_type_index==0){
        if(input_pin_index==0){
            return 0;
        }
    }
    throw std::runtime_error("Unknown device or pin index.");
}

void provider_do_hardware_idle(uint32_t device_index, const void *gp, void *dp_ds)
{
    fprintf(stderr, "Hardware idle.\n");
    exit(1);
}

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
                makeScalar("ns", "uint16_t")
            }),
            {in},
            {out},
            false,
            "None", "None", "None"
        )
    );

    auto gt=std::shared_ptr<GraphTypeDynamic>(
        new GraphTypeDynamic(
            "gals_heat_fake",
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