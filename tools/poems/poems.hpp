#ifndef poems_hpp
#define poems_hpp

#include <cstdint>
#include <cassert>
#include <cstring>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <random>
#include <algorithm>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <cstdarg>
#include <functional>

#include <metis.h>
#include <tbb/concurrent_queue.h>

#include "shared_pool.hpp"

#include "../../include/graph_persist.hpp"

#include "../sprovider/sprovider_types.h"

#include "../sprovider/sprovider_helpers.hpp"

//////////////////////////////////////////////////
// Simulation logic

static unsigned sprovider_handler_log_level = SPROVIDER_MAX_LOG_LEVEL;

void sprovider_handler_log(int level, const char *msg, ...)
{
    if(level<=sprovider_handler_log_level){
        char buffer[256]={0};
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer)-1, msg, args);
        va_end(args);
        fputs(buffer, stderr);
        fputc('\n', stderr);
    }

    if(level==0){
        if(!strcmp(msg,"_HANDLER_EXIT_SUCCESS_9be65737_")){
            exit(0);
        }
        if(!strcmp(msg,"_HANDLER_EXIT_FAIL_9be65737_")){
            exit(1);
        }
    }
}

static std::chrono::high_resolution_clock::time_point g_now_start;

void set_now_start()
{
    g_now_start=std::chrono::high_resolution_clock::now();
}

double now()
{
    return (std::chrono::high_resolution_clock::now()-g_now_start).count();
}


struct POEMS
{
    struct device;
    struct edge;
    struct device;
    struct message;
    struct device_cluster;

    /* We want these optimised for the send case, where once a message is sent we
        have to walk the entire output edge list. We are also mostly (hopefully)
        delivering locally within the cluster.
        
        A tradeoff is that indexed sends need to be able to jump
        directly to a specific edge, so we either neeed fixed size cells,
        or an array of pointers to variable sized cells. Here we'll keep
        the edges fixed-size, and put properties/state in a side buffer*/
    struct edge{
        device_cluster *dest_cluster; // Used on the sending side to get to the right cluster
        device *dest_device;            // Used on the receiving side to get into the right device
        union{
            unsigned properties_then_state_offset; // Used during building of data structure
            void *properties_then_state;  // Used after data structure is built on receiving side.
        };
        union{
            int32_t send_index; // Used during loading
            uint32_t dest_device_offset_in_cluster; // Used after loading
        };
        uint16_t is_local;       // Both devices are within the same local cluster (i.e. the same thread)
        uint16_t pin_index;  // combines device type and pin index
    };

    struct edge_vector
    {
        std::vector<edge> edges; // Index to the start of each message for indexed sends.
        std::vector<char> data; // Packed properties and state
    };

    struct message
    {
        message *next;
        const edge *p_edge;
        // Payload immediately follows for locality
        // All messages are sized to hold the largest possible message
        uint8_t payload[SPROVIDER_MAX_PAYLOAD_SIZE]; 
    };

    struct device
    {   
        #ifndef NDEBUG
        const char *id;
        DeviceTypePtr device_type;
        #endif

        unsigned device_type_index;
        device_cluster *cluster;
        unsigned offset_in_cluster;

        // Each port has just a vector of ougoing destinations.
        std::vector<edge_vector> output_ports;

        // Last element of struct for locality
        uint8_t properties_then_state[];


        //////////////////////////////////////////
        // Fancyness for extra bytes

        static void* operator new(std::size_t size, unsigned extra) {
            return ::operator new(size+extra);
        }
        static void operator delete(void* p, unsigned extra)
        {
            ::operator delete(p);
        }

        void sanity()
        {
            for(auto &op : output_ports){
                for(edge &e : op.edges){
                    assert(e.dest_cluster==e.dest_device->cluster);
                    assert(e.dest_device_offset_in_cluster==e.dest_device->offset_in_cluster);
                    
                }
            }
        }
    };

    struct device_cluster
    {
        /////////////////////////
        // Shared amongst threads

        // Assume the start of struct is nicely aligned
        std::atomic<message *> incoming_queue;
        char _padding_[128-sizeof(std::atomic<message*>)];

        /////////////////////////
        // Private to thread

        std::vector<device*> devices;
        std::vector<uint64_t> devices_active_mask;

        bool active=false; // Is any device in the cluster currently active?
        bool provider_do_hardware_idle=false; // Is there a pending hardware idle?  implies active is true.
        
        uint64_t nonLocalMessagesSent=0;
        uint64_t nonLocalMessagesReceived=0;
        uint64_t localMessagesSentAndReceived=0;

        uint64_t numClusterSteps=0;
        uint64_t numNoSendClusterSteps=0;
        uint64_t numNoActivityClusterSteps=0;

        device_cluster()
            : incoming_queue(0)
        {}

        void sanity(const void *gp)
        {
            for(unsigned i=0; i<devices.size();i++){
                auto *dev=devices[i];

                assert(devices[i]->cluster==this);
                assert(devices[i]->offset_in_cluster==i);
                devices[i]->sanity();

                uint32_t rts=0;
                bool rtc=0;
                bool active=sprovider_calc_rts(nullptr, dev->device_type_index, gp, dev->properties_then_state, &rts, &rtc);
                if(rtc!=0 || rts!=0){
                    assert(is_device_active(dev->offset_in_cluster));
                }
                assert(active ? 1 : (rtc==0 && rts==0));
            }
        }

        /////////////////////////////////////////
        // Message queue stuff

        bool is_device_active(unsigned offset) const
        {
            return 0 != (devices_active_mask[offset/64] & 1ull<<(offset%64));
        }

        void set_device_active(unsigned offset, bool isActive)
        {
            assert(offset < devices.size());
            uint64_t mask=1ull<<(offset%64);
            if(isActive){
                devices_active_mask[offset/64] |= mask;
            }else{
                devices_active_mask[offset/64] &= ~mask;
            }
        }

        void push(message * &msg)
        {
            assert(msg);
            assert(msg->p_edge);

            message *working=msg;
            msg=0;

            assert(working->p_edge->dest_cluster==this);

            working->next=incoming_queue.load(std::memory_order_relaxed);
            do{
                assert(working->p_edge->dest_cluster==this);
            }while(!incoming_queue.compare_exchange_strong(working->next, working, std::memory_order_release, std::memory_order_relaxed));
        }

        message *pop()
        {
            return incoming_queue.exchange(nullptr, std::memory_order_acquire);
        }
    };

    bool try_send(shared_pool<message>::local_pool &pool, device_cluster *cluster, const void *gp, device *dev, int &nonLocalMessagesSent, int &localMessagesSent)
    {
#ifndef NDEBUG
        //fprintf(stderr, "try_send device %s\n", dev->id);
#endif

        char payload_buffer[SPROVIDER_MAX_PAYLOAD_SIZE];

        int output_port_index=-1;
        int sendIndex=-1;
        int action_taken=-2;
        unsigned size;
        bool active=sprovider_try_send_or_compute(nullptr, dev->device_type_index, gp, dev->properties_then_state, &action_taken, &output_port_index, &size, &sendIndex, payload_buffer);
        assert(
            (action_taken==-2 && output_port_index < 0 && !active)
            ||
            (action_taken==-1 && output_port_index < 0)
            ||
            (action_taken==output_port_index)
        );
        cluster->set_device_active(dev->offset_in_cluster, active);
        if(output_port_index<0){
#ifndef NDEBUG
            //fprintf(stderr, "   no message, action=%d, active=%d\n", action_taken, active);
#endif
            return active; // The device may have done something, but it resulted in no message
        }

#ifndef NDEBUG
        //fprintf(stderr, "   message, action=%d, active=%d\n", action_taken, active);
#endif


        // If we got here then doSend was true

        const auto &output=dev->output_ports[output_port_index];

        const edge *pEdges;
        unsigned nEdges;
        if(sendIndex==-1){
            pEdges=&output.edges[0];
            nEdges=output.edges.size();
        }else{
            pEdges=&output.edges[sendIndex];
            nEdges=1;
        }

        for(unsigned i=0; i<nEdges; i++){
            const auto &edge=pEdges[i];
            if(edge.is_local){
                auto dest_dev=edge.dest_device;
                bool active=sprovider_do_recv(nullptr, dest_dev->device_type_index, edge.pin_index, gp, dest_dev->properties_then_state, edge.properties_then_state, payload_buffer);
                unsigned dest_cluster_offset=edge.dest_device_offset_in_cluster;
                edge.dest_cluster->set_device_active(dest_cluster_offset, active);
#ifndef NDEBUG
                //fprintf(stderr, "     Delivering from %s:%u to %s:%u\n", dev->id, output_port_index, dest_dev->id, dest_dev->device_type_index);
                //dump_state(stderr, dest_dev->cluster, dest_dev);
#endif
                localMessagesSent++;
            }else{
                message *m=pool.alloc();
                assert(m);
                m->next=0;
                m->p_edge=&edge;
                memcpy(m->payload, payload_buffer, size);
                edge.dest_cluster->push(m);
                nonLocalMessagesSent++;
            }
        }
        return true;
    }

    bool try_recv(shared_pool<message>::local_pool &pool, device_cluster *cluster, const void *gp, message *head, int &nonLocalMessagesReceived)
    {
        bool anyActive=false;
        while(head){
            const auto *pedge=head->p_edge;
            assert(pedge);
            auto *dev=pedge->dest_device;
            assert(pedge->dest_cluster==dev->cluster);
            assert(pedge->dest_cluster==cluster);
            assert(dev->cluster==cluster);
            assert(cluster->devices[dev->offset_in_cluster]==dev);
            bool active=sprovider_do_recv(nullptr, dev->device_type_index, pedge->pin_index, gp, dev->properties_then_state, pedge->properties_then_state, head->payload);
            cluster->set_device_active(dev->offset_in_cluster, active); // Any messages mean we need to check rts at some point
            anyActive = anyActive || active;
            auto curr=head;
            head=head->next;
            pool.free(curr);
            nonLocalMessagesReceived++;
            assert(nonLocalMessagesReceived < 1000000);
        }
        return anyActive;
    }



    /*
        A key principle is when processing clusters we always check if there are
        messages to receive. This lets us get round race conditions todo with the
        active flags, hardware detection, and so on - as long as clusters are moving,
        we will always find outstanding messages.
     */
    void step_cluster(
        shared_pool<message>::local_pool &pool,
        const void *gp,
        device_cluster &cluster,
        bool throttleSend,
        unsigned &nonLocalMessagesSent,
        unsigned &nonLocalMessagesReceived
    ){
#ifndef NDEBUG
        cluster.sanity(gp);
#endif

#ifndef NDEBUG
        /* fprintf(stderr, "Step cluster %p\n", &cluster);
        for(auto *d : cluster.devices){
            fprintf(stderr, "  %s : %d\n", d->id, cluster.is_device_active(d->offset_in_cluster));
        }
        */
#endif

        if(cluster.provider_do_hardware_idle){
            assert(cluster.active);
            // We _must_ run hardware idle seperately, as otherwise we might do local
            // delivery into a device which has not yet had hardware idle.
            for(device *dev : cluster.devices){
                assert(!cluster.is_device_active(dev->offset_in_cluster)); // Otherwise how did we get into hardware idle?
                #ifndef NDEBUG
                uint32_t rts=0;
                bool rtc=0;
                sprovider_calc_rts(nullptr, dev->device_type_index, gp, dev->properties_then_state, &rts, &rtc);
                assert(rts==0 && rtc==0);
                #endif
                bool active=sprovider_do_hardware_idle(nullptr, dev->device_type_index, gp, dev->properties_then_state);
                cluster.set_device_active(dev->offset_in_cluster, active);
            }
            cluster.provider_do_hardware_idle=false;
            assert(cluster.active); // We are still active
#ifndef NDEBUG
            cluster.sanity(gp);
#endif
        }

        int nonLocalMessagesSentDelta=0, nonLocalMessagesReceivedDelta=0;
        int localMessagesSentAndReceivedDelta=0;
        
        // Receiving might set interior dev->active high
        auto *head=cluster.pop();
        bool anyActiveFromReceive=try_recv(pool, &cluster, gp, head, nonLocalMessagesReceivedDelta);

        bool anyActive=false;
        if(!throttleSend && (cluster.active || anyActiveFromReceive))
        {
            for(unsigned i_base=0; i_base<cluster.devices_active_mask.size(); i_base++){
                uint64_t m=cluster.devices_active_mask[i_base];
                // TODO: There is some cost to the ctzull even as one instruction. Whether it is
                // worth it depends on the density of bits.
                // This can be hot enough that it might be worth making it adaptive, but
                // not the biggest deal at the moment.
                /*
                unsigned i_off=__builtin_ctzll(m);
                while(m){
                    unsigned i=i_base*64+i_off;
                    m=m&~(1ull<<i_off);
                    i_off=__builtin_ctzll(m);
                    anyActive=anyActive or try_send(pool, gp, cluster.devices[i], nonLocalMessagesSentDelta, localMessagesSentAndReceivedDelta);
                }
                */
                unsigned i=i_base*64;
                while(m){
                    if(m&1){
                        bool active=try_send(pool, &cluster, gp, cluster.devices[i], nonLocalMessagesSentDelta, localMessagesSentAndReceivedDelta);
                        anyActive=anyActive or active;
                    }
                    m>>=1;
                    i++;
                }
            }
        }

#ifndef NDEBUG
        cluster.sanity(gp);
#endif

        cluster.active=anyActive || throttleSend;

        nonLocalMessagesSent=nonLocalMessagesSentDelta;
        nonLocalMessagesReceived=nonLocalMessagesReceivedDelta;

        cluster.nonLocalMessagesSent+=nonLocalMessagesSentDelta;
        cluster.nonLocalMessagesReceived+=nonLocalMessagesReceivedDelta;
        cluster.localMessagesSentAndReceived+= localMessagesSentAndReceivedDelta;
        cluster.numClusterSteps++;
        cluster.numNoSendClusterSteps += (nonLocalMessagesSentDelta==0) && (localMessagesSentAndReceivedDelta==0);
        cluster.numNoActivityClusterSteps += (nonLocalMessagesSentDelta==0) && (localMessagesSentAndReceivedDelta==0) && (nonLocalMessagesReceivedDelta==0);
    }

    // Target number of devices in a cluster
    // If we assume about 100 instructions per device in the average
    // case (a bit optimistic), then we get about 100000 instructions
    // per cluster. That's
    // going to be worse in the case where it is racing through
    // on the way to idle, but probably not less than 25 instructions
    // per device (again, optimistic), so still up around 25K instructions.
    // Most expensive
    // thing is the idle detection atomics, but they become cheaper the
    // more work is going on.
    unsigned m_cluster_size=1024;

    // This really shouldn't live here
    bool use_metis=true;

    const void *m_gp;
    std::vector<device*> m_devices;
    std::vector<device_cluster*> m_clusters;
    tbb::concurrent_queue<device_cluster*> m_cluster_queue;
    //moodycamel::ConcurrentQueue<device_cluster*> m_cluster_queue;

    uint64_t total_received_messages_approx()
    {
        uint64_t received=m_globalNonLocalReceives.load();
        for(auto *c : m_clusters){
            // Warning: this is _not_ synchronised
            received += c->localMessagesSentAndReceived;
        }
        return received;
    }

    void sanity()
    {
        std::unordered_set<device_cluster*> clusters(m_clusters.begin(), m_clusters.end());
        assert(clusters.size()==m_clusters.size());

        for(auto *d : m_devices){
            d->sanity();
            assert( clusters.find(d->cluster) != clusters.end() );
        }
        for(auto *c : m_clusters){
            c->sanity(m_gp);
        }
    }

    /* Idle detection.
    
        If a cluster is idle (no receives or sends in a pass), then there is a chance that the whole thing has gone idle.
        We take a heuristic approach to this, by first trying to guess when things are idle, then switching to
        a correct approach when it seems very likely we are idle.

        There is a global atomic counter which tracks the number of consecutive idle clusters processed across all threads.
        Any time a thread processes an idle cluster it increments the counter, and every time it processes a
        non-idle cluster it resets it to zero. We also track the number of non-local messages that have been
        sent and received. So it is _likely_ that we are idle if the following conditions hold:
        - consecutiveIdle > numClusters+SAFETY = INACTIVE_THRESHOLD
        - nonLocalSent == nonLocalReceived

        INACTIVE_THRESHOLD should be quite conservative, 
        
        These three counters are a potential bottleneck, so we need to make sure clusters are large enough to amortise this cust.
        They should probably all be in the same cache line as well, as typically they are all accessed together.

        When a thread finds the inactive counter exceeds INACTIVE_THRESHOLD and sent==received, it will block. As each following
        thread hits the same counter it will block on the same lock. When the final thread enters the lock, it will verify
        that:
        - non-local sent == non-local received
        - each cluster is inactive.
        Once it has checked each cluster, it sets the provider_do_hardware_idle flag, and then wakes up all the threads.

        If it turns out that the wake-up was incorrect, it resets the idle wait count, and wakes up the other threads.

        We can be sure that all pending hardware idles must be dealt with before another idle is possible,
        as any cluster that still has pending hardware idle is also marked as active.
     */

    
    uint8_t _pad1_[128];
    std::atomic<uint64_t> m_globalNonLocalSends;
    std::atomic<uint64_t> m_globalNonLocalReceives;
    std::atomic<uint64_t> m_globalInactiveClusters;
    std::atomic<unsigned> m_idleDetectionWaiters; // Number of threads currently blocked on idle detection
    uint8_t _pad2_[128];

    uint64_t m_lastCheckpointTotalMessageCount=-1;
    unsigned m_nonMessageRoundCount=0;
    unsigned m_maxNonMessageRoundCount=10;

    unsigned m_idleDetectionInactiveThreshold; // Number of clusters plus some safety factor
    std::mutex m_idleDetectionMutex;
    std::condition_variable m_idleDetectionCond;

    void check_for_idle_verify(unsigned nThreads)
    {
        /////////////////////////////////////////////////////////////////
        // Begin lock
        std::unique_lock<std::mutex> lk(m_idleDetectionMutex);
        unsigned waiters=1+m_idleDetectionWaiters.fetch_add(1);
        if(waiters<nThreads){
            // Either we went to sleep erroneously, or there was an idle. Either way we don't care
            // and just leave when the inactive clusters drops below the threshold
            while(m_globalInactiveClusters.load() >= m_idleDetectionInactiveThreshold){
                m_idleDetectionCond.wait(lk);
            }
        }else{
            // We are the final thread.

            // Have to capture once more, to get the definitive final version
            uint64_t totalNonLocalSent=m_globalNonLocalSends.load();
            uint64_t totalNonLocalReceived=m_globalNonLocalReceives.load();

            if(totalNonLocalSent!=totalNonLocalReceived){
                assert(totalNonLocalSent > totalNonLocalReceived); // Should be a consistent view at this point
                // We have not yet reached idle as there are messages in flight. So there is some message
                // still to be delivered within some cluster.
            }else{
                // Verify that all clusters are inactive
                uint64_t totalLocalMessages=0;
                bool active=false;
                for(auto *cluster : m_clusters){
                    if(cluster->active){
                        active=true;
                        break;
                    }
                    totalLocalMessages += cluster->localMessagesSentAndReceived;

                    #ifndef NDEBUG
                    for(auto *dev : cluster->devices){
                        uint32_t rts=0;
                        bool rtc=0;
                        bool aa=sprovider_calc_rts(nullptr, dev->device_type_index, m_gp, dev->properties_then_state, &rts, &rtc);
                        assert(!aa);
                        assert( aa ? 1 : rts==0 && rtc==0);
                        assert(rts==0 && rtc==0);        
                    }
                    #endif
                }
                if(!active){
                    // Yes! We have reach idle: all messages sent have been received, and no cluster is active
                    uint64_t totalMessages=totalNonLocalSent+totalLocalMessages;
                    fprintf(stderr, "Idle: nonLocal=%llu, local=%llu, total=%llu\n", (unsigned long long)totalNonLocalSent, (unsigned long long)totalLocalMessages, (unsigned long long)totalMessages);

                    if(m_lastCheckpointTotalMessageCount!=totalMessages){
                        m_nonMessageRoundCount=0;
                        m_lastCheckpointTotalMessageCount=totalMessages;
                    }else{
                        m_nonMessageRoundCount++;
                        if(m_nonMessageRoundCount>m_maxNonMessageRoundCount){
                            fprintf(stderr, "Error: system has completed %u idle steps without sending a message. Terminating.\n", m_nonMessageRoundCount);
                            exit(1);
                        }
                    }

                    for(auto *cluster : m_clusters){
                        cluster->active=true; // Wake up the cluster
                        cluster->provider_do_hardware_idle=true; // And indicate idle needs to be run
                    }
                    // And that's it!
                }
            }

            // Regardless of the outcome we need to start idle detection again
            m_globalInactiveClusters.store(0); // Start the process again
            m_idleDetectionCond.notify_all(); // Wake up all the threads
        }
        m_idleDetectionWaiters.fetch_sub(1);
        // End lock
        //////////////////////////////////////////////////

    }

    void check_for_idle(unsigned nThreads, bool clusterActive, unsigned nonLocalSent, unsigned nonLocalReceived)
    {
        // We can't save the values for these until we get into the mutex. We also
        // want to avoid updating if nothing is happening, as that is when lots of
        // threads will be racing through here with no sends or receives, just before
        // idle
        if(nonLocalSent){
            m_globalNonLocalSends.fetch_add(nonLocalSent, std::memory_order_relaxed);
        }
        if(nonLocalReceived){
            m_globalNonLocalReceives.fetch_add(nonLocalReceived, std::memory_order_relaxed);
        }

        if(clusterActive){
            // Cancel anyone attempting to idle detect for now. 
            m_globalInactiveClusters.store(0);
            // Stop anyone else who is trying. This condition should be unlikely unless we are approaching idle
            if(m_idleDetectionWaiters.load() > 0){
                m_idleDetectionCond.notify_all(); // Wake up everyone that went to sleep by mistake
            }
        }else{
            // Start the route towards idle detection
            uint64_t totalInactiveClusters=m_globalInactiveClusters.fetch_add(1);
            if(totalInactiveClusters < m_idleDetectionInactiveThreshold){
                // We haven't reached the point where idleness is likely. Just carry on
            }else{
                // Capture a possibly inconsistent view of these variables. Note that they may not
                // be exact, unless we are the final thread.
                uint64_t totalNonLocalSent=m_globalNonLocalSends.load();
                uint64_t totalNonLocalReceived=m_globalNonLocalReceives.load();

                if(totalNonLocalSent!=totalNonLocalReceived){
                    // Send/receive count is not yet stable
                }else{
                    // Heuristic has been passed. It is quite likely we are now idle, so need to
                    // start verifying.
                    check_for_idle_verify(nThreads);
                }
            }
        }

    }


    void run(unsigned nThreads)
    {
        shared_pool<message> gpool(sizeof(message)+SPROVIDER_MAX_PAYLOAD_SIZE);

        std::atomic<bool> quit;
        quit.store(false);
        std::condition_variable quitCond;
        std::mutex quitMutex;

        const int IN_FLIGHT_THROTTLE=1<<20;

        m_globalNonLocalReceives=0;
        m_globalNonLocalSends=0;
        m_globalInactiveClusters=0;
        m_idleDetectionWaiters=0;
        m_idleDetectionInactiveThreshold=2*m_clusters.size();

        for(auto *cluster : m_clusters){
            m_cluster_queue.push(cluster);
        }

        nThreads=std::min(nThreads, (unsigned)m_clusters.size());
        std::vector<std::thread> threads;

        threads.push_back(std::thread([&]{
            std::unique_lock<std::mutex> lk(quitMutex);
            while(!quit.load()){
                quitCond.wait_for(lk, std::chrono::milliseconds(500));

                fprintf(stderr, "nlInFlight=%lld, nlRecvs=%llu, allRecvs=%llu, inactiveClustes=%llu, allocBytes = %f MBytes\n",
                     (long long)((int64_t)m_globalNonLocalSends.load()-m_globalNonLocalReceives.load()),
                     (unsigned long long)m_globalNonLocalReceives.load(),
                     (unsigned long long)total_received_messages_approx(),
                    (unsigned long long)m_globalInactiveClusters.load(),
                    gpool.get_alloced_bytes()/(1024.0*1024.0)
                );
            }
        }));

        if(nThreads==1){
            auto lpool=gpool.create_local_pool();
            while(!quit.load(std::memory_order_relaxed)){
                device_cluster *cluster=0;
                if(!m_cluster_queue.try_pop(cluster)){
                    // Is this possible due to slight delays?
                    throw std::runtime_error("Attempt to pop failed.");
                }
                unsigned sent=0, received=0;
                bool throttleSend = IN_FLIGHT_THROTTLE < ((int64_t)m_globalNonLocalSends.load(std::memory_order_relaxed)-m_globalNonLocalReceives.load(std::memory_order_relaxed));
                step_cluster(lpool, m_gp, *cluster, throttleSend, sent, received);
                bool active=cluster->active;
                m_cluster_queue.push(cluster);
                check_for_idle(nThreads, active, sent, received);
            }
        }else if(nThreads==m_clusters.size()){
            for(unsigned i=0; i<nThreads; i++){
                threads.emplace_back( std::thread([&, i](){
                    auto lpool=gpool.create_local_pool();

                    while(!quit.load(std::memory_order_relaxed)){
                        unsigned sent=0, received=0;
                        bool throttleSend = IN_FLIGHT_THROTTLE < ((int64_t)m_globalNonLocalSends.load(std::memory_order_relaxed)-m_globalNonLocalReceives.load(std::memory_order_relaxed));
                        step_cluster(lpool, m_gp, *m_clusters[i], throttleSend, sent, received);
                        check_for_idle(nThreads, m_clusters[i]->active, sent, received);
                    }
                }));
            }
        }else{
            for(unsigned i=0; i<nThreads; i++){
                threads.emplace_back( std::thread([&](){
                    auto lpool=gpool.create_local_pool();

                    while(!quit.load(std::memory_order_relaxed)){
                        device_cluster *cluster=0;
                        if(!m_cluster_queue.try_pop(cluster)){
                            // Is this possible due to slight delays?
                            throw std::runtime_error("Attempt to pop failed.");
                        }
                        std::atomic_thread_fence(std::memory_order_seq_cst);
                        unsigned sent=0, received=0;
                        bool throttleSend = IN_FLIGHT_THROTTLE < ((int64_t)m_globalNonLocalSends.load(std::memory_order_relaxed)-m_globalNonLocalReceives.load(std::memory_order_relaxed));
                        step_cluster(lpool, m_gp, *cluster, throttleSend, sent, received);
                        std::atomic_thread_fence(std::memory_order_seq_cst);
                        bool active=cluster->active;
                        m_cluster_queue.push(cluster);
                        check_for_idle(nThreads, active, sent, received);
                    }
                }));
            }
        }

        for(auto &thread : threads){
            thread.join();
        }
    }

    void dump_state(FILE *dst, device_cluster *cluster, device *dev)
    {
#ifndef NDEBUG
        fprintf(dst, "  Device %s : %s\n", dev->id, SPROVIDER_DEVICE_TYPE_INFO[dev->device_type_index].id);
#else   
    fprintf(dst, "  Device %p : %s\n", dev, SPROVIDER_DEVICE_TYPE_INFO[dev->device_type_index].id);
#endif
        uint32_t rts=0;
        bool rtc=false;
        bool active=cluster->is_device_active(dev->offset_in_cluster);
        bool gactive=sprovider_calc_rts(nullptr, dev->device_type_index, m_gp, dev->properties_then_state, &rts, &rtc);
        fprintf(dst, "    rts=0x%x, rtc=%u, active=%u,  sys active=%u\n", rts, rtc, gactive, active);
#ifndef NDEBUG
        fprintf(dst, "    properties=%s\n", get_json_properties(dev->device_type, dev->properties_then_state  ).c_str());
        std::string sss=get_json_state(dev->device_type, dev->properties_then_state  );
        fprintf(dst, "         state=%s\n", sss.c_str());
#endif
    }

    void dump_state(FILE *dst)
    {
        for(auto *cluster : m_clusters){
            fprintf(dst, "Cluster %p:\n", cluster);
            for(auto *dev : cluster->devices){
                dump_state(dst, cluster, dev);        
            }
            fprintf(dst, "Head message = %p\n", cluster->incoming_queue.load());
        }
    }

    void add_edge(edge_vector &ev, unsigned handler_index);
};



//////////////////////////////////////////////////////////////
// Building a graph

class POEMSBuilder
    : public GraphLoadEvents
{
public:
    using device = POEMS::device;
    using edge = POEMS::edge;    
    using device_cluster = POEMS::device_cluster;    
private:
    POEMS &m_target;
    std::vector<DeviceTypePtr> m_deviceTypes;
    std::unordered_map<std::string,unsigned> m_deviceIdToIndex;

    unsigned device_type_id_to_index(const std::string &s)
    {
        /*
        For moderate numbers of device types it is faster to just search than to
        build a hash-table. Shouldn't be the main bottlneck anyway.
         */
        for(unsigned i=0; i<SPROVIDER_DEVICE_TYPE_COUNT; i++){
            if(!strcmp(SPROVIDER_DEVICE_TYPE_INFO[i].id, s.c_str())){
                return i;
            }
        }
        throw std::runtime_error("Device type id '"+s+"' not known to static provider.");
    }
public:
    POEMSBuilder(POEMS &target)
        : m_target(target)
    {}

    std::function<void(const std::string &, const std::string &, const std::string &)> stats_log
        = [](const std::string &, const std::string &, const std::string &)
        {};

    void check_size( const char *thing_type, const char *thing_id, unsigned esize, const TypedDataPtr &value)
    {
        unsigned gsize = value ? value.payloadSize() : 0;
        if(gsize!=esize){
            fprintf(stderr, "Found %s %s with expected size of %u but got size %u. Graph mismatch?\n", thing_type, thing_id, esize, gsize);
            exit(1);
        }
    }


    uint64_t onBeginGraphInstance(
    const GraphTypePtr &graph,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
    ) override
    {
        stats_log("loading", "onBeginGraphInstance", std::to_string( now() ));

        if(graph->getId()!=SPROVIDER_GRAPH_TYPE_INFO.id){
            fprintf(stderr, "Simulator is compiled for graph type '%s', but file contained '%s'\n", SPROVIDER_GRAPH_TYPE_INFO.id, graph->getId().c_str());
            exit(1);
        }

        std::vector<DeviceTypePtr> device_types=graph->getDeviceTypes();
        for(unsigned i=0; i<device_types.size(); i++){
            auto dt=device_types[i];
            const auto &dti=SPROVIDER_DEVICE_TYPE_INFO[i];
            if(dt->getId()!=dti.id){
                fprintf(stderr, "Expected device index %u to have id %s, but got %s\n", i, dti.id, dt->getId().c_str());
                exit(1);
            }

            unsigned expected_state_size=dti.state_size;
            unsigned expected_properties_size=dti.properties_size;
            unsigned got_state_size=dt->getStateSpec() ? dt->getStateSpec()->payloadSize() : 0;
            unsigned got_properties_size=dt->getPropertiesSpec() ? dt->getPropertiesSpec()->payloadSize() : 0;

            if(expected_state_size!=got_state_size){
                fprintf(stderr, "Device type %s has state size of %u internally, but graph loaded thinks it is %u\n.",
                    dt->getId().c_str(), expected_state_size, got_state_size
                );
                auto def=dt->getStateSpec()->create();
                std::cerr<<"Default state = "<<dt->getStateSpec()->toJSON(def)<<"\n";
                exit(1);
            }
        }

        check_size("graph properties", id.c_str(), SPROVIDER_GRAPH_TYPE_INFO.properties_size, properties);
        m_target.m_gp=alloc_copy_P(properties);
        return 0;
    }

    void dump_tds(std::string name, TypedDataSpecPtr spec, TypedDataPtr val)
    {

        std::cerr<<name<<" = "<< (spec ? spec->toJSON(val) : "None") <<"\n";
    }

  uint64_t onDeviceInstance
  (
   uint64_t graphInst,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
   rapidjson::Document &&metadata=rapidjson::Document()
  ) override
  {
    unsigned device_type_index=device_type_id_to_index(dt->getId());

    unsigned index=m_deviceIdToIndex.size();
    m_deviceIdToIndex.insert(std::make_pair(id, index));

    unsigned expected_state_size=SPROVIDER_DEVICE_TYPE_INFO[device_type_index].state_size;
    unsigned expected_properties_size=SPROVIDER_DEVICE_TYPE_INFO[device_type_index].properties_size;

    check_size("device properties", id.c_str(), expected_properties_size, properties);
    check_size("device state", id.c_str(), expected_state_size, state);

    unsigned dev_P_S_size=sizeof(device)+calc_P_S_size(properties,state);
    device *dev=new (dev_P_S_size) device();
    dev->cluster=0;
    dev->offset_in_cluster=-1;
    dev->device_type_index=device_type_index;
    copy_P_S( dev->properties_then_state, properties, state );

    dev->output_ports.resize(dt->getOutputCount());

    #ifndef NDEBUG
    dev->id=strdup(id.c_str());
    dev->device_type=dt;
    #endif
/*
    std::cerr<<"Instance "<<id<<"\n";
    dump_tds("  props", dt->getPropertiesSpec(), properties);
    dump_tds("  state", dt->getStateSpec(), state);
*/
    m_target.m_devices.push_back(dev);

    sprovider_do_init(nullptr, dev->device_type_index, m_target.m_gp, dev->properties_then_state);
    
    return index;
  }

void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   int sendIndex, // -1 if it is not indexed pin, or if index is not explicitly specified
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
    rapidjson::Document &&metadata=rapidjson::Document()
  ) override
  {
    edge e;
    e.dest_device=m_target.m_devices.at(dstDevInst);
    e.pin_index=dstPin->getIndex();
    e.is_local=false; // No local stuff to start with.
    e.send_index=sendIndex; // Could be -1 or a real send_index
    auto &output=m_target.m_devices.at(srcDevInst)->output_ports.at(srcPin->getIndex());
    unsigned output_edge_offset=output.edges.size();
    unsigned output_p_s_offset=output.data.size();
    unsigned output_p_s_size=calc_P_S_size(properties,state);

    const auto &input_info=SPROVIDER_DEVICE_TYPE_INFO[e.dest_device->device_type_index].inputs[e.pin_index];
    check_size("edge properties", "?", input_info.properties_size, properties);
    check_size("edge state", "?", input_info.state_size, state);

    output.data.resize(output.data.size()+output_p_s_size);
    copy_P_S(output.data.data()+output_p_s_offset, properties, state);

    e.properties_then_state_offset=output_p_s_offset;

    output.edges.push_back(e);
  }

  void onEndEdgeInstances(uint64_t /*graphToken*/) override
  {
    for(device *d : m_target.m_devices){
        for(POEMS::edge_vector & op : d->output_ports){
            for(edge & e : op.edges){
                e.properties_then_state = op.data.data()+e.properties_then_state_offset;

            } 
        }
    }
  }

  void assign_clusters_random(std::vector<device*> &devices, std::vector<device_cluster*> &clusters)
  {
      unsigned nClusters=clusters.size();
    for(unsigned i=0; i<devices.size(); i++){
        auto d=devices[i];
        auto c=m_target.m_clusters[i%nClusters];
        unsigned offset=c->devices.size();
        c->devices.push_back(d);
        d->cluster=c;
        d->offset_in_cluster=offset;
    }
  }

void assign_clusters_metis(std::vector<device*> &devices, std::vector<device_cluster*> &clusters)
{
    fprintf(stderr, "Assigning giant cluster indices\n");
    // Pretend we have one giant cluster, and assign indices
    for(unsigned i=0; i<devices.size(); i++){
        auto *d = devices[i];
        d->offset_in_cluster=i;
    }

    fprintf(stderr, "Building weighted graph\n");
    // Build a weighted graph
    std::vector<std::unordered_map<int,float> > graph;
    graph.resize(devices.size());

    for(device *d : devices){
        unsigned src_i=d->offset_in_cluster;
        for(auto &ev : d->output_ports){
            for(edge &e : ev.edges){
                unsigned dst_i=e.dest_device->offset_in_cluster;
                // Edges must be bidirectional for metis
                graph[src_i][dst_i] += 1;
                graph[dst_i][src_i] += 1;
            }
        }
    }

    fprintf(stderr, "Converting to metis\n");
    // Convert to metis
    std::vector<idx_t> xadj;
    std::vector<idx_t> adjncy;
    std::vector<idx_t> adjwgt;

    unsigned start_i=0;
    for(unsigned src_i=0; src_i<graph.size(); src_i++){    
        xadj.push_back(start_i);
        const auto &edges = graph[src_i];
        for(const auto &e : edges){
            adjncy.push_back(e.first);
            adjwgt.push_back(e.second);
        }
        start_i=adjncy.size();
    }
    xadj.push_back(start_i);

    idx_t nvtxs=devices.size();
    idx_t ncon=1; // Needs to be at least 1 ?
    idx_t nparts=clusters.size();
    idx_t objval=0;
    std::vector<idx_t> part(nvtxs);
    int code=METIS_PartGraphRecursive(
        &nvtxs, &ncon, &xadj[0], &adjncy[0], NULL /* vwgt*/, NULL /* vsize */, &adjwgt[0], &nparts, NULL /* tpwgts */,
            NULL /* ubvec */, NULL/*options*/, &objval, &part[0]);
    if(code!=METIS_OK){
        throw std::runtime_error("Error from metis.");
    }

    fprintf(stderr, "Applying metis partition\n");
    for(unsigned i=0; i<devices.size(); i++){
        auto d=devices[i];
        unsigned pi=part[d->offset_in_cluster];
        auto c=m_target.m_clusters[pi];
        unsigned offset=c->devices.size();
        c->devices.push_back(d);
        d->cluster=c;
        d->offset_in_cluster=offset;
    }
  }

  void onEndGraphInstance(uint64_t /*graphToken*/) override
  {
      std::mt19937 urng;

        /////////////////////////////////////////////////////////////////////
        // Task one: sort out any explicit send indices

        for(device *d : m_target.m_devices){
            const auto &dinfo=SPROVIDER_DEVICE_TYPE_INFO[d->device_type_index];
            for(unsigned i=0; i<dinfo.output_count; i++){
                std::vector<edge> &edges=d->output_ports[i].edges;
                if(edges.empty()){
                    continue;
                }

                if(edges.front().send_index==-1){
                    // At least one implicit, so all must be implicit
                    for(auto &e : edges){
                        if(e.send_index!=-1){
                            throw std::runtime_error("Mix of explicit and implicit send indices.");
                        }
                    }
                }else{
                    std::sort(edges.begin(), edges.end(), [](const edge &a, const edge &b){ return a.send_index < b.send_index;  });
                    for(unsigned i=0; i<edges.size(); i++){
                        if(edges[i].send_index!=i){
                            throw std::runtime_error("Send indices are either not contiguous, or there is a mix of explicit and implicit.");
                        }
                    }
                }
            }
        }


        // At this point send_index data has all been used, and will be destroyed.

        ///////////////////////////////////////////////////////////////////////
        // Task two: cluster assignment

      std::vector<device*> devices(m_target.m_devices);

      std::shuffle(devices.begin(), devices.end(), urng);

      unsigned nClusters=std::max(1u, unsigned(devices.size() / m_target.m_cluster_size));
      
      fprintf(stderr, "Splitting %u devices into %u clusters; about %u devices/cluster\n", unsigned(devices.size()), nClusters, unsigned(devices.size()/nClusters));
      
      assert(m_target.m_clusters.empty());
      m_target.m_clusters.reserve(nClusters);
      for(unsigned i=0; i<nClusters; i++){
          m_target.m_clusters.push_back(new device_cluster());
      }

      if(m_target.use_metis && nClusters>1){
          assign_clusters_metis(devices, m_target.m_clusters);
      }else{
          assign_clusters_random(devices, m_target.m_clusters);
      }

      int locals=0;
      int nonLocals=0;

      /* For space/simplicity reasons we don't maintain back edges from devices
         back to their incoming edges. That means that to set the edge to cluster
         binding for the destination we have to follow it from the source. */
        for(auto *c : m_target.m_clusters){
            c->active=true;
            c->provider_do_hardware_idle=false;

            unsigned nDevices=c->devices.size();
            c->devices_active_mask.clear();
            c->devices_active_mask.resize((nDevices+63)/64, 0);

            for(unsigned i=0; i<nDevices; i++){
                auto d = c->devices[i];
                uint32_t rts=0;
                bool rtc=false;
                bool active=sprovider_calc_rts(nullptr, d->device_type_index, m_target.m_gp, d->properties_then_state, &rts, &rtc);
                c->set_device_active(i, active);

                for(auto &o : d->output_ports){
                    for(edge &e : o.edges){
                        auto *dc=e.dest_device->cluster;
                        if( dc == c ){
                            e.is_local=true;
                            locals++;
                        }else{
                            nonLocals++;
                        }
                        e.dest_cluster=dc;
                        e.dest_device_offset_in_cluster=e.dest_device->offset_in_cluster;
                    }
                }
            }
        }

        m_target.sanity();

      fprintf(stderr, "Made %u of %u edges local (%f%%).\n", locals, nonLocals+locals, locals*100.0/(locals+nonLocals));
  }



};

#endif
