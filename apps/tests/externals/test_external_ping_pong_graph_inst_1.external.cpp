#include "poets_protocol/InProcessBinaryUpstreamConnection.hpp"

#include <cassert>

#pragma pack(push,1)
struct msg_message_t
{
    uint32_t payload1;
    float payload2;
};
#pragma pack(pop)

void inproc_external_main_t(
    InProcessBinaryUpstreamConnection &services,
    int argc,
    const char *argv[]
){
    services.check_interface_hash_value();

    services.connect(
        "test_external_ping_pong",
        "test_external_ping_pong_inst_1",
        {
            {"outsider_0","outsider"}
        }
    );

    for(unsigned i=0; i<10; i++){
        auto msgG=std::make_shared<std::vector<uint8_t>>();
        msgG->resize(sizeof(msg_message_t));
        auto msg=(msg_message_t*)&(msgG->at(0));
        msg->payload1=i+1;
        msg->payload2=i*i;

        services.wait_until(true);
        
        services.send("outsider_0", poets_pin_index_t{0},  msgG);

        services.wait_until(false);

        std::vector<std::pair<std::string,poets_pin_index_t>> destinations;
        std::shared_ptr<std::vector<uint8_t>> payload;

        assert(services.can_recv());
        
        services.recv(destinations, payload);

        assert(destinations.size()==1);
        assert(destinations[0].first=="outsider_0");
        assert(destinations[0].second==poets_pin_index_t{0});
    }
}