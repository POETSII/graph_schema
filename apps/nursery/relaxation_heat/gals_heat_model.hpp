#ifndef gals_heat_model_hpp
#define gals_heat_model_hpp

#include "model_checker.hpp"

const int max_t = 4;

struct update_message_t
{
    uint8_t t;
};

std::shared_ptr<MessageType> updateMessageType
    = std::make_shared< MessageTypePlain<update_message_t> >();

struct gals_heat_properties_t
{
    uint8_t neighbours;
};

struct gals_heat_state_t
{
    uint8_t t;
    uint8_t cs;
    uint8_t ns;
};

class GALSHeatDeviceType
    : public DeviceTypeHelper<gals_heat_properties_t,gals_heat_state_t>
{
protected:
    uint32_t GetRTS( const gals_heat_properties_t *pProperties, const gals_heat_state_t *pState) const override
    {
        if(pState->t <max_t && pState->cs==pProperties->neighbours){
            return 1;
        }else{
            return 0;
        }
    }

    void onInit(
        const gals_heat_properties_t *pProperties,
        gals_heat_state_t *pState
    ) const override
    {
        pState->t=0;
        pState->cs=pProperties->neighbours;
        pState->ns=0;
    }

    bool onSend(
        const gals_heat_properties_t *pProperties,
        gals_heat_state_t *pState,
        unsigned port,
        uint8_t *payload
    ) const override
    {
        assert(port==0);

        assert(pState->cs==pProperties->neighbours);

        pState->t += 1;
        pState->cs=pState->ns;
        pState->ns=0;

        auto pUpdate=(update_message_t*)payload;
        pUpdate->t=pState->t;

        return true;
    }

    virtual void onRecv(
        const gals_heat_properties_t *pProperties,
        gals_heat_state_t *pState,
        unsigned port,
        const uint8_t *payload
    ) const {
        assert(port==0);

        auto pUpdate=(const update_message_t*)payload;
        if(pUpdate->t==pState->t){
            pState->cs++;
        }else if(pUpdate->t==pState->t+1){
            pState->ns++;
        }else{
            assert(0);
        }
    }
public: 

    GALSHeatDeviceType()
        : DeviceTypeHelper<gals_heat_properties_t,gals_heat_state_t>(
            { updateMessageType },
            { updateMessageType }
        )
    {
        /*addReceiveHandler(0, +[](const gals_heat_properties_t *pProperties, gals_heat_state_t *pState, const update_message_t *pMsg){
        });*/
    }
};

#endif