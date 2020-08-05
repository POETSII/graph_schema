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
#include <stack>
#include <random>

template<class T>
class FIFOSet
{
private:
    std::list<const T*> m_order;
    std::unordered_map<T,typename std::list<const T*>::const_iterator> m_index;
public:
    struct const_iterator {
        typename std::list<const T *>::const_iterator it;

        bool operator==(const const_iterator &o) const
        { return it==o.it; }

        bool operator!=(const const_iterator &o) const
        { return it!=o.it; }

        const_iterator &operator++()
        { ++it; return *this; }

        const T &operator*() const
        { return **it; }
    };

    const_iterator begin() const
    { return const_iterator{m_order.begin()}; }

    const_iterator end() const
    { return const_iterator{m_order.end()}; }

    bool empty() const
    { return m_order.empty(); }

    size_t size() const
    { return m_order.size(); }

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
        if(it==m_index.end()){
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

template<class T>
class LIFOSet
{
private:
    std::list<const T*> m_order;
    std::unordered_map<T,typename std::list<const T*>::const_iterator> m_index;
public:
    struct const_iterator {
        typename std::list<const T *>::const_iterator it;

        bool operator==(const const_iterator &o) const
        { return it==o.it; }

        bool operator!=(const const_iterator &o) const
        { return it!=o.it; }

        const_iterator &operator++()
        { ++it; return *this; }

        const T &operator*() const
        { return **it; }
    };

    const_iterator begin() const
    { return const_iterator{m_order.begin()}; }

    const_iterator end() const
    { return const_iterator{m_order.end()}; }

    bool empty() const
    { return m_order.empty(); }

    size_t size() const
    { return m_order.size(); }

    const T &back()
    {
        assert(!empty());
        return *m_order.back();
    }

    void pop_back()
    {
        assert(!empty());
        const T *pVal=m_order.back();
        m_order.pop_back();
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
        if(it==m_index.end()){
            return;
        }

        m_order.erase(it->second);
        m_index.erase(it);
    }

};

template<class T>
class RandomSelectionSet
{
private:
    std::vector<const T*> m_elements;
    std::unordered_map<T,unsigned> m_index;
public:
    struct const_iterator {
        typename std::vector<const T *>::const_iterator it;

        bool operator==(const const_iterator &o) const
        { return it==o.it; }

        bool operator!=(const const_iterator &o) const
        { return it!=o.it; }

        const_iterator &operator++()
        { ++it; return *this; }

        const T &operator*() const
        { return **it; }
    };

    const_iterator begin() const
    { return const_iterator{m_elements.begin()}; }

    const_iterator end() const
    { return const_iterator{m_elements.end()}; }

    const_iterator find(const T &value) const
    {
        auto itPlace=m_index.find(value);
        if(itPlace==m_index.end()){
            return end();
        }
        return const_iterator{ m_elements.begin()+itPlace->second };
    }

    void insert(const T &x)
    {
        auto it=m_index.find(x);
        if(it!=m_index.end()) {
            return;
        }

        it=m_index.insert(std::make_pair(x,m_elements.size())).first;
        m_elements.push_back(&it->first);
    }

    bool empty() const
    { return m_elements.empty(); }

    bool size() const
    { return m_elements.size(); }

    template<class TUrng>
    const T &get_random(TUrng &urng) const {
        std::uniform_real_distribution<> udist;
        size_t sel = size_t(floor(udist(urng) * m_elements.size()));
        assert(sel < m_elements.size());
        return *m_elements[sel];
    }

    template<class TUrng>
    T pop_random(TUrng &urng)
    {
        T res=get_random(urng);
        erase(res);
        return res;
    }

    void erase(const T &x)
    {
        auto it=m_index.find(x);
        if(it==m_index.end()){
            return;
        }

        // Note that it may be the case that it==itBack
        auto itBack=m_index.find(*m_elements.back());
        itBack->second=it->second;
        m_elements[itBack->second]=&itBack->first;
        m_elements.resize(m_elements.size()-1);
        m_index.erase(it);

        /*
        for(const auto &kv : m_index){
            assert(kv.first!=x);
            assert(kv.second<m_elements.size());
            assert(&kv.first==m_elements[kv.second]);
        }
        */
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

#ifndef NDEBUG
    interned_string_t(const std::shared_ptr<StringInterner> &_owner, const std::string *_value)
        : value(_value)
        , owner(_owner)
    {}
#else
    interned_string_t(const std::string *_value)
        : value(_value)
    {}
#endif

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
        edge_index_t begin;             // [begin,beginExternals) is all internal devices
        edge_index_t beginExternals;    // [beginExternals,end) is all external devices
        edge_index_t end;
    };

    struct routing_tuple_t
    {
        // There is one of these for every edge, which is the largest overall data-structure
        // in the simulation.

        uint32_t destDeviceAddress;
        uint32_t sourceDeviceAddress;
        uint16_t destDevicePin;
        uint16_t sourceDevicePin;

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

    virtual void setLogLevel(unsigned level) =0;

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
        uint64_t sendEventId,
        uint32_t &readyToSend,
        routing_tuple_t *route
    ) =0;

    virtual void onExternalSend(
        device_address_t sourceDev,
        pin_index_t sourcePinIndex,
        const TypedDataPtr &payload,
        int sendIndex,
        edge_index_range_t &destinations
    ) =0;

    virtual void executeSend(
        device_address_t sourceDev,
        pin_index_t sourcePortIndex,
        TypedDataPtr &payload,
        uint64_t &sendEventId,
        bool &doSend,
        edge_index_range_t &pEdges,
        uint32_t &readyToSend
    ) =0;

    virtual void executeHardwareIdle() =0;

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
        int sendIndex;
    };

    struct output_pin_t
    {
        OutputPinPtr pin;
        TypedDataPtr defaultMsg;
        edge_index_t beginEdgeIndex;
        edge_index_t beginExternalIndex; // May be invalid for interleaved indexed sends
        edge_index_t endEdgeIndex;
        // Indexed sends are special, as we may have interleaved externals and internals.
        bool isIndexedSend;
    };

    struct device_t
    {
        interned_string_t name;
        device_address_t address;
        
        std::vector<output_pin_t> outputPins;

        DeviceTypePtr type;
        TypedDataPtr properties;
        TypedDataPtr state;
        bool isExternal; // == type->isExternal(); Cache here for performance

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

        void vcheckpoint(bool preEvent, int level, const char *tagFmt, va_list tagArgs) override
        {
            static bool warned=false;
            if(!warned) {
                // no-op
                fprintf(stderr, "handler_checkpoint is not supported by this engine\n");
                warned=true;
            }
        }

        void check_for_exit(const char *msg)
        {
            if(!strcmp(msg, "_HANDLER_EXIT_SUCCESS_9be65737_")){
                fprintf(stderr, "Pseudo application exit, success.\n");
                exit(0);
            }
            if(!strcmp(msg, "_HANDLER_EXIT_FAIL_9be65737_")){
                fprintf(stderr, "Pseudo application exit, fail.\n");
                exit(1);
            }
        }

        void application_exit(int code) override
        {
            fprintf(stderr, "Application exit %d\n", code);
            exit(code);
        }
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
            check_for_exit(msg);
        }
    };

    struct HardwareIdleServicesHandler
        : public OrchestratorServicesBase
    {
        device_t *device;
        unsigned outputPinIndex;

        HardwareIdleServicesHandler(SimulationEngineFast *_engine, device_t *_device)
            : OrchestratorServicesBase(_engine)
            , device(_device)
        {}

        void vlog(unsigned level, const char *msg, va_list args) override
        {
            if(level<engine->m_logLevel){
                fprintf(stderr, "%s : ", ((const std::string &)device->name).c_str());
                vfprintf(stderr, msg, args);
                fprintf(stderr, "\n");
            }
            check_for_exit(msg);
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
            check_for_exit(msg);
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
            check_for_exit(msg);
        }
    };

    unsigned m_logLevel=6;

    TypedDataPtr m_graphProperties;
    std::vector<device_t> m_devices;
    std::vector<edge_t> m_edges;

    std::unordered_map<routing_tuple_t,edge_index_t,hash_routing_tuple_t> m_routeToEdge;

    std::unordered_map<MessageTypePtr,TypedDataPtr> m_messageTypeToDefaultMessage;

    std::shared_ptr<LogWriter> m_logWriter;
    uint64_t m_logIdUnq=0;
    uint64_t m_barrierIdUnq=0;

    uint64_t make_log_id()
    {
        return m_logIdUnq++;
    }

    std::string make_barrier_id()
    {
        return "barrier_"+std::to_string(m_barrierIdUnq++);
    }

    // We want a default message to clone for output messages, but this
    // should be shared by everyone (as there could be thousands of them)
    TypedDataPtr getDefaultMessageForOutputPin(const MessageTypePtr &messageType)
    {
        auto it=m_messageTypeToDefaultMessage.find(messageType);
        if(it!=m_messageTypeToDefaultMessage.end()){
            return it->second;
        }

        TypedDataPtr d;
        if(messageType && messageType->getMessageSpec()){
            d=messageType->getMessageSpec()->create();
        }
        
        return m_messageTypeToDefaultMessage.emplace_hint(it, messageType, d)->second;
    }
    
public:
    SimulationEngineFast(std::shared_ptr<LogWriter> pLogWriter)
        : m_logWriter(pLogWriter)
    {}

    void setLogLevel(unsigned level) override
    {
        m_logLevel=level;
    }


    /////////////////////////////////////////////////////////////////////////////////////////////
    // GraphLoadEvents

    uint64_t onBeginGraphInstance(
            const GraphTypePtr &graph,
            const std::string &id,
            const TypedDataPtr &properties,
            rapidjson::Document &&metadata
    ) override
    {
        m_graphProperties=properties;
        return 1;
    }

    uint64_t onDeviceInstance
    (
        uint64_t graphInst,
        const DeviceTypePtr &dt,
        const std::string &id,
        const TypedDataPtr &properties,
        const TypedDataPtr &state,
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
        dev.state=state.clone(); // Bit of a waste of time, but loading only
        dev.isExternal=dt->isExternal();
        
        for(auto & pin : dt->getOutputs()){
            dev.outputPins.emplace_back(output_pin_t{
                    pin,
                    getDefaultMessageForOutputPin(pin->getMessageType()),
                    invalid_edge_index,
                    invalid_edge_index,
                    invalid_edge_index,
                    pin->isIndexedSend()
            });
        }

        if(!dt->isExternal()) {
            InitServicesHandler services(this, &dev);

            dt->init(
                &services,
                m_graphProperties.get(),
                dev.properties.get(),
                dev.state.get()
            );

            if(m_logWriter){
                auto id=make_log_id();
                m_logWriter->onInitEvent(
                    std::to_string(id).c_str(),
                    (double)id,
                    0.0,
                    {},
                    dt,
                    dev.name.c_str(),
                    dev.RTS,
                    id,
                    {},
                    dev.state
                );
            }
            
            dev.RTS = dt->calcReadyToSend(
                    &services,
                    m_graphProperties.get(),
                    dev.properties.get(),
                    dev.state.get()
            );
        }else{
            dev.RTS=0;
        }


        m_devices.push_back(std::move(dev));

        //fprintf(stderr, "Loaded device : %s=%u, numOutputs=%lu, type=%s\n", id.c_str(), address, m_devices.back().outputPins.size(), dt->getId().c_str());

        return address;
    }

    virtual void onEdgeInstance
    (
        uint64_t graphInst,
        uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
        uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
        int sendIndex,
        const TypedDataPtr &properties,
        const TypedDataPtr &state,
        rapidjson::Document &&metadata
    ) override
    {
        // We improve cache locality and reduce memory a bit by:
        // - Collecting all edges up-front
        // - Sorting all edges by (srcDevInst,srcPortIndex), and internals before externals
        // - Capturing the output list for a pin as [beginOutputIndex,endOutputIndex)
        // - The point beginExternalIndex marks the break where externals start
        //
        // In the special case of indexed sends the internals and externals may be
        // interleaved, and beginExternalIndex will be invalid.


        edge_index_t index=m_edges.size();

        edge_t edge;
        edge.route.destDeviceAddress=dstDevInst;
        edge.route.sourceDeviceAddress=srcDevInst;
        edge.route.destDevicePin=dstPin->getIndex();
        edge.route.sourceDevicePin=srcPin->getIndex();
        edge.inputPin=dstPin;
        edge.properties=properties;
        edge.state=state;
        edge.sendIndex=sendIndex;

        auto &srcDev = m_devices.at(edge.route.sourceDeviceAddress);
        auto &dstDev = m_devices.at(edge.route.destDeviceAddress);
        auto dstPinType=dstDev.type->getInput(edge.route.destDevicePin);
        auto srcPinType=srcDev.type->getOutput(edge.route.sourceDevicePin);
        /*fprintf(stderr, "  Route: %s:%s-%s:%s\n",
                dstDev.name.c_str(), dstPinType->getName().c_str(),
                srcDev.name.c_str(), srcPinType->getName().c_str()
        );*/

        // We do not hook up edges at this point, as we'll want to sort
        // them first to make sure (srcDevInst,srcPortIndex) runs are contiguous

        m_edges.push_back(edge);

    }

    //! There will be no more edge instances in the graph.
    void onEndEdgeInstances(uint64_t /*graphToken*/) override
    {
        /*
        for(auto & edge : m_edges){
            auto &srcDev = m_devices.at(edge.route.sourceDeviceAddress);
            auto &dstDev = m_devices.at(edge.route.destDeviceAddress);
            fprintf(stderr, "  Route: %s:%s-%s:%s\n",
                    dstDev.name.c_str(), dstDev.type->getInput(edge.route.destDevicePin)->getName().c_str(),
                    srcDev.name.c_str(), srcDev.type->getOutput(edge.route.sourceDevicePin)->getName().c_str()
            );
        }
         */

        // We also sort internals before externals, so make a temporary bit-mask
        // as we could hit it quite a bit during sorting
        std::vector<char> isExternal(m_devices.size());
        for(unsigned i=0; i<m_devices.size(); i++){
            isExternal[i]=m_devices[i].isExternal;
        }

        auto cmp=[&](const edge_t &a, const edge_t &b){
            if(a.route.sourceDeviceAddress<b.route.sourceDeviceAddress) return true;
            if(a.route.sourceDeviceAddress>b.route.sourceDeviceAddress) return false;
            if(a.route.sourceDevicePin<b.route.sourceDevicePin) return true;
            if(a.route.sourceDevicePin>b.route.sourceDevicePin) return false;

            // Explicit send indices have priority over external/internal ordering
            if( (a.sendIndex!=-1) != (b.sendIndex!=-1) ){
                throw std::runtime_error("Graph contains mix of implicit and explicit send indexes for an output.");
            }
            if(a.sendIndex<b.sendIndex) return true;
            if(a.sendIndex>b.sendIndex) return false;

            // Internals destinations come before externals
            if(!isExternal[a.route.destDeviceAddress] && isExternal[b.route.destDeviceAddress]) return true;
            if(isExternal[a.route.destDeviceAddress] && !isExternal[b.route.destDeviceAddress]) return false;

            // We don't care about the rest, because we only need to sort into
            // equivalence classes
            return false;
        };

        std::stable_sort(m_edges.begin(), m_edges.end(), cmp);

        for(unsigned index=0; index<m_edges.size(); index++){
            const edge_t &edge=m_edges[index];
            device_t &srcDev=m_devices.at(edge.route.sourceDeviceAddress);

            /*
            fprintf(stderr, "  Route: %s:%s-%s:%s\n",
                    dstDev.name.c_str(), dstDev.type->getInput(edge.route.destDevicePin)->getName().c_str(),
                    srcDev.name.c_str(), srcDev.type->getOutput(edge.route.sourceDevicePin)->getName().c_str()
                    );
            */

            output_pin_t &srcPin=srcDev.outputPins.at(edge.route.sourceDevicePin);
            if(srcPin.beginEdgeIndex==invalid_edge_index){
                srcPin.beginEdgeIndex=index;
                srcPin.endEdgeIndex=index;
            }
            if(!isExternal[edge.route.destDeviceAddress]){
                srcPin.beginExternalIndex=index+1;
            }
            
            if(!srcPin.isIndexedSend){
                if(srcPin.beginExternalIndex==srcPin.endEdgeIndex && isExternal[edge.route.destDeviceAddress]){
                    srcPin.beginExternalIndex=index;
                }
            }else{
                if(edge.sendIndex!=-1 && edge.sendIndex!=(int)(index-srcPin.beginEdgeIndex)){
                    throw std::runtime_error("Graph contains non-contiguous or non-zero-base explicit send indices.");
                }
            }

            srcPin.endEdgeIndex=index+1;
        }


        /*for(const auto & dev : m_devices) {
            for (const auto &pin : dev.outputPins) {
                fprintf(stderr, "%s/%s : [%u,%u,%u)\n", dev.name.c_str(), pin.pin->getName().c_str(), pin.beginEdgeIndex, pin.beginExternalIndex, pin.endEdgeIndex);
            }
        }*/
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
        uint64_t sendEventId,
        uint32_t &readyToSend,
        routing_tuple_t *route = nullptr // optional : information about the route
    ) override
    {
        auto &edge=m_edges.at(edgeIndex);
        auto &device=m_devices.at(edge.route.destDeviceAddress);

        assert(!device.type->isExternal()); // External deliveries need to be managed... externally

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
        device.RTS=readyToSend;

        if(m_logWriter){
            auto id=make_log_id();
            m_logWriter->onRecvEvent(
                std::to_string(id).c_str(),
                (double)id,
                0.0,
                {},
                device.type,
                device.name.c_str(),
                device.RTS,
                id,
                {},
                device.state,
                edge.inputPin,
                std::to_string(sendEventId).c_str()
            );
        }

        if(route){
            *route=edge.route;
        }
    }

private:
    void extractDestinations(
        const output_pin_t &pin,
        int sendIndex,
        edge_index_range_t &destinations
    ){
        if(!pin.isIndexedSend){
            assert(sendIndex==-1); // Simulator should enforce this, not application
            destinations.begin=pin.beginEdgeIndex;
            destinations.beginExternals=pin.beginExternalIndex;
            destinations.end=pin.endEdgeIndex;
        }else{
            if( (sendIndex<0) || (int(pin.endEdgeIndex-pin.beginEdgeIndex)<sendIndex) ){
                throw std::runtime_error("Attempt to send on out of range sendIndex.");
            }
            destinations.begin=pin.beginEdgeIndex+sendIndex;
            destinations.end=pin.beginEdgeIndex+sendIndex+1;
            
            const auto &edge=m_edges[pin.beginEdgeIndex+sendIndex];
            const auto &dstDev=m_devices[edge.route.destDeviceAddress];
            if(dstDev.isExternal){
                assert(0);
                destinations.beginExternals=destinations.begin;
            }else{
                destinations.beginExternals=destinations.end;
            }
        }
        assert(destinations.begin <= destinations.beginExternals);
        assert(destinations.beginExternals <= destinations.end);
    }
public:

    virtual void onExternalSend(
        device_address_t sourceDev,
        pin_index_t sourcePinIndex,
        const TypedDataPtr &payload,
        int sendIndex, // -1 for non indexed
        edge_index_range_t &destinations
    )
    {
        const auto &device=m_devices.at(sourceDev);
        const auto &pin=device.outputPins.at(sourcePinIndex);
        extractDestinations(pin, sendIndex, destinations);
    }

    virtual void executeSend(
        device_address_t sourceDev,
        pin_index_t sourcePortIndex,
        TypedDataPtr &payload,
        uint64_t &sendEventId,
        bool &doSend,
        edge_index_range_t &destinations,
        uint32_t &readyToSend
    ) override final
    {
        auto &device=m_devices.at(sourceDev);

        assert(!device.type->isExternal()); // External deliveries need to be managed... externally

        assert( device.RTS & (1<<sourcePortIndex) );

        auto &pin=device.outputPins.at(sourcePortIndex);

        SendServicesHandler services(this, &device, sourcePortIndex);

        payload=pin.defaultMsg.clone();

        unsigned sendIndex=-1;
        unsigned *sendIndexPtr=pin.isIndexedSend ? &sendIndex : nullptr;

        pin.pin->onSend(
            &services,
			m_graphProperties.get(),
			device.properties.get(),
			device.state.get(),
            payload.get(),
            &doSend,
            sendIndexPtr
        );

        readyToSend=device.type->calcReadyToSend(
            &services,
            m_graphProperties.get(),
            device.properties.get(),
            device.state.get()
        );
        device.RTS=readyToSend;

        extractDestinations(pin, sendIndex, destinations);
        

        if(!m_logWriter){
            sendEventId=-1;
        }else{
            auto id=make_log_id();
            m_logWriter->onSendEvent(
                std::to_string(id).c_str(),
                (double)id,
                0.0,
                {},
                device.type,
                device.name.c_str(),
                device.RTS,
                id,
                {},
                device.state,
                pin.pin,
                !doSend,
                destinations.end-destinations.begin,
                payload
            );
            sendEventId=id;
        }
    }  

    virtual void executeHardwareIdle(    ) override final
    {
        std::string hardwareIdleEventId=make_barrier_id();

        for(auto &device : m_devices){
            if(device.type->isExternal()){
                continue; // TODO: Is this correct for externals...?
            }

            assert( !device.RTS );

            HardwareIdleServicesHandler services(this, &device);
            
            device.type->onHardwareIdle(
                &services,
                m_graphProperties.get(),
                device.properties.get(),
                device.state.get()
            );

            device.RTS=device.type->calcReadyToSend(
                &services,
                m_graphProperties.get(),
                device.properties.get(),
                device.state.get()
            );

            if(m_logWriter){
                auto id=make_log_id();
                m_logWriter->onHardwareIdleEvent(
                    std::to_string(id).c_str(),
                    (double)id,
                    0.0,
                    {},
                    device.type,
                    device.name.c_str(),
                    device.RTS,
                    id,
                    {},
                    device.state,
                    hardwareIdleEventId.c_str()
                );
            }
        }
    }
};


class BasicStrategy
{
public:
    virtual ~BasicStrategy()
    {}

protected:
    using device_address_t = SimulationEngine::device_address_t;
    using edge_index_t = SimulationEngine::edge_index_t;
    using edge_index_range_t = SimulationEngine::edge_index_range_t;
    using routing_tuple_t = SimulationEngine::routing_tuple_t;

    struct message_t
    {
        edge_index_t edgeIndex;
        TypedDataPtr payload;
        uint64_t sendEventId;
    };

    std::shared_ptr<SimulationEngine> m_engine;

    double m_probSend=0.5; // The closer this is to 1.0, the more likely to do a send
    bool m_weightedProbs=true; // If true the probability is scaled by number waiting
    std::mt19937 m_urng;
    std::uniform_real_distribution<> m_udist;

    virtual device_address_t pick_ready()=0;
    virtual void add_ready(device_address_t device)=0;
    virtual void remove_ready(device_address_t device)=0;
    virtual void keep_ready(device_address_t device)=0;
    virtual size_t count_ready() const =0;

    virtual void add_messages(const edge_index_range_t &range, const TypedDataPtr &payload, uint64_t sendEventId)=0;
    virtual message_t pop_message()=0;
    virtual size_t count_messages() const=0;

    virtual void check_invariants() const
    {};


    void step_send()
    {
        device_address_t address=pick_ready();

        uint32_t readyToSend = m_engine->getDeviceRTS(address);
        assert(readyToSend);
        unsigned outputPin = __builtin_ctz(readyToSend);

        TypedDataPtr payload;
        bool doSend = true;
        edge_index_range_t range;
        uint64_t sendEventId;
        m_engine->executeSend(
                address,
                outputPin,
                payload,
                sendEventId,
                doSend,
                range,
                readyToSend
        );

        if(doSend) {
            // TODO: Currently we just drop external destinations
            add_messages(range, payload, sendEventId);
            //fprintf(stderr, "  %u -> %u dests, numMessages=%u\n", address, range.end-range.begin, count_messages());
        }

        if(readyToSend){
            keep_ready(address);
        }else{
            remove_ready(address);
        }
    }

    void step_recv()
    {
        auto msg=pop_message();

        uint32_t readyToSend;
        routing_tuple_t route;
        uint64_t sendEventId;
        m_engine->executeReceive(
                msg.edgeIndex,
                msg.payload,
                msg.sendEventId,
                readyToSend,
                &route
        );

        if(readyToSend){
            add_ready(route.destDeviceAddress);
        }
    }
public:
    BasicStrategy(std::shared_ptr<SimulationEngine> engine)
            : m_engine(engine)
    {}

    template<class TUrng>
    void init(TUrng &seed)
    {
        uint32_t seeds[16];
        std::generate(seeds, seeds+16, std::ref(seed));
        std::seed_seq seeder(seeds, seeds+16);
        m_urng.seed(seeder);

        for(device_address_t address=0; address<m_engine->getDeviceCount(); address++){
            if( m_engine->getDeviceRTS(address) ){
                add_ready( address );
            }
        }

    }

    bool step()
    {

        auto numMessages=count_messages();
        auto numReady=count_ready();

        //fprintf(stderr, "numMessages=%u, numReady=%u\n", numMessages, numReady);

        if(numMessages==0 && numReady==0){
            m_engine->executeHardwareIdle();
            for(device_address_t address=0; address<m_engine->getDeviceCount(); address++){
                if( m_engine->getDeviceRTS(address) ){
                    add_ready( address );
                }
            }
            return true;
        }

        bool doSend;
        if(m_probSend>=1.0 || (numMessages==0)){
            doSend=true;
        }else if(m_probSend<=0.0 || (numReady==0)){
            doSend=false;
        }else{
            double nSend=numReady;
            double nRecv=numMessages;

            if(m_weightedProbs){
                nSend *= m_probSend;
                nRecv *= (1-m_probSend);
            }

            doSend = m_udist(m_urng) * (nSend+nRecv) >= nRecv;
        }

        if(doSend){
            step_send();
        }else{
            step_recv();
        }

        return true;

    }

};


class InOrderQueueStrategy
        : public BasicStrategy
{
private:
    std::queue<message_t> m_messageQueue;
    FIFOSet<device_address_t> m_readyQueue;
protected:
    device_address_t pick_ready() override
    { return m_readyQueue.front(); }

    void add_ready(device_address_t device) override
    { m_readyQueue.push_back(device); }

    void remove_ready(device_address_t device) override
    { m_readyQueue.erase(device); }

    void keep_ready(device_address_t device) override
    {
        assert(device==m_readyQueue.front());
        m_readyQueue.move_front_to_back();
    }

    size_t count_ready() const override
    { return m_readyQueue.size(); }

    void add_messages(const edge_index_range_t &range, const TypedDataPtr &payload, uint64_t sendEventId) override
    {
        //fprintf(stderr, "  numInt=%u, numExt=%u\n", range.beginExternals-range.begin, range.end-range.beginExternals);
        assert(range.begin<=range.beginExternals);
        for(auto ei=range.begin; ei<range.beginExternals; ei++){
            m_messageQueue.push(message_t{ei, payload,sendEventId});
        }
    }

    message_t pop_message() override
    {
        message_t res=m_messageQueue.front();
        m_messageQueue.pop();
        return res;
    }

    size_t count_messages() const override
    { return m_messageQueue.size(); }

public:
    InOrderQueueStrategy(std::shared_ptr<SimulationEngine> engine)
        : BasicStrategy(engine)
    {}
};


class OutOfOrderStrategy
        : public BasicStrategy
{
private:
    std::vector<message_t> m_messageSet;
    RandomSelectionSet<device_address_t> m_readySet;

    void check_invariants() const override
    {
        for(unsigned i=0; i<m_engine->getDeviceCount(); i++){
            assert( (m_engine->getDeviceRTS(i)==0)  == ( m_readySet.find(i) == m_readySet.end() ) );
        }
    }

    device_address_t pick_ready() override
    { return m_readySet.get_random(m_urng); }

    void add_ready(device_address_t device) override
    { m_readySet.insert(device); }

    void remove_ready(device_address_t device) override
    { m_readySet.erase(device); }

    void keep_ready(device_address_t device) override
    {
        // Leave in place
    }

    size_t count_ready() const override
    { return m_readySet.size(); }

    void add_messages(const edge_index_range_t &range, const TypedDataPtr &payload, uint64_t sendEventId) override
    {
        for(auto ei=range.begin; ei<range.beginExternals; ei++){
            m_messageSet.push_back(message_t{ei, payload, sendEventId});
        }
    }

    message_t pop_message() override
    {
        size_t sel=(size_t)(floor(m_udist(m_urng)*m_messageSet.size()));
        message_t res=m_messageSet[sel];
        std::swap(m_messageSet[sel], m_messageSet.back());
        m_messageSet.resize(m_messageSet.size()-1);
        return res;
    }

    size_t count_messages() const override
    { return m_messageSet.size(); }

public:
    OutOfOrderStrategy(std::shared_ptr<SimulationEngine> engine)
            : BasicStrategy(engine)
    {}

};

class ReverseOrderStrategy
        : public BasicStrategy
{
private:
    std::stack<message_t> m_messageQueue;
    LIFOSet<device_address_t> m_readyQueue;
protected:
    device_address_t pick_ready() override
    { return m_readyQueue.back(); }

    void add_ready(device_address_t device) override
    { m_readyQueue.push_back(device); }

    void remove_ready(device_address_t device) override
    { m_readyQueue.erase(device); }

    void keep_ready(device_address_t device) override
    {
        assert(device==m_readyQueue.back());
        // Do nothing
    }

    size_t count_ready() const override
    { return m_readyQueue.size(); }

    void add_messages(const edge_index_range_t &range, const TypedDataPtr &payload, uint64_t sendEventId) override
    {
        for(auto ei=range.begin; ei<range.beginExternals; ei++){
            m_messageQueue.push(message_t{ei, payload, sendEventId});
        }
    }

    message_t pop_message() override
    {
        message_t res=m_messageQueue.top();
        m_messageQueue.pop();
        return res;
    }

    size_t count_messages() const override
    { return m_messageQueue.size(); }

public:
    ReverseOrderStrategy(std::shared_ptr<SimulationEngine> engine)
            : BasicStrategy(engine)
    {}
};

#endif
