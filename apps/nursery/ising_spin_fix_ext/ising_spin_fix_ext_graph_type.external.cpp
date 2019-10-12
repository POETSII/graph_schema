#include "poets_protocol/InProcessBinaryUpstreamConnection.hpp"

#include <cassert>

#pragma pack(push,1)
struct window_message_t
{
    uint32_t next_time;
};
#pragma pack(pop)

#pragma pack(push,1)
struct pixel_message_t
{
    uint16_t x;
    uint16_t y;
    uint32_t time;
    uint32_t spin;
};
#pragma pack(pop)

#pragma pack(push,1)
struct graph_properties_t
{

    uint32_t width;
    uint32_t height;
    uint32_t probabilities[10];
    uint32_t slice_step;
};
#pragma pack(pop)


using Events = InProcessBinaryUpstreamConnection::Events;

extern "C" void in_proc_external_main(
    InProcessBinaryUpstreamConnection &services,
    int argc,
    const char *argv[]
){
    services.check_interface_hash_value();

    services.connect(
        "ising_spin_fix_ext",
        "ising_spin_fix_ext_.*",
        {
            {"e","sink"}
        }
    );

    graph_properties_t gp;
    services.get_graph_properties(sizeof(gp), &gp);
    unsigned totalDevices=gp.width*gp.height;

    auto external=services.get_device_address("external");
    auto external_out=makeEndpoint(external , poets_pin_index_t{0});

    unsigned gotNow=0;
    uint32_t timeNow=0;

    std::shared_ptr<std::vector<uint8_t>> msgG;
    while(1){
        if(gotNow==totalDevices){
            services.wait_until(Events::CAN_SEND);
            
            timeNow += gp.slice_step;
            gotNow=0;

            msgG->resize(sizeof(window_message_t));
            auto w=(window_message_t*)msgG->at(0);
            
            w->next_time=timeNow;
            bool done=services.send(external_out, msgG);
            assert(done);
        }

        services.wait_until(Events(Events::CAN_RECV|Events::TERMINATED));

        auto t=services.get_terminate_message();
        if(t){
            exit(t->code);
        }

        assert(services.can_send());

        poets_endpoint_address_t source;
        unsigned sendIndex;
        if(!services.recv(source, msg, sendIndex)){
            throw std::runtime_error("Services violated contract");
        }

        auto msg=(const pixel_message_t*)msgG.get();

        std::cout<<msg->time<<","<<msg->x<<","<<msg->y<<","<<msg->spin;
    }

        
}
