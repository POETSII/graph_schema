#include "poets_protocol/InProcessBinaryUpstreamConnection.hpp"

#include <cassert>
#include <unistd.h>
#include <unordered_map>
#include <cstring>
#include <random>

#pragma pack(push,1)
struct window_message_t
{
    uint32_t next_time;
    uint16_t fix_x;
    uint16_t fix_y;
    uint16_t fix_type;
};
#pragma pack(pop)

#pragma pack(push,1)
struct pixel_message_t
{
    uint16_t x;
    uint16_t y;
    uint32_t time;
    int32_t spin;
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

extern "C" void poets_in_proc_external_main(
    InProcessBinaryUpstreamConnection &services,
    int argc,
    const char *argv[]
){
    services.check_interface_hash_value();

    unsigned long long max_slices=ULLONG_MAX;

    if(argc>1){
        max_slices=strtoull(argv[1], 0, 0);
        services.external_log(1, "Stopping output after %llu slices", max_slices);
    }else{
        services.external_log(1, "External is allowing infinite slices.");
    }


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

    auto external=services.get_device_address("e");
    auto external_out=makeEndpoint(external , poets_pin_index_t{0});

    unsigned gotNow=totalDevices;
    uint32_t timeNow=0|0xf;
    uint64_t timeNow64=timeNow;
    unsigned long long slice_index=0;

    std::vector<uint8_t> pixels(totalDevices*3);

    uint8_t variablePositive[3]={0x7F,0,0};
    uint8_t variableNegative[3]={0,0x7f,0};

    std::vector<uint8_t> msgG;
    while(1){
        services.external_log(2, "Top of loop, gotNow=%u, totalDevices=%u.", gotNow, totalDevices);

        if(gotNow==totalDevices){
            slice_index++;
            if(slice_index >= max_slices){
                services.external_log(1, "Completed %llu slices. Exiting happily.", max_slices);
                exit(0);
            }

            services.external_log(0, "Waiting to send.");
            services.wait_until(Events::CAN_SEND);
            
            timeNow += (gp.slice_step<<4);
            timeNow64 += (gp.slice_step<<4);
            gotNow=0;

            msgG.resize(sizeof(window_message_t));
            auto w=(window_message_t*)&msgG[0];
            
            w->next_time=timeNow;
            w->fix_type=0;

            bool done=services.send(external_out, msgG);
            assert(done);

            fprintf(stdout, "P6\n%u\n%u\n255\n", gp.width, gp.height);
            
            fwrite(&pixels[0], 1, pixels.size(), stdout);
        }

        services.external_log(2, "Waiting for receive or terminate.");
        services.wait_until(Events(Events::CAN_RECV|Events::TERMINATED));
        
        auto t=services.get_terminate_message();
        if(t){
            services.external_log(0, "Received terminate message, but expected to self-exit.");
            exit(1);
        }

        assert(services.can_send());

        poets_endpoint_address_t source;
        unsigned sendIndex;
        if(!services.recv(source, msgG, sendIndex)){
            throw std::runtime_error("Services violated contract");
        }

        if(msgG.size()!=sizeof(pixel_message_t)){
            throw std::runtime_error("Received invalid size message.");
        }
        auto msg=(const pixel_message_t*)&msgG[0];
        /*for(unsigned i=0; i<sizeof(pixel_message_t); i++){
            services.external_log(2, "  recv[%u] = %08x", i, msgG->at(i));
        }*/

        if(msg->time!=timeNow){
            throw std::runtime_error("Got pixel from time "+std::to_string(msg->time)+" expecting "+std::to_string(timeNow));
        }


        const auto *pel=msg->spin > 0 ? variablePositive : variableNegative;
        memcpy( &pixels[0] + (msg->y*gp.width + msg->x)*3, pel, 3);

        gotNow++;
    }

        
}
