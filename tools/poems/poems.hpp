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

#include <metis.h>
#include <tbb/concurrent_queue.h>
#include "contrib/concurrentqueue.hpp"

#include "shared_pool.hpp"

#include "../../include/graph_persist.hpp"

#include "../sprovider/sprovider_types.h"

//////////////////////////////////////////////////
// Simulation logic

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
        uint32_t dest_device_offset_in_cluster;
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

        void sanity()
        {
            for(unsigned i=0; i<devices.size();i++){
                assert(devices[i]->cluster==this);
                assert(devices[i]->offset_in_cluster==i);
                devices[i]->sanity();
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

    static bool try_send(shared_pool<message>::local_pool &pool, device_cluster *cluster, const void *gp, device *dev, int &nonLocalMessagesSent, int &localMessagesSent)
    {
        char payload_buffer[SPROVIDER_MAX_PAYLOAD_SIZE];

        int output_port_index=-1;
        int sendIndex=-1;
        int action_taken=-2;
        unsigned size;
        bool active=sprovider_try_send_or_compute(nullptr, dev->device_type_index, gp, dev->properties_then_state, &action_taken, &output_port_index, &size, &sendIndex, payload_buffer);
        cluster->set_device_active(dev->offset_in_cluster, active);
        if(output_port_index==-1){
            return active; // The device may have done something, but it resulted in no message
        }

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

    static bool try_recv(shared_pool<message>::local_pool &pool, device_cluster *cluster, const void *gp, message *head, int &nonLocalMessagesReceived)
    {
        bool anyActive=false;
        while(head){
            const auto *pedge=head->p_edge;
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
    static void step_cluster(
        shared_pool<message>::local_pool &pool,
        const void *gp,
        device_cluster &cluster,
        unsigned &nonLocalMessagesSent,
        unsigned &nonLocalMessagesReceived
    ){
#ifndef NDEBUG
        cluster.sanity();
#endif

        if(cluster.provider_do_hardware_idle){
            assert(cluster.active);
            // We _must_ run hardware idle seperately, as otherwise we might do local
            // delivery into a device which has not yet had hardware idle.
            for(device *dev : cluster.devices){
                assert(cluster.is_device_active(dev->offset_in_cluster)); // Otherwise how did we get into hardware idle?
                sprovider_do_hardware_idle(nullptr, dev->device_type_index, gp, dev->properties_then_state);
                // The device is still ready to send
            }
            cluster.provider_do_hardware_idle=false;
            assert(cluster.active); // We are still active
        }

        int nonLocalMessagesSentDelta=0, nonLocalMessagesReceivedDelta=0;
        int localMessagesSentAndReceivedDelta=0;
        
        // Receiving might set interior dev->active high
        auto *head=cluster.pop();
        bool anyActiveFromReceive=try_recv(pool, &cluster, gp, head, nonLocalMessagesReceivedDelta);

        bool anyActive=false;
        if(cluster.active || anyActiveFromReceive)
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
                        anyActive=anyActive or try_send(pool, &cluster, gp, cluster.devices[i], nonLocalMessagesSentDelta, localMessagesSentAndReceivedDelta);
                    }
                    m>>=1;
                    i++;
                }
            }
        }

        cluster.active=anyActive;

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

    void sanity()
    {
        std::unordered_set<device_cluster*> clusters(m_clusters.begin(), m_clusters.end());
        assert(clusters.size()==m_clusters.size());

        for(auto *d : m_devices){
            d->sanity();
            assert( clusters.find(d->cluster) != clusters.end() );
        }
        for(auto *c : m_clusters){
            c->sanity();
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
                }
                if(!active){
                    // Yes! We have reach idle: all messages sent have been received, and no cluster is active
                    fprintf(stderr, "Idle: nonLocal=%llu, local=%llu\n", (unsigned long long)totalNonLocalSent, (unsigned long long)totalLocalMessages);

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
                fprintf(stderr, "nlSends=%llu, nlRecvs=%llu, inactiveClustes=%llu, allocBytes = %f MBytes\n",
                    (unsigned long long)m_globalNonLocalSends.load(), (unsigned long long)m_globalNonLocalReceives.load(),
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
                step_cluster(lpool, m_gp, *cluster, sent, received);
                bool active=cluster->active;
                m_cluster_queue.push(cluster);
                check_for_idle(nThreads, active, sent, received);
            }
        }else if(nThreads==m_clusters.size()){
            for(unsigned i=0; i<nThreads; i++){
                threads.emplace_back( std::thread([&, i](){
                    fprintf(stderr, "Thread starting.\n");
                    auto lpool=gpool.create_local_pool();

                    fprintf(stderr, "Loop starting.\n");
                    while(!quit.load(std::memory_order_relaxed)){
                      fprintf(stderr, "Top o loop.\n");
                        unsigned sent=0, received=0;
                        step_cluster(lpool, m_gp, *m_clusters[i], sent, received);
                        check_for_idle(nThreads, m_clusters[i]->active, sent, received);
                    }
                    fprintf(stderr, "Thread Exiting.\n");
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
                        step_cluster(lpool, m_gp, *cluster, sent, received);
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

    void add_edge(edge_vector &ev, unsigned handler_index);
};



#include "../sprovider/sprovider_helpers.hpp"



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

  uint64_t onBeginGraphInstance(
    const GraphTypePtr &graph,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ) override
  {
    m_target.m_gp=alloc_copy_P(properties);
    return 0;
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
    unsigned index=m_deviceIdToIndex.size();
    m_deviceIdToIndex.insert(std::make_pair(id, index));

    unsigned dev_P_S_size=sizeof(device)+calc_P_S_size(properties,state);
    device *dev=new (dev_P_S_size) device();
    dev->cluster=0;
    dev->offset_in_cluster=-1;
    dev->device_type_index=device_type_id_to_index(dt->getId());
    copy_P_S( dev->properties_then_state, properties, state );

    dev->output_ports.resize(dt->getOutputCount());

    m_target.m_devices.push_back(dev);
    
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
    if(sendIndex!=-1){
        // Could be supported, but need a shadow data structure to store the indices while building
        throw std::runtime_error("Explicit pin indices not yet supported (implicit ok).");
    }

    edge e;
    e.dest_device=m_target.m_devices.at(dstDevInst);
    e.pin_index=dstPin->getIndex();
    e.is_local=false; // No local stuff to start with.
    auto &output=m_target.m_devices.at(srcDevInst)->output_ports.at(srcPin->getIndex());
    unsigned output_edge_offset=output.edges.size();
    unsigned output_p_s_offset=output.data.size();
    unsigned output_p_s_size=calc_P_S_size(properties,state);

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

      std::vector<device*> devices(m_target.m_devices);

      std::shuffle(devices.begin(), devices.end(), urng);

      unsigned nClusters=std::max(1u, unsigned(devices.size() / m_target.m_cluster_size));
      
      fprintf(stderr, "Splitting %u devices into %u clusters; about %u devices/cluster\n", devices.size(), nClusters, unsigned(devices.size()/nClusters));
      
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
                sprovider_do_init(nullptr, d->device_type_index, m_target.m_gp, d->properties_then_state);

                c->set_device_active(i, true);

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
