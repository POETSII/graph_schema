#ifndef model_checker_hpp
#define model_checker_hpp

#include <algorithm>
#include <vector>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <random>

#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdarg>

typedef uint8_t device_id_t;
typedef uint8_t port_index_t;

struct endpoint_id_t
{
    device_id_t device;
    port_index_t port;
};

struct hash_t
{
    uint64_t hash;

    static hash_t Random()
    {
        static std::mt19937 rng;
        hash_t res;
        for(unsigned i=0; i<8; i++){
            res & uint8_t(rng());
        }
        return res;
    }

    hash_t()
        : hash(14695981039346656037ull)
    {}

    hash_t(const hash_t &o)
        : hash(o.hash)
    {}

    bool operator==(const hash_t &o) const
    { return o.hash==hash; }

    bool operator!=(const hash_t &o) const
    { return o.hash!=hash; }

    bool operator<(const hash_t &o) const
    { return o.hash<hash; }

    void operator & (uint8_t x)
    {
        hash = (hash ^ x) * 1099511628211ull ; // fnv-1a
    }

    void operator & (uint16_t x)
    {
        *this & uint8_t(x);
        *this & uint8_t(x>>8);
    }

    void operator & (uint32_t x)
    {
        for(unsigned i=0; i<4; i++){
            *this & uint8_t(x);
            x=x>>8;
        }
    }

    void operator & (float x)
    {
        uint32_t r;
        memcpy(&r, &x, 4);
        *this & r;
    }

    void operator & (const hash_t &o)
    {
        // Assume the other hash is "good".
        // Try to make sure hash ordering is important.
        // Note that we will be adding hashes together at various points, so this should not be
        // a linear combination.
        //hash = ((hash<<11) ^ (hash>>7)) + ((o.hash<<7) ^ (o.hash>>11));
        for(unsigned i=0; i<8; i++){
            *this & uint8_t(o.hash>>(i*8));
        }
    }
};

namespace std
{
    template<>
    class hash<hash_t>
    {
        public:
        size_t operator()(const hash_t &h) const
        {
            return h.hash;
        }
    };
};

class MessageType
{
private:
    size_t m_payloadSize;
protected:
    MessageType( size_t payloadSize)
        : m_payloadSize(payloadSize)
    {}
public:

    size_t GetPayloadSize() const
    { return m_payloadSize; }

    virtual void HashPayload(hash_t &hash, const uint8_t *payload) const
    {
        for(unsigned i=0; i<m_payloadSize; i++){
            hash & payload[i];
        }
    }

    virtual bool Equal(const MessageType *o) const
    {
        return this==o; // default is very restrictive
    }
};

template<class TMsg>
class MessageTypeHelper
    : public MessageType
{
protected:
    virtual void HashPayload(hash_t &hash, const TMsg *payload) const
    {
        MessageType::HashPayload(hash, (const uint8_t*) payload); // Call on the base. Not infinite recursion...
    }
public:
    MessageTypeHelper()
        : MessageType(sizeof(TMsg))
    {}

    virtual void HashPayload(hash_t &hash, const uint8_t *payload) const final
    {
        return HashPayload(hash, (const TMsg *)payload);
    }
};

/*! This class wraps a data-type with standard hashing, and is equal
    to all other data-types wrapping the same type. */
template<class TMsg>
class MessageTypePlain final
    : public MessageTypeHelper<TMsg>
{
public:
    bool Equal(const MessageType *o) const override
    {
        // Any instances of this class is the same message type
        return dynamic_cast<const MessageTypePlain<TMsg> *>(o) !=0;
    }
};

class DeviceType
{
private:
    size_t m_propertiesSize;
    size_t m_stateSize;

    std::vector<std::shared_ptr<MessageType> > m_inputPorts;
    std::vector<std::shared_ptr<MessageType> > m_outputPorts;

    DeviceType() = delete;
protected:
    DeviceType(
        size_t propertiesSize, size_t stateSize,
        std::vector<std::shared_ptr<MessageType> > inputPorts,
        std::vector<std::shared_ptr<MessageType> > outputPorts
    )
        : m_propertiesSize(propertiesSize)
        , m_stateSize(stateSize)
        , m_inputPorts(inputPorts)
        , m_outputPorts(outputPorts)
    {}    
public:
    virtual ~DeviceType()
    {}

    size_t GetStateSize() const
    { return m_stateSize; }

    size_t GetPropertiesSize() const
    { return m_propertiesSize; }

    const std::vector<std::shared_ptr<MessageType> > GetInputPorts()
    { return m_inputPorts; }

    const std::vector<std::shared_ptr<MessageType> > GetOutputPorts()
    { return m_outputPorts; }

    virtual uint32_t GetRTS(
        const uint8_t *pProperties,
        const uint8_t *pState
    ) const=0;
    
    virtual void HashState(
        hash_t &base,
        const uint8_t *pState
    ) const
    {
        for(unsigned i=0; i<m_stateSize; i++){
            base & pState[i];
        }
    }

    virtual void onInit(
        const uint8_t *pProperties,
        uint8_t *pState
    ) const
    {
        memset(pState, 0, GetStateSize());
    }

    virtual bool onSend(
        const uint8_t *pProperties,
        uint8_t *pState,
        unsigned port,
        uint8_t *payload
    ) const =0;

    virtual void onRecv(
        const uint8_t *pProperties,
        uint8_t *pState,
        unsigned port,
        const uint8_t *payload
    ) const=0;
};

template<class TProp, class TState>
class DeviceTypeHelper
    : public DeviceType
{

protected:
    DeviceTypeHelper(
        std::vector<std::shared_ptr<MessageType> > inputPorts,
        std::vector<std::shared_ptr<MessageType> > outputPorts
    )
        : DeviceType(sizeof(TProp), sizeof(TState), inputPorts, outputPorts)
    {}

    /*
    Experimental - a bit silly

    template<class TMsg>
    void addReceiveHandler(unsigned portIndex, void (*onReceive)(const TProp *, TState *, const TMsg *)  )
    {

    }

    template<class TMsg,class TEdgeProps>
    void addReceiveHandler(unsigned portIndex, void (*onReceive)(const TProp *, TState *, const TMsg *, const TEdgeProps *)  )
    {

    }
    */

    virtual uint32_t GetRTS( const TProp *pProperties, const TState *pState) const=0;

    virtual void HashState(
        hash_t &base,
        const TState *pState
    ) const
    {
        return DeviceType::HashState(base, (const uint8_t*)pState); // Shouldn't be infinite recursion...
    }

    virtual void onInit(
        const TProp *pProperties,
        TState *pState
    ) const 
    {
        DeviceType::onInit((const uint8_t *)pProperties, (uint8_t*)pState);
    }

    virtual bool onSend(
        const TProp *pProperties,
        TState *pState,
        unsigned port,
        uint8_t *payload
    ) const =0;

    virtual void onRecv(
        const TProp *pProperties,
        TState *pState,
        unsigned port,
        const uint8_t *payload
    ) const =0;

public:

    virtual uint32_t GetRTS(
        const uint8_t *pProperties,
        const uint8_t *pState
    ) const final
    {
        return GetRTS((const TProp *)pProperties, (const TState *)pState);
    } 

    virtual void HashState(
        hash_t &base,
        const uint8_t *pState
    ) const final
    {
        return HashState(base, (const TState *)pState);
    }

    virtual void onInit(
        const uint8_t *pProperties,
        uint8_t *pState
    ) const final
    {
        return onInit((const TProp*)pProperties, (TState*)pState);
    }

    virtual bool onSend(
        const uint8_t *pProperties,
        uint8_t *pState,
        unsigned port,
        uint8_t *payload
    ) const final
    {
        return onSend((const TProp *)pProperties, (TState*)pState, port, payload);
    }

    virtual void onRecv(
        const uint8_t *pProperties,
        uint8_t *pState,
        unsigned port,
        const uint8_t *payload
    ) const final
    {
        return onRecv((const TProp *)pProperties, (TState*)pState, port, payload);
    }
};


struct message_t
{
    hash_t hash;
    endpoint_id_t dst;
    endpoint_id_t src;
    uint8_t size;
    uint8_t payload[64];
};

hash_t hash_message_except_dst(const message_t &msg, const hash_t &hashSrcEndpoint)
{
    hash_t res;
    res & msg.size; // Redundant, as it is implied by the tag
    for(unsigned i=0; i<msg.size; i++){
        res & msg.payload[i];
    }
    res & hashSrcEndpoint;
    return res;
}

hash_t hash_message_add_dst(const hash_t &msgHashExceptDst, const hash_t &hashDstEndpoint )
{
    hash_t res=msgHashExceptDst;
    res & hashDstEndpoint;
    return res;
}

void error_cycle(const char *message)
{
    fputs(message, stderr);
    exit(1);
}

struct device_instance_t
{
    // Immutable while running
    std::shared_ptr<DeviceType> deviceType;
    hash_t baseHash;    // This is a random per-device hash used to provide a unique starting point for each device's state hash
    std::vector<uint8_t> properties;
    
    std::vector< std::pair<hash_t, std::vector<std::pair<hash_t,endpoint_id_t> > > > outPorts; // index -> (srcHash,[(dstHash,dstEndpoint)])
    std::vector< hash_t > inPorts; // index -> dstHash
    
    // Mutable while running
    uint32_t rts;
    std::vector<uint8_t> state;
    hash_t currHash;    // The current hash of the device state

    void UpdateRTS()
    {
        rts=deviceType->GetRTS(&properties[0], &state[0]);
    }

    void UpdateHashAndRTS()
    {
        currHash=hash_t();
        deviceType->HashState(currHash, &state[0]);
        currHash & baseHash;
        UpdateRTS();
    }
};

struct world_state_t
{
    std::vector<device_instance_t> devices;
    hash_t devicesHash; // Sum of all device hashes

    std::vector<message_t> messages;
    hash_t messagesHash; // Sum of all message hashes

    void sanity()
    {
        hash_t altDevicesHash;
        for(auto &d : devices){
            hash_t tmp=d.currHash;
            d.UpdateHashAndRTS();
            assert(tmp==d.currHash);
            altDevicesHash.hash += d.currHash.hash;
        }
        assert(altDevicesHash==devicesHash);

    }

    device_id_t addDevice(std::shared_ptr<DeviceType> deviceType, std::vector<uint8_t> properties)
    {
        device_id_t res=devices.size();

        device_instance_t dev;
        dev.deviceType=deviceType;
        dev.baseHash=hash_t::Random();
        dev.properties=properties;

        for(auto input : deviceType->GetInputPorts()){
            dev.inPorts.push_back( hash_t::Random() );
        }
        for(auto output : deviceType->GetOutputPorts()){
            dev.outPorts.push_back( std::make_pair(hash_t::Random(), std::vector<std::pair<hash_t,endpoint_id_t> >() ) );
        }

        dev.rts=false;
        dev.state.resize(deviceType->GetStateSize());

        deviceType->onInit(&dev.properties[0], &dev.state[0]);

        dev.UpdateHashAndRTS();

        devicesHash.hash += dev.currHash.hash;

        devices.push_back(dev);

        return res;
    }

    template<class TProp>
    device_id_t addDevice(std::shared_ptr<DeviceType> deviceType, const TProp &props)
    {
        std::vector<uint8_t> bytes(sizeof(props));
        memcpy(&bytes[0], &props, sizeof(TProp));
        return addDevice(deviceType, bytes);
    }


    void addEdge(endpoint_id_t dst, endpoint_id_t src)
    {
        auto &dstDev=devices.at(dst.device);
        auto &srcDev=devices.at(src.device);

        auto dstMsgType=dstDev.deviceType->GetInputPorts().at(dst.port);
        auto srcMsgType=srcDev.deviceType->GetOutputPorts().at(src.port);

        if(!dstMsgType->Equal(srcMsgType.get())){
            throw std::runtime_error("Message types don't seem to match.");
        }

        srcDev.outPorts.at(src.port).second.push_back( std::make_pair(dstDev.inPorts.at(dst.port), dst) );
    }

    hash_t GetHash() const
    {
        hash_t res=devicesHash;
        res & messagesHash;
        return res;
    }

    std::vector<uint8_t> stash; // Stack of various bytes used during recursion
    std::unordered_set<hash_t> seen; // Hashes of states that have already been visited
    std::unordered_set<hash_t> history; // States that were visited in the current path
    std::vector<hash_t> historyList; // States that were visited in the current path

    std::unordered_set<hash_t> terminal; // States with no more transitions

    int indent=0;

    void log(const char *msg, ...)
    {
        /*for(int i=0; i<indent; i++){ putchar('|'); putchar(' '); }
        va_list v;
        va_start(v,msg);
        vprintf(msg, v);
        va_end(v);
        */
    }

    void beginEvent(const char *msg, ...)
    {
        
        /*for(int i=0; i<indent; i++){ putchar('|'); putchar(' '); }
        va_list v;
        va_start(v,msg);
        vprintf(msg, v);
        va_end(v);*/
        indent++;
    }

    void endEvent()
    {
        indent--;
    }

    template<class T>
    void push(const T &data)
    {
        uint8_t tmp[sizeof(T)];
        memcpy(tmp, &data, sizeof(T));
        stash.insert(stash.end(), tmp, tmp+sizeof(T));
    }

    template<class T>
    void pop(T &data)
    {
        assert(stash.size()>=sizeof(T));
        uint8_t tmp[sizeof(T)];
        std::copy(stash.end()-sizeof(T), stash.end(), tmp);
        memcpy(&data, tmp, sizeof(T));
        stash.resize(stash.size()-sizeof(T));
    }

    void push(const std::vector<uint8_t> &data)
    {
        stash.insert(stash.end(), data.begin(), data.end());
        uint32_t size=data.size();
        push(size);
    }

    void pop(std::vector<uint8_t> &data)
    {
        uint32_t size;
        pop(size);
        assert(size==data.size());
        std::copy(stash.end()-size, stash.end(), data.begin());
        stash.erase(stash.end()-size, stash.end());
    }

    void appendMessage(const message_t &msg)
    {
        messages.push_back(msg);
        messagesHash.hash += msg.hash.hash;
    }

    void popMessagesToSize(uint32_t targetSize)
    {
        assert(targetSize <= messages.size());
        for(unsigned i=targetSize; i<messages.size(); i++){
            messagesHash.hash -= messages[i].hash.hash;
        }
        messages.resize(targetSize);
    }

    void insertMessage(unsigned index, message_t &msg)
    {
        assert(index <= messages.size());
        messages.insert(messages.begin()+index, msg);
        messagesHash.hash += msg.hash.hash;
    }

    void removeMessage(unsigned index, message_t &msg)
    {
        assert(index<messages.size());
        msg=messages[index];
        messages.erase(messages.begin()+index);
        messagesHash.hash -= msg.hash.hash;
    }
};

void run(world_state_t &ws);

void run_do_sends(world_state_t &ws)
{
    for(unsigned i=0; i<ws.devices.size(); i++){
        if(!ws.devices[i].rts){
            continue;
        }

        ws.beginEvent("Send device=%u, devHash=%llu\n", i, (unsigned long long)ws.devices[i].currHash.hash);

        ws.sanity();

        // Do all of this locally, hopefully don't keep on the stack across recursive call
        {
            // Sanity check - push the current hash
            ws.push( ws.GetHash() );

            // Save the number of messages currently in service (so that we can cheaply restore later)
            uint32_t msgCount=ws.messages.size();
            ws.push( msgCount );

            auto &device=ws.devices[i];
            auto &deviceType=device.deviceType;

            // Save the state and hash for the device that will send
            hash_t prevHash=device.currHash;
            ws.push( device.state );
            ws.push( device.currHash );
            ws.devicesHash.hash -= device.currHash.hash;

            // NOTE: we follow priority here, so we don't support send handlers out of order!!!!!
            // BUG: this is probably a bug
            assert(device.rts);
            unsigned sendPort=0;
            while(!( (1<<sendPort) & device.rts ) ){
                ++sendPort;
            }

            message_t msg;
            bool doSend=deviceType->onSend(&device.properties[0], &device.state[0], sendPort, msg.payload  );
            device.UpdateHashAndRTS();
            ws.devicesHash.hash += device.currHash.hash;
            
            if(device.currHash == prevHash){
                error_cycle("Device sent message but did not change state. Infinite messages possible.");
            }

            if(doSend){
                const auto &outPort=device.outPorts[sendPort];
                hash_t baseMsgHash=hash_message_except_dst(msg, outPort.first);
                for(const auto &dst : outPort.second){
                    msg.dst = dst.second;
                    msg.hash = hash_message_add_dst(baseMsgHash, dst.first);

                    ws.appendMessage(msg);
                }
            }

            ws.sanity();
        }
        // We have now done everything todo with the send itself and inserting the messages into the message queue.
        // Hopefully the intermediate stuff will be popped off the stack...?


        // Recurse!
        run(ws);

        {
            ws.sanity();

            // Restore the world to the way it was
            auto &device=ws.devices[i];

            ws.devicesHash.hash-=device.currHash.hash;
            ws.pop(device.currHash);
            ws.pop(device.state);
            ws.devicesHash.hash+=device.currHash.hash;

            device.UpdateRTS();
            assert(device.rts);

            uint32_t msgCount;
            ws.pop(msgCount);
            ws.popMessagesToSize(msgCount);

            hash_t expHash;
            ws.pop(expHash);
            if(expHash != ws.GetHash() ){
                throw std::runtime_error("Hash on the way back doesn't match hash on the way in.");
            }

            ws.sanity();

            ws.endEvent();
        }
    }
}

void run_do_recvs(world_state_t &ws)
{
    for(unsigned i=0; i<ws.messages.size(); i++){
        // TODO / WARNING : This message shuffling is O(messages.size() ^2), but it could be O( messages.size() ) with not that much work
        assert(ws.messages.size() < 64);

        {
            // Sanity check - push the current hash
            ws.push( ws.GetHash() );

            message_t msg;
            ws.removeMessage(i, msg);

            ws.beginEvent("Recv device=%u\n", msg.dst);

            auto &device=ws.devices[msg.dst.device];
            auto &deviceType=device.deviceType;

            ws.push(device.currHash);
            ws.push(device.state);

            ws.push(msg);

            //////////////////////////
            // Everything is saved, now move forwards

            unsigned recvPort=msg.dst.port;

            ws.devicesHash.hash -= device.currHash.hash;
            deviceType->onRecv(&device.properties[0], &device.state[0], recvPort, msg.payload );
            device.UpdateHashAndRTS();
            ws.devicesHash.hash += device.currHash.hash;
        }

        // Recurse!
        run(ws);

        {
            message_t msg;
            ws.pop(msg);

            auto &device=ws.devices[msg.dst.device];

            ws.devicesHash.hash -= device.currHash.hash;
            ws.pop(device.state);
            ws.pop(device.currHash);
            device.UpdateRTS();
            ws.devicesHash.hash += device.currHash.hash;

            ws.insertMessage(i, msg);

            hash_t expHash;
            ws.pop(expHash);
            if(expHash != ws.GetHash() ){
                throw std::runtime_error("Hash on the way back doesn't match hash on the way in.");
            }

            ws.endEvent();
        }
    }
}


void run(world_state_t &ws)
{
    {
        ws.sanity();

        unsigned rts=0;
        for(unsigned i=0; i<ws.devices.size(); i++){
            if(ws.devices[i].rts)
                rts++;
        }

        ws.log("World = %llu, RTS = %u, messages = %u, depth = %d\n", (unsigned long long)ws.GetHash().hash, rts, ws.messages.size(), ws.historyList.size());
        for(unsigned i=0; i<ws.devices.size(); i++){
            ws.log("  ds[%u]=%llu\n", i, ws.devices[i].currHash.hash);
        }

        hash_t hash=ws.GetHash();
        auto it=ws.seen.find(hash);
        if(it!=ws.seen.end()){
            if(ws.history.end()!=ws.history.find(hash)){
                ws.log("Duplicate state %llu\n", (unsigned long long)hash.hash);
                for(int i=ws.historyList.size()-1; i>=0; i--){
                    ws.log("  ws[%u]=%llu\n", i, ws.historyList[i].hash);
                }
                for(unsigned i=0; i<ws.devices.size(); i++){
                    ws.log("  ds[%u]=%llu\n", i, ws.devices[i].currHash.hash);
                }
                error_cycle("Encountered duplicate state.");
            }
            return;
        }

        if(rts==0 && ws.messages.size()==0){
            ws.terminal.insert(hash);
            return;
        }

        ws.seen.insert(it, hash);
        ws.history.insert(hash);
        ws.historyList.push_back(hash);

        ws.push(hash);
    }

    //////////////////////////////////////////////////////
    // Do any potential sends
    if(ws.messages.size()<3){
        run_do_sends(ws);    
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Consider any potential receives
    run_do_recvs(ws);

    {
        hash_t hash;
        ws.pop(hash);

        if(hash != ws.GetHash()){
            throw std::runtime_error("State on the way out doesn't match way in.");
        }

        ws.history.erase(hash);
        ws.historyList.pop_back();
    }
}

#endif