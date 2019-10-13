#include "poets_protocol/InProcessBinaryUpstreamConnection.hpp"

#include <cassert>

#pragma pack(push,1)
struct msg_message_t
{
    uint32_t payload1;
    float payload2;
};
#pragma pack(pop)

extern "C" void poets_in_proc_external_main(
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

    auto outsider_0=services.get_device_address("outsider_0");

    for(unsigned i=0; i<10; i++){
        auto msgG=std::make_shared<std::vector<uint8_t>>();
        msgG->resize(sizeof(msg_message_t));
        auto msg=(msg_message_t*)&(msgG->at(0));
        msg->payload1=i+1;
        msg->payload2=i*i;

        services.external_log(3, "Waiting until external connection can send.");
        services.wait_until(InProcessBinaryUpstreamConnection::Events::CAN_SEND);
        assert(services.can_send());
        
        services.external_log(3, "Sending from outsider_0.");
        services.send(makeEndpoint(outsider_0, poets_pin_index_t{0}),  msgG, UINT_MAX);
        msgG.reset();

        services.external_log(3, "Waiting till can recieve.");
        services.wait_until(InProcessBinaryUpstreamConnection::Events::CAN_RECV);
        assert(services.can_recv());

        services.external_log(3, "Receiving %u.", i);
        poets_endpoint_address_t source;
        unsigned sendIndex;
        services.recv(source, msgG, sendIndex);

        std::vector<poets_endpoint_address_t> fanout;
        services.get_endpoint_destinations(source, fanout);
        
        assert(fanout.size()==1);
        assert(getEndpointDevice(fanout[0])==outsider_0);
        assert(getEndpointPin(fanout[0]) == poets_pin_index_t{0});

        msg=(msg_message_t*)&(msgG->at(0));
        assert(msg->payload1==i+1);
        assert(msg->payload2==i*i);
    }

    auto hmG=std::make_shared<std::vector<uint8_t>>(sizeof(halt_message_type), 0);
    auto hm=(halt_message_type*)&hmG->at(0);
    
    while(!services.send(makeEndpoint(outsider_0, poets_pin_index_t{1}),  hmG)){
        services.wait_until(InProcessBinaryUpstreamConnection::Events::CAN_SEND);
    }
        
}
