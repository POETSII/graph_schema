#ifndef simulator_context_hpp
#define simulator_context_hpp

#include "graph.hpp"
#include "graph_persist.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <vector>
#include <memory>
#include <algorithm>
#include <queue>

template<class T>
class FIFOSet
{
private:
    std::list<const T*> m_order;
    std::unordered_map<T,typename std::list<const T*>::const_iterator> m_index;
public:
    bool empty() const
    { return m_order.empty(); }

    const T &front()
    {
        assert(!empty());
        return *m_order.front();
    }

    void pop_front()
    {
        assert(!empty());
        const T *pVal=m_order.front();
        m_order.pop_front();
        auto it=m_index.find(*pVal);
        m_index.erase(it);
    }

    // If x is already present then do nothing
    void push_back(const T &x)
    {
        auto it=m_index.find(x);
        if(it!=m_index.end()){
            return;
        }

        // Reserve space at end of list
        m_order.push_back(nullptr); 

        // Insert into the map with iterator pointing to back of list
        auto lit=m_order.end();
        --lit;
        it=m_index.insert(it,std::make_pair(x, lit ));

        // Update the list with pointer to the key
        m_order.back() = &it->first;
    }

    void erase(const T &x)
    {
        auto it=m_index.find(x);
        if(it!=m_index.end()){
            return;
        }

        m_order.erase(it->second);
        m_index.erase(it);
    }

    void move_front_to_back()
    {
        assert(!empty());
        const T *pVal=m_order.front();
        auto it=m_index.find(*pVal);

        m_order.splice(m_order.end(), m_order, it->second);
        auto lit=m_order.end();
        --lit;
        it->second=lit;
    }

};

class StringInterner;

struct interned_string_t
{
private:
    const std::string *value;
#ifndef NDEBUG
    std::shared_ptr<StringInterner> owner;
#endif
public:
    interned_string_t()
        : value(nullptr)
    {}

    interned_string_t(const std::shared_ptr<StringInterner> &_owner, const std::string *_value)
        : value(_value)
        #ifndef NDEBUG
        , owner(_owner)
        #endif
    {}

    bool operator ==(const interned_string_t &o) const
    {
        assert(owner==o.owner);
        return value==o.value;
    }

    bool operator <(const interned_string_t &o) const
    {
        assert(owner==o.owner);
        return value<o.value;
    }

    operator const std::string &() const
    {
        assert(value);
        return *value;
    }

    const char *c_str() const
    {
        assert(value);
        return value->c_str();
    }
};

class StringInterner
    : public std::enable_shared_from_this<StringInterner>
{
private:
    friend std::hash<interned_string_t>;

    std::unordered_set<std::string> strings;
public:
    interned_string_t intern(const std::string &v)
    {
        // Pointers to unordered_map elements should be stable
        auto it=strings.insert(v);
        const std::string *p=&*it.first;
#ifndef NDEBUG
        return interned_string_t(shared_from_this(),p);
#else
        return interned_string_t(p);
#endif
    }
};

namespace std
{
    template<>
    struct hash<interned_string_t>
    {
        size_t operator()(const interned_string_t &x) const
        {
            auto hash=std::hash<const std::string*>();
            return hash(&(const std::string&)x);}
    };
};


/*  The simulation engine is responsible for executing the
    handlers, logging events, and so on. It doesn't choose
    which event should happen next, and doesn't track the
    currently pending messages.
*/
class SimulationEngine
        : public GraphLoadEvents
{
private:
    std::shared_ptr<StringInterner> m_interner = std::make_shared<StringInterner>();
protected:

public:
    /* Device addresses are guaranteed to be contiguous and start at zero. */
    typedef uint32_t device_address_t;

    typedef uint32_t pin_index_t;

    typedef uint32_t edge_index_t;

    const edge_index_t invalid_edge_index = (edge_index_t)-1;

    device_address_t max_device_address = (1<<27)-1;
    pin_index_t max_pin_index = 31;

    struct edge_index_range_t{
        edge_index_t begin;
        edge_index_t end;
    };

    struct routing_tuple_t
    {
        // There is one of these for every edge, which is the largest overall data-structure
        // in the simulation.

        uint32_t destDeviceAddress : 27;
        uint32_t sourceDeviceAddress : 5;
        uint32_t destDevicePin : 27;
        uint32_t sourceDevicePin : 5;

        bool operator==(const routing_tuple_t &o) const
        {
            return destDeviceAddress==o.destDeviceAddress
                && sourceDeviceAddress==o.sourceDeviceAddress
                && destDevicePin==o.destDevicePin
                && sourceDevicePin==o.sourceDevicePin;
        }
    };

    struct hash_routing_tuple_t
    {
        size_t operator()(const routing_tuple_t &t) const
        {
            const unsigned P=1000000007;
            size_t acc=t.destDeviceAddress;
            acc=acc*P+t.sourceDeviceAddress;
            acc=acc*P+t.sourceDevicePin;
            acc=acc*P+t.destDevicePin;
            return acc;
        }
    };

    virtual ~SimulationEngine() override
    {}

    std::shared_ptr<StringInterner> getInterner()
    { return m_interner; }

    interned_string_t intern(const std::string &v)
    { return m_interner->intern(v); }

    virtual size_t getDeviceCount() const =0;

    virtual uint32_t getDeviceRTS(device_address_t address) const =0;

    virtual void onExternalReceive(
        device_address_t destDev,
        pin_index_t destPinIndex,
        const TypedDataPtr &payload
    ) =0;

    virtual void executeReceive(
        edge_index_t edgeIncdex,
        const TypedDataPtr &payload,
        uint32_t &readyToSend,
        routing_tuple_t *route
    ) =0;

    virtual void onExternalSend(
        device_address_t sourceDev,
        pin_index_t sourcePinIndex,
        const TypedDataPtr &payload,
        edge_index_range_t &destinations
    ) =0;

    virtual void executeSend(
        device_address_t sourceDev,
        pin_index_t sourcePortIndex,
        TypedDataPtr &payload,
        bool &doSend,
        edge_index_range_t &pEdges,
        uint32_t &readyToSend
    ) =0;

};


class SimulationEngineFast
    : public SimulationEngine
{
private:
    struct edge_t
    {
        routing_tuple_t route;
        InputPinPtr inputPin;
        TypedDataPtr properties;
        TypedDataPtr state;
    };

    struct output_pin_t
    {
        OutputPinPtr pin;
        TypedDataPtr defaultMsg;
        edge_index_t beginEdgeIndex;
        edge_index_t endEdgeIndex;
    };

    struct device_t
    {
        interned_string_t name;
        device_address_t address;
        
        std::vector<output_pin_t> outputPins;

        DeviceTypePtr type;
        TypedDataPtr properties;
        TypedDataPtr state;

        uint32_t RTS;
    };

    struct OrchestratorServicesBase
        : public OrchestratorServices
    {
        SimulationEngineFast *engine;

        OrchestratorServicesBase(SimulationEngineFast *_engine)
            : OrchestratorServices(_engine->m_logLevel)
            , engine(_engine)
        {}

        void export_key_value(uint32_t key, uint32_t value) override
        { throw std::runtime_error("Not supported."); }

        void vcheckpoint(bool preEvent, int level, const char *tagFmt, va_list tagArgs) override
        { throw std::runtime_error("Not supported."); }

        void application_exit(int code) override
        { throw std::runtime_error("Not supported."); }
    };

    struct SendServicesHandler
        : public OrchestratorServicesBase
    {
        device_t *device;
        unsigned outputPinIndex;

        SendServicesHandler(SimulationEngineFast *_engine, device_t *_device, unsigned _outputPinIndex)
            : OrchestratorServicesBase(_engine)
            , device(_device)
            , outputPinIndex(_outputPinIndex)
        {}

        void vlog(unsigned level, const char *msg, va_list args) override
        {
            if(level<engine->m_logLevel){
                fprintf(stderr, "%s : ", ((const std::string &)device->name).c_str());
                vfprintf(stderr, msg, args);
                fprintf(stderr, "\n");
            }
        }
    };

    struct InitServicesHandler
        : public OrchestratorServicesBase
    {
        device_t *device;

        InitServicesHandler( SimulationEngineFast *_engine, device_t *_device)
            : OrchestratorServicesBase(_engine)
            , device(_device)
        {}

        virtual void vlog(unsigned level, const char *msg, va_list args)
        {
            if(level<engine->m_logLevel){
                fprintf(stderr, "%s : ", ((const std::string &)device->name).c_str());
                vfprintf(stderr, msg, args);
                fprintf(stderr, "\n");
            }
        }
    };

    struct ReceiveServicesHandler
        : public OrchestratorServicesBase
    {
        const edge_t *edge;

        ReceiveServicesHandler(SimulationEngineFast *_engine, const edge_t *_edge)
            : OrchestratorServicesBase(_engine)
            , edge(_edge)
        {}

        void vlog(unsigned level, const char *msg, va_list args) override
        {
            if(level < engine->m_logLevel){
                device_t *device=&engine->m_devices.at(edge->route.destDeviceAddress);
                fprintf(stderr, "%s : ", ((const std::string &)device->name).c_str());
                vfprintf(stderr, msg, args);
                fprintf(stderr, "\n");
            }
        }
    };

    int m_logLevel=2;

    TypedDataPtr m_graphProperties;
    std::vector<device_t> m_devices;
    std::vector<edge_t> m_edges;

    std::unordered_map<routing_tuple_t,edge_index_t,hash_routing_tuple_t> m_routeToEdge;

    std::unordered_map<MessageTypePtr,TypedDataPtr> m_messageTypeToDefaultMessage;

    // We want a default message to clone for output messages, but this
    // should be shared by everyone (as there could be thousands of them)
    TypedDataPtr getDefaultMessageForOutputPin(const MessageTypePtr &messageType)
    {
        auto it=m_messageTypeToDefaultMessage.find(messageType);
        if(it!=m_messageTypeToDefaultMessage.end()){
            return it->second;
        }
        
        auto d=messageType->getMessageSpec()->create();
        return m_messageTypeToDefaultMessage.emplace_hint(it, messageType, d)->second;
    }
    
public:

    /////////////////////////////////////////////////////////////////////////////////////////////
    // GraphLoadEvents

    uint64_t onBeginGraphInstance(
            const GraphTypePtr &graph,
            const std::string &id,
            const TypedDataPtr &properties,
            rapidjson::Document &&metadata
    ) override
    {
        return 1;
    }

    uint64_t onDeviceInstance
    (
        uint64_t graphInst,
        const DeviceTypePtr &dt,
        const std::string &id,
        const TypedDataPtr &properties,
        rapidjson::Document &&metadata
    ) override
    {
        if(m_devices.size() >= max_device_address) {
            // If this occurs, look at the bit-field restrictions on routing
            throw std::runtime_error("This graph contains more instances than the simulator can currently support.");
        }

        auto address=device_address_t (m_devices.size());

        device_t dev;

        dev.name=intern(id);
        dev.address=address;
        dev.type=dt;
        dev.properties=properties;
        dev.state=dt->getStateSpec()->create();
        
        for(auto & pin : dt->getOutputs()){
            dev.outputPins.emplace_back(output_pin_t{
                    pin,
                    getDefaultMessageForOutputPin(pin->getMessageType()),
                    invalid_edge_index,
                    invalid_edge_index
            });
        }

        InputPinPtr init=dt->getInput("__init__");
        if(init){
            InitServicesHandler services(this,&dev);

            init->onReceive(
                &services,
                m_graphProperties.get(),
                dev.properties.get(),
                dev.state.get(),
                nullptr,
                nullptr,
                nullptr
            );

            dev.RTS = dt->calcReadyToSend(
                &services,
                m_graphProperties.get(),
                dev.properties.get(),
                dev.state.get()
            );
        }

        m_devices.push_back(std::move(dev));

        fprintf(stderr, "Loaded device : %s\n", id.c_str());

        return address;
    }

    virtual void onEdgeInstance
    (
        uint64_t graphInst,
        uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
        uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
        const TypedDataPtr &properties,
        rapidjson::Document &&metadata
    ) override
    {
        // We improve cache locality and reduce memory a bit by:
        // - Collecting all edges up-front
        // - Sorting all edges by (srcDevInst,srcPortIndex)
        // - Capturing the output list for a pin as [beginOutputIndex,endOutputIndex)

        edge_index_t index=m_edges.size();

        edge_t edge;
        edge.route.destDeviceAddress=dstDevInst;
        edge.route.sourceDeviceAddress=srcDevInst;
        edge.route.destDevicePin=dstPin->getIndex();
        edge.route.sourceDevicePin=srcPin->getIndex();
        edge.inputPin=dstPin;
        edge.properties=properties;
        edge.state=dstPin->getStateSpec()->create();

        // We do not hook up edges at this point, as we'll want to sort
        // them first to make sure (srcDevInst,srcPortIndex) runs are contiguous

        m_edges.push_back(std::move(edge));

        fprintf(stderr, "Edge : (%s,%s)<-(%s,%s)\n",
                m_devices[dstDevInst].name.c_str(), dstPin->getName().c_str(),
                m_devices[srcDevInst].name.c_str(), srcPin->getName().c_str());
    }

    //! There will be no more edge instances in the graph.
    void onEndEdgeInstances(uint64_t /*graphToken*/) override
    {
        std::sort(m_edges.begin(), m_edges.end(),
            [](const edge_t &a, const edge_t &b){
                if(a.route.sourceDeviceAddress<b.route.sourceDeviceAddress) return true;
                if(a.route.sourceDevicePin<b.route.destDevicePin) return true;
                // We don't care about the rest, because we only need to sort into
                // equivalence classes
                return false;
            }
        );

        for(unsigned index=0; index<m_edges.size(); index++){
            const edge_t &edge=m_edges[index];
            device_t &srcDev=m_devices.at(edge.route.sourceDeviceAddress);

            output_pin_t &srcPin=srcDev.outputPins.at(edge.route.sourceDevicePin);
            if(srcPin.beginEdgeIndex==invalid_edge_index){
                srcPin.beginEdgeIndex=index;
                srcPin.endEdgeIndex=index+1;
            }else{
                assert(srcPin.endEdgeIndex==index);
                srcPin.endEdgeIndex++;
            }
        }

        for(const auto & dev : m_devices) {
            for (const auto &pin : dev.outputPins) {
                fprintf(stderr, "%s/%s : [%u,%u)\n", dev.name.c_str(), pin.pin->getName().c_str(), pin.beginEdgeIndex, pin.endEdgeIndex);
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////
    // Interaction methods

    virtual size_t getDeviceCount() const
    { return m_devices.size(); }

    virtual uint32_t getDeviceRTS(device_address_t address) const
    { return m_devices.at(address).RTS; }

    virtual void map_routing_tuples_to_edge_indices(
        unsigned n,
        const routing_tuple_t *routes,
        edge_index_t *edges
    ){
        for(unsigned i=0; i<n; i++){
            auto it=m_routeToEdge.find(routes[i]);
            if(it==m_routeToEdge.end()){
                edges[i]=invalid_edge_index;
            }else{
                edges[i]=it->second;
            }
        }
    }


    void onExternalReceive(
        device_address_t destDev,
        pin_index_t destPinIndex,
        const TypedDataPtr &payload
    ) override
    {
        // Do nothing. We don't care
    }

    void executeReceive(
        edge_index_t edgeIndex,
        const TypedDataPtr &payload,
        uint32_t &readyToSend,
        routing_tuple_t *route = nullptr // optional : information about the route
    ) override
    {
        assert(readyToSend);

        auto &edge=m_edges.at(edgeIndex);
        auto &device=m_devices.at(edge.route.destDeviceAddress);

        ReceiveServicesHandler services(this, &edge);
        
        edge.inputPin->onReceive(
            &services,
			m_graphProperties.get(),
			device.properties.get(),
			device.state.get(),
            edge.properties.get(),
			edge.state.get(),
			payload.get()
        );

        readyToSend=device.type->calcReadyToSend(
            &services,
            m_graphProperties.get(),
            device.properties.get(),
            device.state.get()
        );

        if(route){
            *route=edge.route;
        }
    }

    virtual void onExternalSend(
        device_address_t sourceDev,
        pin_index_t sourcePinIndex,
        const TypedDataPtr &payload,
        edge_index_range_t &destinations
    )
    {
        auto &device=m_devices.at(sourceDev);
        auto &pin=device.outputPins.at(sourcePinIndex);
        destinations.begin=pin.beginEdgeIndex;
        destinations.end=pin.endEdgeIndex;
    }

    virtual void executeSend(
        device_address_t sourceDev,
        pin_index_t sourcePortIndex,
        TypedDataPtr &payload,
        bool &doSend,
        edge_index_range_t &destinations,
        uint32_t &readyToSend
    ) override final
    {
        auto &device=m_devices.at(sourceDev);

        assert( device.RTS & (1<<sourcePortIndex) );

        auto &pin=device.outputPins.at(sourcePortIndex);

        SendServicesHandler services(this, &device, sourcePortIndex);

        payload=pin.defaultMsg.clone();

        pin.pin->onSend(
            &services,
			m_graphProperties.get(),
			device.properties.get(),
			device.state.get(),
            payload.get(),
            &doSend
        );

        readyToSend=device.type->calcReadyToSend(
            &services,
            m_graphProperties.get(),
            device.properties.get(),
            device.state.get()
        );

        destinations.begin=pin.beginEdgeIndex;
        destinations.end=pin.endEdgeIndex;
    }  
};


class InOrderQueueStrategy
{
private:
    using device_address_t = SimulationEngine::device_address_t;
    using edge_index_t = SimulationEngine::edge_index_t;
    using edge_index_range_t = SimulationEngine::edge_index_range_t;
    using routing_tuple_t = SimulationEngine::routing_tuple_t;

    struct message_t
    {
        edge_index_t edgeIndex;
        TypedDataPtr payload;
    };

    std::shared_ptr<SimulationEngine> m_engine;
    
    std::queue<message_t> m_messageQueue;
    FIFOSet<device_address_t> m_readyQueue;

    void init()
    {
        for(device_address_t address=0; address<m_engine->getDeviceCount(); address++){
            if( m_engine->getDeviceRTS(address) ){
                m_readyQueue.push_back( address );
            }
        }
    }

    bool step()
    {
        if(!m_readyQueue.empty()){
            device_address_t address=m_readyQueue.front();
            // Don't remove just yet

            uint32_t readyToSend=m_engine->getDeviceRTS(address);
            unsigned outputPin=__builtin_ctz(readyToSend);
            
            TypedDataPtr payload;
            bool doSend=true;
            edge_index_range_t range;
            m_engine->executeSend(
                address,
                outputPin,
                payload,
                doSend,
                range,
                readyToSend
            );

            if(readyToSend){
                m_readyQueue.pop_front();
            }else{
                m_readyQueue.move_front_to_back();
            }

            return true;
        }else if(!m_messageQueue.empty()){
            auto msg=m_messageQueue.front();
            m_messageQueue.pop();

            uint32_t readyToSend;
            routing_tuple_t route;
            m_engine->executeReceive(
                msg.edgeIndex,
                msg.payload,
                readyToSend,
                &route
            );

            if(readyToSend){
                m_readyQueue.push_back(route.destDeviceAddress);
            }

            return true;
        }else{

            return false;
        }
    }

};

#endif
