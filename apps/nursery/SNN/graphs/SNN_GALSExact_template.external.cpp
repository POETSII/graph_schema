#include "poets_protocol/InProcessBinaryUpstreamConnection.hpp"

#include "robin_hood.hpp"

#include <cassert>
#include <cstdlib>

#pragma pack(push,1)
struct spikem_message_t
{
    uint32_t nid;
    uint32_t t_plus_fired; 
    uint32_t hash;
};
#pragma pack(pop)

#pragma pack(push,1)
struct graph_properties_t
{
    uint64_t global_rng_seed;
    int32_t max_steps;
    float dt;
    uint32_t total_neurons;
    uint32_t send_hash_on_spike;
    uint32_t hash_export_timer_gap;
    uint32_t hash_export_on_max_steps;
};
#pragma pack(pop)

#pragma pack(push,1)
struct neuron_hash_message_t
{
    uint32_t nid;
    uint32_t t;
    uint32_t hash;
    uint32_t total_firings;
    uint64_t sum_square_firing_gaps;
};
#pragma pack(pop)


using Events = InProcessBinaryUpstreamConnection::Events;

extern "C" void poets_in_proc_external_main(
    InProcessBinaryUpstreamConnection &services,
    int argc,
    const char *argv[]
){
    services.check_interface_hash_value();

    unsigned long long max_slices=10;

    if(argc>1){
        max_slices=strtoull(argv[1], 0, 0);
        services.external_log(1, "Stopping output after %llu slices", max_slices);
    }else{
        services.external_log(1, "External is allowing infinite slices.");
    }

    services.connect(
        "SNN_GALSExact_${ModelType}",
        ".*", // Bind to any instacne
        {
            {"ext_out","external_output"}
        }
    );

    graph_properties_t gp;
    services.get_graph_properties(sizeof(gp), &gp);

    auto external=services.get_device_address("ext_out");
    auto external_spike_in=makeEndpoint(external , poets_pin_index_t{0});
    auto external_hash_in=makeEndpoint(external , poets_pin_index_t{1});
    auto external_halt_out=makeEndpoint(external, poets_pin_index_t{0});

    auto halt_device=services.get_device_address("__halt__");
    auto halt_endpoint=makeEndpoint(halt_device, poets_pin_index_t{0});

    robin_hood::unordered_flat_map<poets_endpoint_address_t,poets_pin_index_t> source_to_dest;

    std::vector<poets_endpoint_address_t> _scratch;
    auto get_dest_pin=[&](poets_endpoint_address_t source) -> poets_pin_index_t
    {
        auto it=source_to_dest.find(source);
        if(it==source_to_dest.end()){
            
            services.get_endpoint_destinations(source, _scratch);
            if(_scratch.size()!=1){
                throw std::runtime_error("Expected non-indexed send to external to have exactly one external dest endpoint.");
            }

            poets_endpoint_address_t dest=_scratch[0];

            if(dest!=external_spike_in && dest!=external_hash_in){
                fprintf(stderr, "dest=%u:%u, external=%u\n", getEndpointDevice(dest).value, getEndpointPin(dest).value, external.value);
                throw std::runtime_error("Incoming message not headed to expected pin on external.");
            }

            it=source_to_dest.insert({source,getEndpointPin(dest)}).first;
        }
        return it->second;
    };

    uint32_t final_hashes_received=0;
    bool send_halt=false;

    std::vector<uint8_t> msgG;
    while(1){
        if(send_halt && services.can_send()){
            services.external_log(0, "Received all final hashes. Quiting");

            msgG.resize(sizeof(halt_message_type));
            auto &msg=*(halt_message_type*)&msgG[0];
            
            memset(&msg, 0, sizeof(msg));
            msg.code=0;
            strncpy(msg.description, "AllFinalHashesReceived", sizeof(msg.description)-1);

            services.send(external_halt_out, msgG);

            send_halt=false;
        }

        services.external_log(2, "Waiting for receive or terminate.");
        services.wait_until(Events(Events::CAN_RECV|Events::TERMINATED));
        
        auto t=services.get_terminate_message();
        if(t){
            services.external_log(0, "Received halt message, code=%u, exiting loop", t->code);
            break;
        }

        poets_endpoint_address_t source;
        unsigned sendIndex;
        if(!services.recv(source, msgG, sendIndex)){
            throw std::runtime_error("Services violated contract");
        }
        if(sendIndex!=-1){
            throw std::runtime_error("Didnt expect indexed send.");
        }

        poets_pin_index_t dest_pin=get_dest_pin(source);

        if(dest_pin==poets_pin_index_t{0}){
            /* Note that dumping messages directly is a massive bottleneck.
                If you want to collect true spikes, there should be an 
                aggregator to do it which takes part in GALS and sends the
                firings up to the host.
            */
            if(msgG.size()!=sizeof(spikem_message_t)){
                 throw std::runtime_error("Received invalid size message.");
            }

            auto msg=(const spikem_message_t*)&msgG[0];
            auto t=msg->t_plus_fired>>1;
            bool fired=msg->t_plus_fired&1;

            if(fired){
                fprintf(stdout, "%u,%u,S,%08x\n", t, msg->nid, msg->hash);
            }
        }else if(dest_pin==poets_pin_index_t{1}){
            if(msgG.size()!=sizeof(neuron_hash_message_t)){
                 throw std::runtime_error("Received invalid size message.");
            }
            auto msg=(const neuron_hash_message_t*)&msgG[0];

            fprintf(stdout, "%u,%u,H,%08x,%u,%llu\n", msg->t, msg->nid, msg->hash, msg->total_firings, (unsigned long long)msg->sum_square_firing_gaps);
            if(msg->t==gp.max_steps){
                final_hashes_received++;
            }

            if(final_hashes_received==gp.total_neurons){
                send_halt=true;
            }
        }else{
            throw std::runtime_error("External received message from unknown source.");
        }
    }

    fprintf(stderr, "External thread returning.\n");
}
