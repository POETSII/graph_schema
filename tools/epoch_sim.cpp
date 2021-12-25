#include "graph.hpp"

#include <libxml++/parsers/domparser.h>
#include <libxml++/document.h>

#include <unordered_map>
#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <signal.h>
#include <regex>
#include <thread>
#include <chrono>

#include "fenv_control.hpp"

#include "poets_protocol/InProcessBinaryUpstreamConnection.hpp"

#include <cstring>
#include <cstdlib>
#include <cstdarg>

static unsigned logLevel=2;
static unsigned messageInit;


struct InProcMessageBuffer
  : public InProcessBinaryUpstreamConnection
{
  const unsigned MAX_EXT2INT_IN_FLIGHT = 65536;
  const unsigned MAX_INT2EXT_IN_FLIGHT = 65536;

  struct message
  {
    poets_endpoint_address_t address;
    std::vector<uint8_t> payload;
    unsigned sendIndex;
  };

  std::function<void(const std::string &graph_type,const std::string &graph_instance,const std::vector<std::pair<std::string,std::string>> &owned)> m_connect;
  std::function<size_t (size_t cbBuffer, void *pBuffer)> m_getGraphProperties;
  std::function<size_t(poets_device_address_t address, size_t cbBuffer, void *pBuffer)> m_getDeviceProperties;
 
  std::function<poets_device_address_t(const std::string &s)> m_idToAddress;
  std::function<std::string(poets_device_address_t)> m_addressToId;
  std::function<void(poets_endpoint_address_t,std::vector<poets_endpoint_address_t>&)> m_endpointToFanout;

  std::mutex m_mutex;
  std::condition_variable m_cond;
  std::queue<message> m_ext2int;
  std::queue<message> m_int2ext;

  int m_waiting=0;

  std::shared_ptr<halt_message_type> m_halt;

  void post_message(poets_endpoint_address_t address, std::vector<uint8_t> &payload, unsigned sendIndex)
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_int2ext.push(message{address,payload,sendIndex});
    if(m_waiting>0){
      m_cond.notify_all();
    }
  }

  void wait_for_client_to_send()
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_waiting++;
    m_cond.wait(lk, [&](){
      return !m_ext2int.empty();
    });
    m_waiting--;
  }

  void wait_for_client_to_drain()
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    if(m_int2ext.size() < MAX_INT2EXT_IN_FLIGHT){
      return;
    }

    fprintf(stderr, "Simulator is waiting for external to drain messages.\n");
    m_waiting++;
    m_cond.wait(lk, [&](){
      return m_int2ext.size() < MAX_INT2EXT_IN_FLIGHT;
    });
    m_waiting--;
  }

  void set_halt_message(TypedDataPtr message)
  {
    if(message.payloadSize()!=sizeof(halt_message_type)){
      throw std::runtime_error("Received a halt message of the wrong size (internal error?)");
    }

    fprintf(stderr, "set_halt_message - code=%u\n", ((halt_message_type*)message.payloadPtr())->code);

    std::unique_lock<std::mutex> lk(m_mutex);
    assert(!m_halt);
    m_halt.reset(new halt_message_type());
    memcpy(m_halt.get(), message.payloadPtr(), sizeof(halt_message_type));
    m_cond.notify_all();
  }

  ////////////////////////////////////
  // Client interface

    virtual void connect(
        const std::string &graph_type,      //! Regex for matching graph type, or empty
        const std::string &graph_instance,  //! Regex for matching graph instance, or empty
        const std::vector<std::pair<std::string,std::string>> &owned    //! Vector of (device_id,device_type) pairs
    ) {
      std::unique_lock<std::mutex> lk;
      if(!m_connect){
        throw std::runtime_error("connect was called twice.");
      }
      m_connect(graph_type, graph_instance, owned);
      m_connect=nullptr;
    }

    size_t get_graph_properties(size_t cbBuffer, void *pBuffer) override
    { return m_getGraphProperties(cbBuffer, pBuffer); }

    size_t get_device_properties(poets_device_address_t address, size_t cbBuffer, void *pBuffer) override
    { return m_getDeviceProperties(address, cbBuffer, pBuffer); }

    poets_device_address_t get_device_address(const std::string &id) override
    { return m_idToAddress(id); }

    std::string get_device_id(poets_device_address_t address) override
    { return m_addressToId(address); }

    void get_endpoint_destinations(poets_endpoint_address_t source, std::vector<poets_endpoint_address_t> &destinations) override
    { m_endpointToFanout(source, destinations); }


    bool can_send() override
    {
      std::unique_lock<std::mutex> lk(m_mutex);
      return m_ext2int.size() < MAX_EXT2INT_IN_FLIGHT;
    }

    bool send(
        poets_endpoint_address_t source,
        std::vector<uint8_t> &payload,
        unsigned sendIndex
    ) override {
      std::unique_lock<std::mutex> lk(m_mutex);
      if(m_ext2int.size() >= MAX_EXT2INT_IN_FLIGHT){
        return false;
      }

      message m;
      m.address=source;
      m.sendIndex=sendIndex;
      std::swap(m.payload, payload);

      m_ext2int.push(std::move(m));
      m_cond.notify_one();

      return true;
    }

    void flush() override
    {
      // This is a no-op for us
    }

    bool can_recv() override
    {
      std::unique_lock<std::mutex> lk(m_mutex);
      return !m_int2ext.empty();
    }

    bool recv(
        poets_endpoint_address_t &source,
        std::vector<uint8_t> &payload,
        unsigned &sendIndex
    ) override {
      std::unique_lock<std::mutex> lk(m_mutex);
      if(m_int2ext.empty()){
        return false;
      }

      auto &m=m_int2ext.front();
      source=m.address;
      std::swap(payload, m.payload);
      sendIndex=m.sendIndex;
      
      m_int2ext.pop();
      m_cond.notify_one();
      return true;
    }

    virtual const halt_message_type *get_terminate_message()
    {
      std::unique_lock<std::mutex> lk(m_mutex);
      return m_halt.get();
    }

    using Events = InProcessBinaryUpstreamConnection::Events;

    Events wait_until(
        Events events,
        uint64_t timeoutMicroSeconds
    ) override {
      std::unique_lock<std::mutex> lk(m_mutex);

      unsigned res;
    
      auto update_res=[&]() -> unsigned
      {
        res=0;
        if(m_ext2int.size() < MAX_EXT2INT_IN_FLIGHT){
          res |= Events::CAN_SEND;
        }
        if(!m_int2ext.empty()){
          res |= Events::CAN_RECV;
        }
        if(m_halt){
          res |= Events::TERMINATED;
        }
        return res;
      };

      m_waiting++;
      if(events==0){
        // pass
      }else if( (events&Events::TIMEOUT) && (timeoutMicroSeconds!=0)){
        m_cond.wait_for(lk, std::chrono::microseconds(timeoutMicroSeconds),  [&](){
          return (update_res()&events)!=0; 
        });
      }else{
        m_cond.wait(lk, [&](){
          return (update_res()&events)!=0; 
        });
      };
      m_waiting--;
      return Events(res);
    }

    virtual void external_log(
        int level,
        const char *msg,
        ...
    ){
      if(level < (int)logLevel){
        va_list va;
        va_start(va, msg);
        vfprintf(stderr, msg, va);
        va_end(va);
        fputs("\n", stderr);
      }
    }
};


struct EpochSim
  : public GraphLoadEvents
{
  std::unordered_set<std::string> m_interned;

  // Return a stable C pointer to the name. Avoids us to store
  // pointers in the data structures, and avoid calling .c_str() everywhere
  const char *intern(const std::string &name)
  {
    auto it=m_interned.insert(name);
    return it.first->c_str();
  }

  struct output
  {
    unsigned dstDevice;
    unsigned dstPinIndex;
    unsigned dstPinSlot; // This is the actual landing zone within the destination, i.e. where the state is
    const char *dstDeviceId;
    const char *dstInputName;
    int sendIndex;
  };

  struct input
  {
    TypedDataPtr properties;
    TypedDataPtr state;
    unsigned firings;
    const char *id;
  };

  struct device
  {
    unsigned index;
    std::string id;
    const char *name; // interned id
    bool isExternal;
    DeviceTypePtr type;
    TypedDataPtr properties;
    TypedDataPtr state;

    unsigned outputCount;
    uint32_t readyToSend;

    std::vector<const char *> outputNames; // interned names

    std::vector<std::vector<output> > outputs;
    std::vector<std::vector<input> > inputs;

    std::queue<std::tuple<unsigned,TypedDataPtr,int>> ext2int; // (port,message,sendIndex)

    void post_message(unsigned port, TypedDataPtr payload, int sendIndex)
    {
      ext2int.push(std::make_tuple(port, payload, sendIndex));
      if(ext2int.size()==1){
        readyToSend = 1u<<port;
      }
    }

    std::tuple<unsigned,TypedDataPtr,int> pop_message()
    {
      assert(!ext2int.empty());
      auto res=ext2int.front();
      ext2int.pop();
      if(ext2int.empty()){
        readyToSend=0;
      }else{
        readyToSend=1u<<std::get<0>(ext2int.front());
      }
      return res;
    }

    bool anyReady() const
    {
      return readyToSend!=0;
    }
  };

  GraphTypePtr m_graphType;
  std::string m_id;
  TypedDataPtr m_graphProperties;
  std::vector<device> m_devices;
  std::shared_ptr<LogWriter> m_log;

  std::unordered_map<const char *,unsigned> m_deviceIdToIndex;
  std::vector<std::tuple<const char*,unsigned,uint32_t,uint32_t> > m_keyValueEvents;
  std::unordered_set<unsigned> m_externalIndices;
  int m_haltDeviceIndex=-1;
  TypedDataPtr m_haltMessage;

  std::function<void (const char*,bool,int,const char *)> m_onCheckpoint;

  int m_checkpointLevel=0;
  std::vector<std::pair<bool,std::string> > m_checkpointKeys;

  bool m_deviceExitCalled=false;
  bool m_deviceExitCode=0;
  std::function<void (const char*,int)> m_onDeviceExit;

  std::shared_ptr<InProcMessageBuffer> m_pExternalBuffer;

  struct delayed_message_t
  {
    std::string idSend;
    output *out;
    TypedDataPtr payload;
    unsigned src_epoch;
  };

  std::vector<delayed_message_t> m_delayed;

  uint64_t m_unq;

  uint64_t nextSeqUnq()
  {
    return ++m_unq;
  }

  virtual uint64_t onBeginGraphInstance(const GraphTypePtr &graphType, const std::string &id, const TypedDataPtr &graphProperties, rapidjson::Document &&) override
  {
    m_graphType=graphType;
    m_id=id;
    m_graphProperties=graphProperties;
    return 0;
  }

  virtual uint64_t onDeviceInstance(uint64_t gId, const DeviceTypePtr &dt, const std::string &id, const TypedDataPtr &deviceProperties, const TypedDataPtr &deviceState, rapidjson::Document &&) override
  {
    if(dt->isExternal()){
      if(!m_pExternalBuffer){
        throw std::runtime_error("Graph contains an external instance, but no external connection has been specified.");
      }
    }

    device d;
    d.index=m_devices.size();
    d.id=id;
    d.name=intern(id);
    d.type=dt;
    d.isExternal=dt->isExternal();

    d.properties=deviceProperties;
    d.state=deviceState;
    d.readyToSend=0;
    d.outputCount=dt->getOutputCount();
    d.outputs.resize(dt->getOutputCount());
    for(unsigned i=0;i<dt->getOutputCount();i++){
      d.outputNames.push_back(intern(dt->getOutput(i)->getName()));
    }
    d.inputs.resize(dt->getInputCount());
    m_devices.push_back(d);
    m_deviceIdToIndex[d.name]=d.index;

    if(d.isExternal){
      if(id=="__halt__"){
        m_haltDeviceIndex=d.index;
      }else{
        m_externalIndices.insert(d.index);
      }
    }

    return d.index;
  }

  void onEdgeInstance(uint64_t gId, uint64_t dstDevIndex, const DeviceTypePtr &dstDevType, const InputPinPtr &dstInput, uint64_t srcDevIndex, const DeviceTypePtr &srcDevType, const OutputPinPtr &srcOutput, int sendIndex, const TypedDataPtr &properties, const TypedDataPtr &state, rapidjson::Document &&) override
  {
    input i;
    i.properties=properties;
    i.state=state;
    i.firings=0;
    i.id=intern( m_devices.at(dstDevIndex).id + ":" + dstInput->getName() + "-" + m_devices.at(srcDevIndex).id+":"+srcOutput->getName() );
    auto &slots=m_devices.at(dstDevIndex).inputs.at(dstInput->getIndex());
    unsigned dstPinSlot=slots.size();
    slots.push_back(i);

    output o;
    o.dstDevice=dstDevIndex;
    o.dstPinIndex=dstInput->getIndex();
    o.dstPinSlot=dstPinSlot;
    o.dstDeviceId=m_devices.at(dstDevIndex).name;
    o.dstInputName=intern(dstInput->getName());
    o.sendIndex=sendIndex;
    m_devices.at(srcDevIndex).outputs.at(srcOutput->getIndex()).push_back(o);

  }

  void onEndEdgeInstances(uint64_t ) override
  {
    for(auto &d : m_devices){
      for(auto &op : d.type->getOutputs()){
        if(op->isIndexedSend()){
          auto &ov = d.outputs.at(op->getIndex());
          bool anyIndexed=false;
          bool anyNonIndexed=false;
          for(auto x : ov){
            if(x.sendIndex!=-1){
              anyIndexed=true;
            }else{
              anyNonIndexed=true;
            }
          }
          if(anyIndexed && anyNonIndexed){
            std::stringstream tmp;
            tmp<<"Output "<<d.name<<":"<<op->getName()<<" has both explict and non explicit send indices.";
            throw std::runtime_error(tmp.str());
          }

          if(anyIndexed){
            std::sort(ov.begin(), ov.end(), [](const output &a, const output &b){ return a.sendIndex < b.sendIndex; });
            for(int i=0; i<(int)ov.size(); i++){
              if(i!=ov[i].sendIndex){
                throw std::runtime_error("Explicit send indices are not contiguous and/or don't start at zero.");
              }
            }            
          }
        }
      }
    }
  }


  unsigned pick_bit(unsigned n, uint32_t bits, unsigned rot)
  {
    if(bits==0)
      return (unsigned)-1;

    for(unsigned i=0;i<n;i++){
      unsigned s=(i+rot)%n;
      if( (bits>>s)&1 )
        return s;
    }

    assert(0);
    return (unsigned)-1;
  }


  void writeSnapshot(SnapshotWriter *dst, double orchestratorTime, unsigned sequenceNumber)
  {
    dst->startSnapshot(m_graphType, m_id.c_str(), orchestratorTime, sequenceNumber);

    for(auto &dev : m_devices){
      dst->writeDeviceInstance(dev.type, dev.name, dev.state, dev.readyToSend);
      for(unsigned i=0; i<dev.inputs.size(); i++){
        const auto &ip=dev.type->getInput(i);
        for(auto &slot : dev.inputs[i]){
          dst->writeEdgeInstance(ip, slot.id, slot.state, slot.firings, 0, 0);
          slot.firings=0;
        }
      }
    }

    dst->endSnapshot();
  }

  void init()
  {

    m_onDeviceExit=[&](const char *id, int code) ->void
    {
      m_deviceExitCalled=true;
      m_deviceExitCode=code;
      fprintf(stderr, "  device '%s' called application_exit(%d)\n", id, code);
    };

    if(!m_log){
      m_onCheckpoint=[this](const char *id, bool preEvent, int level, const char *key)
      {};
    }else{
      m_onCheckpoint=[this](const char *id, bool preEvent, int level, const char *key)
      {
        if(level<=m_checkpointLevel){
          m_checkpointKeys.push_back(std::make_pair(preEvent,key));
        }
      };
    }

    for(auto &dev : m_devices){
      ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, dev.name, "Init handler", m_onDeviceExit, m_onCheckpoint  };
      if(!dev.isExternal){
        dev.type->init(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get());
        dev.readyToSend = dev.type->calcReadyToSend(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get());
      }

      if(m_log){
        // Try to avoid allocation, while gauranteeing m_checkpointKeys is left in empty() state
        std::vector<std::pair<bool,std::string> > tags;
        std::swap(tags,m_checkpointKeys);
        auto id=nextSeqUnq();
        auto idStr=std::to_string(id);
        m_log->onInitEvent(
          idStr.c_str(),
          0.0,
          0.0,
          std::move(tags),
          dev.type,
          dev.name,
          dev.readyToSend,
          id,
          std::vector<std::string>(),
          dev.state
        );
      }
    }
  }

  double m_statsSends=0;
  double m_statsDelays=0;
  double m_statsShearCount=0;
  double m_statsShearSum=0;
  double m_statsShearSumSqr=0;
  double m_statsShearMax=0;
  unsigned m_epoch=0;

//Generate either a zero initialised message, or a random message based on
//messageInit, set as an argument.
  TypedDataPtr getMessage(MessageTypePtr m)
  {
    TypedDataPtr res;
    unsigned r = 2;//Init at 2 incase the message is not to be randomly zero or random.
    if (messageInit == 2)
    {// If the initialisation is to be randomly zero or random, generate a random value for this.
      r = rand() % 2;
    }

    if (messageInit == 0 || r == 0)
    {//Zero initialised message
      res = m->getMessageSpec()->create();
    } else if (messageInit == 1 || r == 1)
    {//Random message
      auto empty = m->getMessageSpec()->create();
      unsigned int size = ((int*) empty.get())[1];

      typed_data_t *p=(typed_data_t*)malloc(size);
      p->_ref_count=0;
      p->_total_size_bytes=size;

      auto tup = m->getMessageSpec()->getTupleElement();
      tup->createBinaryRandom(((char*)p)+sizeof(typed_data_t), size-8);
      res = TypedDataPtr(p);
    }

    return res;

  }

  void do_hardware_idle()
  {
    auto barrierId="b"+std::to_string(nextSeqUnq());

    if(logLevel>1){
      fprintf(stderr, "onHardwareIdle\n");
    }
    for(auto &d : m_devices){
      if(!d.isExternal){
        ReceiveOrchestratorServicesImpl services{logLevel, stderr, d.name, "Idle handler", m_onDeviceExit, m_onCheckpoint  };
        d.type->onHardwareIdle(&services, m_graphProperties.get(), d.properties.get(), d.state.get()  );
        d.readyToSend = d.type->calcReadyToSend(&services, m_graphProperties.get(), d.properties.get(), d.state.get());

        if(m_log){
          auto id=nextSeqUnq();
          auto idStr=std::to_string(id);
          m_log->onHardwareIdleEvent(
            idStr.c_str(),
            0.0,
            0.0,
            std::vector<std::pair<bool,std::string> >(), // No tags
            d.type,
            d.name,
            d.readyToSend,
            id,
            std::vector<std::string>(),
            d.state,
            barrierId.c_str()
          );
        }
      }else{
        // Device is an external, which is not involved in hardware idle
      }
    }
  }

  template<class TRng>
  bool step(TRng &rng, double probSend,bool capturePreEventState, double probDelay)
  {

    // Within each step every object gets the chance to send a message with probability probSend
    std::uniform_real_distribution<> udist;

    ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, 0, 0, m_onDeviceExit, m_onCheckpoint};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Recv: ";
      receiveServices.setPrefix(tmp.str().c_str());
    }

    SendOrchestratorServicesImpl sendServices{logLevel, stderr, 0, 0, m_onDeviceExit, m_onCheckpoint};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Send: ";
      sendServices.setPrefix(tmp.str().c_str());
    }

  

    auto do_recv=[&](output &out, const TypedDataPtr &message, const std::string &idSend, unsigned srcEpoch)
    {
      auto &dst=m_devices[out.dstDevice];
      auto &in=dst.inputs[out.dstPinIndex];
      auto &slot=in[out.dstPinSlot];

      slot.firings++;

      if(srcEpoch!=m_epoch){
        double shear=m_epoch-srcEpoch;
        m_statsShearCount++;
        m_statsShearMax=std::max(m_statsShearMax, shear);
        m_statsShearSum+=shear;
        m_statsShearSumSqr+=shear*shear;
      }

      if(logLevel>3){
        fprintf(stderr, "    sending to device %d = %s\n", dst.index, dst.id.c_str());
      }

      const auto &pin=dst.type->getInput(out.dstPinIndex);

      if(!dst.isExternal){
        TypedDataPtr prevState;
        if(capturePreEventState){
          prevState=dst.state.clone();
        }
        receiveServices.setDevice(out.dstDeviceId, out.dstInputName);
        try{
          pin->onReceive(&receiveServices, m_graphProperties.get(), dst.properties.get(), dst.state.get(), slot.properties.get(), slot.state.get(), message.get());
        }catch(provider_assertion_error &e){
          fprintf(stderr, "Caught handler exception during Receive. devId=%s, dstDevType=%s, dstPin=%s.\n", dst.name, dst.type->getId().c_str(), pin->getName().c_str());
          fprintf(stderr, "  %s\n", e.what());

          fprintf(stderr, "     message = %s\n", pin->getMessageType()->getMessageSpec()->toJSON(message).c_str());
          if(capturePreEventState){
            fprintf(stderr, "  preRecvState = %s\n", dst.type->getStateSpec()->toJSON(prevState).c_str());
          }
          fprintf(stderr, "     currState = %s\n", dst.type->getStateSpec()->toJSON(dst.state).c_str());

          throw;
        }
        dst.readyToSend = dst.type->calcReadyToSend(&receiveServices, m_graphProperties.get(), dst.properties.get(), dst.state.get());
      }else{
        throw std::runtime_error("Should not be handling external messages here.");
      }

      if(m_log){
        std::vector<std::pair<bool,std::string> > tags;
        std::swap(tags, m_checkpointKeys);

        auto id=nextSeqUnq();
        auto idStr=std::to_string(id);

        m_log->onRecvEvent(
          idStr.c_str(),
          m_epoch,
          0.0,
          std::move(tags),
          dst.type,
          dst.name,
          dst.readyToSend,
          id,
          std::vector<std::string>(),
          dst.state,
          pin,
          idSend.c_str()
        );
      }
    };
    

   

    /// Drain any incoming external messages first;
    if(m_pExternalBuffer){
        std::queue<InProcMessageBuffer::message> pending;
        {
          std::unique_lock<std::mutex> lk(m_pExternalBuffer->m_mutex);
          std::swap(pending, m_pExternalBuffer->m_ext2int);
          if(!pending.empty()){
            m_pExternalBuffer->m_cond.notify_one();
          }
        }
        while(!pending.empty()){
          const auto &m=pending.front();
          unsigned devIndex=getEndpointDevice(m.address).value;
          unsigned portIndex=getEndpointPin(m.address).value;
          if(devIndex>=m_devices.size()){
            throw std::runtime_error("External connection sent message from non-existent external address.");
          }
          auto &dev=m_devices.at(devIndex);
          if(!dev.isExternal){
            throw std::runtime_error("External connection sent message from an address that is not an external.");
          }
          if(portIndex >= dev.outputCount){
            throw std::runtime_error("External connection sent message from an invalid output port address.");
          }
          auto port=dev.type->getOutput(portIndex);
          int sendIndex=m.sendIndex==UINT_MAX ? -1 : m.sendIndex;
          if( (sendIndex!=-1) != port->isIndexedSend() ){
            throw std::runtime_error("External connection sent message where sendIndex did not match indexed type of port.");
          }
          auto spec=port->getMessageType()->getMessageSpec();
          if(spec->payloadSize() != m.payload.size()){
            throw std::runtime_error("External connection sent message where payload size did not match type's payload size.");
          }
          TypedDataPtr p=spec->create();
          memcpy(p.payloadPtr(), &m.payload.at(0), p.payloadSize());
          dev.post_message(portIndex, p, sendIndex);
          
          pending.pop();
        }

        // Also make sure that we don't have huge numbers of messages building
        // up that the client hasn't drained
        m_pExternalBuffer->wait_for_client_to_drain();
    }

    ///////////////////////////////////////
    // Pick any random messages in the buffer

    if(!m_delayed.empty()){
      double probRelease=1-probDelay;
      
      std::binomial_distribution<> dist(m_delayed.size(), probRelease);
      unsigned n=dist(rng);
      assert(n <= m_delayed.size());

      for(int i=0; i<(int)n; i++){
        unsigned sel=rng() % m_delayed.size();
        auto &m=m_delayed.at(sel);
        do_recv(*m.out, m.payload, m.idSend, m.src_epoch);
        std::swap(m_delayed[sel], m_delayed.back());
        m_delayed.resize(m_delayed.size()-1);
      }
    }

    ////////////////////////////////////////
    //

    std::vector<int> sendSel(m_devices.size());

    unsigned rotA=rng();
    for(unsigned i=0;i<m_devices.size();i++){
      auto &src=m_devices[i];

      // Pick a random message
      sendSel[i]=pick_bit(src.outputCount, src.readyToSend, rotA+i);
    }

    // If accurateAssertionInfo, then we capture full state before each send/recv
    TypedDataPtr prevState;

    bool sent=false;
    bool anyReady=false;

    unsigned rot=rng();

    double threshSendDbl=ldexp(probSend, 32);
    uint32_t threshSend=(uint32_t)std::min((double)0xFFFFFFFFul,std::max(0.0,threshSendDbl));
    uint32_t threshRng=rng();

    uint32_t threshDelay=(uint32_t)std::min((double)0xFFFFFFFFul,std::max(0.0,ldexp(probDelay,32)));

    for(unsigned i=0;i<m_devices.size();i++){
      unsigned index=(i+rot)%m_devices.size();

      auto &src=m_devices[index];

      if(logLevel>3){
        fprintf(stderr, "  step device %d = %s\n", src.index, src.id.c_str());
      }

      // Pick a random message
      int sel=sendSel[index];
      if(sel==-1){
        if(logLevel>3){
          fprintf(stderr, "   not ready to send.\n");
        }
        continue;
      }

      threshRng= threshRng*1664525+1013904223UL;
      if(threshRng > threshSend){
        anyReady=true;
        continue;
      }

      if(!((src.readyToSend>>sel)&1)){
        anyReady=true; // We don't know if it turned any others on
        continue;
      }

      if(logLevel>3){
        fprintf(stderr, "    output pin %d ready\n", sel);
      }

      m_statsSends++;

      const OutputPinPtr &output=src.type->getOutput(sel);
      TypedDataPtr message(getMessage(output->getMessageType()));

      std::string idSend;

      bool doSend=true;
      unsigned sendIndexStg=-1;
      unsigned *sendIndex=0;
      {
        #ifndef NDEBUG
        if(src.isExternal){
          assert(!src.ext2int.empty());
          assert(std::get<0>(src.ext2int.front())==(int)sel);
        }else{
          uint32_t check=src.type->calcReadyToSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get());
          assert(check);
          assert( (check>>sel) & 1);
        }
        #endif

        if(output->isIndexedSend()){
          sendIndex=&sendIndexStg;
        }

        if(!src.isExternal){
          if(capturePreEventState){
            prevState=src.state.clone();
          }
          sendServices.setDevice(src.name, src.outputNames[sel]);
          try{
            // Do the actual send handler
            output->onSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get(), message.get(), &doSend, sendIndex);
          }catch(provider_assertion_error &e){
            fprintf(stderr, "Caught handler exception during send. devId=%s, devType=%s, outPin=%s.", src.name, src.type->getId().c_str(), output->getName().c_str());
            fprintf(stderr, "  %s\n", e.what());

            if(capturePreEventState){
              fprintf(stderr, "  preSendState = %s\n", src.type->getStateSpec()->toJSON(prevState).c_str());
            }
            fprintf(stderr, "     currState = %s\n", src.type->getStateSpec()->toJSON(src.state).c_str());
            throw;
          }

          if(sendIndex && sendIndexStg >= src.outputs[sel].size()){
            fprintf(stderr, "Application tried to specify sendIndex of %u, but out degree is %u\n", sendIndexStg, (unsigned)src.outputs.size());
            exit(1);
          }

          src.readyToSend = src.type->calcReadyToSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get());
        }else{
          assert(m_pExternalBuffer);
          auto m=src.pop_message();

          // Should have all been full validated when unpacking/importing message
          assert(std::get<0>(m)==sel);
          assert( std::get<1>(m).payloadSize()==message.payloadSize() );
          assert( (std::get<2>(m)!=-1) == (sendIndex!=0) );

          message=std::get<1>(m);
          if(sendIndex){
            *sendIndex=std::get<2>(m);
          }
        }

        if(m_log){
          // Try to avoid allocation, while gauranteeing m_checkpointKeys is left in empty() state
          std::vector<std::pair<bool,std::string> > tags;
          std::swap(tags, m_checkpointKeys);

          auto id=nextSeqUnq();
          auto idStr=std::to_string(id);
          idSend=idStr;

          m_log->onSendEvent(
            idStr.c_str(),
            m_epoch,
            0.0,
            std::move(tags),
            src.type,
            src.name,
            src.readyToSend,
            id,
            std::vector<std::string>(),
            src.state,
            output,
            !doSend,
            doSend ? src.outputs.size() : 0,
            message
          );
        }

      }

      if(!doSend){
        if(logLevel>3){
          fprintf(stderr, "    send aborted.\n");
        }
        anyReady = anyReady || src.anyReady();
        continue;
      }

      sent=true;

      // Try to support both indexed sends and broadcast, though indexed are less efficient
      auto *pOutputVec=&src.outputs[sel];
      std::vector<EpochSim::output> indexedOutputBuffer;
      if(sendIndex){
        // This is quite inefficient
        indexedOutputBuffer.push_back( pOutputVec->at(*sendIndex) );
        pOutputVec = &indexedOutputBuffer;
      }
      
      bool sendToExternalConnection=false;
      for(auto &out : *pOutputVec){
        auto &dst = m_devices[out.dstDevice];

        if(dst.index==(unsigned)m_haltDeviceIndex){
          // Have to special case the halt
          fprintf(stderr, "Received halt message from somewhere.\n");
          if(message.payloadSize()!=sizeof(halt_message_type)){
            throw std::runtime_error("Receive halt message with the wrong size.");
          } 
          if(!m_haltMessage){
            m_pExternalBuffer->set_halt_message(message);
            m_haltMessage=message;
          }
          continue;
        }
        if(dst.isExternal){
          sendToExternalConnection=true;
          continue;
        }
        
        threshRng= threshRng*1664525+1013904223UL;
        if(threshRng < threshDelay){
          m_delayed.push_back(delayed_message_t{
            idSend,
            &out,
            message,
            m_epoch
          });
          anyReady=true;
          m_statsDelays++;
          continue;
        }
        
        do_recv(out, message, idSend, m_epoch);

        anyReady = anyReady || dst.anyReady();
      }

      if(sendToExternalConnection){
        assert(m_pExternalBuffer);
        // At least one external device needs to receive this message
        auto payload=std::vector<uint8_t>( message.payloadPtr(), message.payloadPtr()+message.payloadSize() );
        m_pExternalBuffer->post_message(makeEndpoint(poets_device_address_t{index}, poets_pin_index_t{sel}), payload, sendIndex ? *sendIndex : UINT_MAX);
      }
    }
    ++m_epoch;
    return sent || anyReady || !m_delayed.empty();
  }


};


void usage()
{
  fprintf(stderr, "epoch_sim [options] sourceFile? [--external spec [args]*]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  --log-level n\n");
  fprintf(stderr, "  --max-steps n\n");
  fprintf(stderr, "  --stats-delta n : How often to print statistics about steps\n");
  fprintf(stderr, "  --max-contiguous-idle-steps n : Maximum number of steps without any messages before aborting.\n");
  fprintf(stderr, "  --snapshots interval destFile\n");
  fprintf(stderr, "  --log-events destFile\n");
  fprintf(stderr, "  --prob-send probability\n");
  fprintf(stderr, "  --prob-delay probability\n");
  fprintf(stderr, "  --rng-seed seed\n");
  fprintf(stderr, "  --accurate-assertions : Capture device state before send/recv in case of assertions.\n");
  fprintf(stderr, "  --message-init n: 0 (default) - Zero initialise all messages, 1 - All messages are randomly inisitalised, 2 - Randomly zero or random inisitalise\n");
  fprintf(stderr, "  --external spec [args]* : External spec, plus any args. Must be the last option\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "External spec could be:\n");
  fprintf(stderr, "  PROVIDER - Take the default in-proc external from the provider.\n");
  fprintf(stderr, "  INPROC:<PATH> - Load the shared object and instantiate InProcessBinaryUpstreamConnection.\n");
  fprintf(stderr, "\n");
  fprintf(stderr,"   For directly instantiated providers the startup arguments can be");
  fprintf(stderr,"   provided by arguments following the external spec.\n");
  exit(1);
}

std::shared_ptr<LogWriter> g_pLog; // for flushing purposes on exit
std::unique_ptr<SnapshotWriter> snapshotWriter;

void close_resources()
{
  if(g_pLog){
    g_pLog->close();
    g_pLog=0;
  }
  if(snapshotWriter){
    snapshotWriter.reset();
  }
}

void atexit_close_resources()
{
  close_resources();
}

void onsignal_close_resources (int)
{
  close_resources();
  exit(1);
}

int main(int argc, char *argv[])
{



  try{
    DisableDenormals();

    std::string srcFilePath="";

    std::string snapshotSinkName;
    unsigned snapshotDelta=0;

    std::string logSinkName;

    std::string checkpointName;

    unsigned statsDelta=10;

    int maxSteps=INT_MAX;
    int max_contiguous_idle_steps=10;

    bool expectIdleExit=false;

    //double probSend=0.9;
    double probSend=1.0;
    double probDelay=0.0;
    std::mt19937_64 rng;

    bool enableAccurateAssertions=false;

    std::string externalSpec;
    std::vector<std::string> externalArgs;

    int ia=1;
    while(ia < argc){
      if(!strcmp("--help",argv[ia])){
	      usage();
      }else if(!strcmp("--log-level",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --log-level\n");
          usage();
        }
        logLevel=strtoul(argv[ia+1], 0, 0);
        ia+=2;
      }else if(!strcmp("--max-steps",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --max-steps\n");
          usage();
        }
        maxSteps=strtoul(argv[ia+1], 0, 0);
        ia+=2;
      }else if(!strcmp("--max-contiguous-idle-steps",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --max-contiguous-idle-steps\n");
          usage();
        }
        max_contiguous_idle_steps=strtoul(argv[ia+1], 0, 0);
        ia+=2;
      }else if(!strcmp("--stats-delta",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --stats-delta\n");
          usage();
        }
        statsDelta=strtoul(argv[ia+1], 0, 0);
        ia+=2;
      }else if(!strcmp("--prob-send",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --prob-send\n");
          usage();
        }
        probSend=strtod(argv[ia+1], 0);
        if(probSend < ldexp(2,-10) || probSend>1){
          fprintf(stderr, "Probability of sending must be in range [2^-10,1] ");
          usage();
        }
        ia+=2;
      }else if(!strcmp("--prob-delay",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --prob-delay\n");
          usage();
        }
        probDelay=strtod(argv[ia+1], 0);
        if(probDelay < 0 || probDelay>=(1-ldexp(2,-10))){
          fprintf(stderr, "Probability of sending must be in range [0,1-2^-10) ");
          usage();
        }
        ia+=2;
      }else if(!strcmp("--rng-seed",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --rng-seed\n");
          usage();
        }
        unsigned seed=strtoull(argv[ia+1], nullptr, 0);
        rng.seed(seed);
        ia+=2;
      }else if(!strcmp("--snapshots",argv[ia])){
        if(ia+2 >= argc){
          fprintf(stderr, "Missing two arguments to --snapshots interval destination \n");
          usage();
        }
        snapshotDelta=strtoul(argv[ia+1], 0, 0);
        snapshotSinkName=argv[ia+2];
        ia+=3;
      }else if(!strcmp("--log-events",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing two arguments to --log-events destination \n");
          usage();
        }
        logSinkName=argv[ia+1];
        ia+=2;
      }else if(!strcmp("--accurate-assertions",argv[ia])){
        enableAccurateAssertions=true;
        ia+=1;
      }else if(!strcmp("--message-init",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --message-init\n");
          usage();
        }
        messageInit=strtoul(argv[ia+1], 0, 0);
        ia+=2;
        if (messageInit > 2) {
          fprintf(stderr, "Argument for --message-init is too large. Defaulting to 0\n");
        }
      }else if(!strcmp("--expect-idle-exit",argv[ia])){
        expectIdleExit=true;
        ia+=1;
      }else if(!strcmp("--external",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing specification to --external\n");
          usage();
        }
        externalSpec=argv[ia+1];
        ia+=2;
        // Chomp all the remaining args
        while(ia<argc){
          externalArgs.push_back(argv[ia]);
          ia++;
        }
      }else if(argv[ia][0]=='-'){
        fprintf(stderr, "Didn't understand option '%s'.\n", argv[ia]);
        usage();
      
      }else{
        if(!srcFilePath.empty()){
          fprintf(stderr, "Received two input paths.\n");
          usage();
        }
        srcFilePath=argv[ia];
        ia++;
      }
    }

    RegistryImpl registry;

    xmlpp::DomParser parser;

    filepath srcPath(current_path());

    if(srcFilePath.empty()){
      srcFilePath="-";
    }
    if(srcFilePath!="-"){
      filepath p(srcFilePath);
      p=absolute(p);
      if(logLevel>1){
        fprintf(stderr,"Parsing XML from '%s' ( = '%s' absolute)\n", srcFilePath.c_str(), p.c_str());
      }
      srcPath=p.parent_path();
      parser.parse_file(p.c_str());
    }else{
      if(logLevel>1){
        fprintf(stderr, "Parsing XML from stdin (this will fail if it is compressed\n");
      }
      parser.parse_stream(std::cin);
    }
    if(logLevel>1){
      fprintf(stderr, "Parsed XML\n");
    }

    atexit(atexit_close_resources);
    signal(SIGABRT, onsignal_close_resources);
    signal(SIGINT, onsignal_close_resources);

    EpochSim graph;

    if(!externalSpec.empty()){
      graph.m_pExternalBuffer=std::make_shared<InProcMessageBuffer>();
    }

    if(!logSinkName.empty()){
      graph.m_log.reset(new LogWriterToFile(logSinkName.c_str()));
      g_pLog=graph.m_log;
    }

    loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), &graph);
    if(logLevel>1){
      fprintf(stderr, "Loaded\n");
    }

    if(snapshotDelta!=0){
      snapshotWriter.reset(new SnapshotWriterToFile(snapshotSinkName.c_str()));
    }

    if(graph.m_pExternalBuffer){
      graph.m_pExternalBuffer->m_idToAddress=[&](const std::string &id) -> poets_device_address_t 
      {
        auto iid=graph.intern(id);
        auto it=graph.m_deviceIdToIndex.find(iid);
        if(it==graph.m_deviceIdToIndex.end()){
          throw std::runtime_error("Unknown device/external instance id "+id);
        }
        return poets_device_address_t{it->second};
      };
      graph.m_pExternalBuffer->m_addressToId=[&](poets_device_address_t address) -> std::string
      {
        if(address.value>=graph.m_devices.size()){
          throw std::runtime_error("Device address is invalid.");
        }
        return graph.m_devices.at(address.value).id;
      };
      graph.m_pExternalBuffer->m_endpointToFanout =[&](poets_endpoint_address_t address, std::vector<poets_endpoint_address_t> &fanout) -> void
      {
        fanout.resize(0);
        if(getEndpointDevice(address).value>=graph.m_devices.size()){
          throw std::runtime_error("Device address is invalid.");
        }
        auto &dev=graph.m_devices.at(getEndpointDevice(address).value);
        if(getEndpointPin(address).value >= dev.outputCount){
          throw std::runtime_error("Device pin index is invalid.");
        }
        auto &f = dev.outputs[getEndpointPin(address).value];
        
        if(dev.type->getOutput(getEndpointPin(address).value)->isIndexedSend()){
          fanout.resize(f.size());
          for(unsigned i=0; i<f.size(); i++){
            const auto &d=f[i];
            if(graph.m_devices[d.dstDevice].isExternal && (d.dstDevice!=(unsigned)graph.m_haltDeviceIndex)){
              fanout[i]=makeEndpoint(poets_device_address_t{d.dstDevice}, poets_pin_index_t{d.dstPinIndex});
            }else{
              fanout[i]=poets_endpoint_address_t();
            }
          }
        }else{
          fanout.resize(0);
          fanout.reserve(f.size());
          for(unsigned i=0; i<f.size(); i++){ 
            const auto &d=f[i];
            if(graph.m_devices[d.dstDevice].isExternal && (d.dstDevice!=(unsigned)graph.m_haltDeviceIndex)){
              fanout.push_back(makeEndpoint(poets_device_address_t{d.dstDevice}, poets_pin_index_t{d.dstPinIndex}));
            }
          }
        }
      };

    }

    std::thread inProcExternalThread;

    if(!externalSpec.empty()){
      in_proc_external_main_t pProc=nullptr;

      if(externalSpec.substr(0, 7)=="INPROC:"){
        std::string so_path=externalSpec.substr(7);
        fprintf(stderr, "Loading inproc external from %s\n", so_path.c_str());
        void *hExternalObj=dlopen(so_path.c_str(), RTLD_NOW);
        if(!hExternalObj){
          throw std::runtime_error("Couldn't open shared object file "+so_path);
        }

        pProc=(in_proc_external_main_t)dlsym(hExternalObj, "poets_in_proc_external_main");
        if(!pProc){
          throw std::runtime_error("Couldn't find symbol 'poets_in_proc_external_main' in shared object.");
        }
        fprintf(stderr, "Found non-null inproc external in %s at %p\n", so_path.c_str(), pProc);
      }else if(externalSpec=="PROVIDER"){
        fprintf(stderr, "Loading default inproc external from provider\n");
        pProc=graph.m_graphType->getDefaultInProcExternalProcedure();
        if(pProc==nullptr){
          throw std::runtime_error("Provider did not contain an in-proc external (is it dynamic? You might need to compiler the provider.)");
        }
      }else{
        throw std::runtime_error("Didn't understand external spec: "+externalSpec);
      }

      bool connected=false;

      graph.m_pExternalBuffer->m_connect = [&](const std::string &graphType, const std::string &graphInst, const std::vector<std::pair<std::string,std::string> > &owned)
      {
        fprintf(stderr, "Recevied call to connect from inproc external.\n");
        if(!std::regex_match(graph.m_graphType->getId(), std::regex(graphType))){
          throw std::runtime_error("Graph type of "+graph.m_graphType->getId()+" does not match externals type of "+graphType);
        }
        if(!std::regex_match(graph.m_id, std::regex(graphInst))){
          throw std::runtime_error("Graph instance of "+graph.m_graphType->getId()+" does not match externals type of "+graphType);
        }

        fprintf(stderr, "Checking owned externals and types from inproc external connection against graph instance.\n");
        std::unordered_set<unsigned> foundIndices;
        for(auto i_v : owned){
          if(i_v.first=="__halt__"){
            throw std::runtime_error("Client connection tried to take ownership of the __halt__ device.");
          }
          auto ii=graph.intern(i_v.first);
          auto it=graph.m_deviceIdToIndex.find(ii);
          if(it==graph.m_deviceIdToIndex.end()){
            throw std::runtime_error("Attempt to bind to unknown external instance "+i_v.first);
          }
          auto &d = graph.m_devices.at(it->second);
          if(!d.isExternal){
            throw std::runtime_error("Attempt to bind to non-external device "+i_v.first);
          }
          if(!i_v.second.empty()){
            if(!std::regex_match( d.type->getId(), std::regex(i_v.second))){
              throw std::runtime_error("Attempt to bind instance "+i_v.first+" to device matching '"+i_v.second+"' but real type is '+"+ d.type->getId() +"'.");
            }
          }
          foundIndices.insert(d.index);
        }
        if(foundIndices != graph.m_externalIndices){
          // TODO: is this really an error?
          throw std::runtime_error("Inproc external did not bind to the complete set of externals.");
        }

        connected=true;
        graph.m_pExternalBuffer->m_cond.notify_one(); // We are still under lock here
      };
      
      graph.m_pExternalBuffer->m_getGraphProperties=[&](size_t cbBuffer, void *pBuffer) -> size_t
      {
        if(cbBuffer < graph.m_graphProperties.payloadSize()){
          throw std::runtime_error("Buffer is too small");
        }
        memcpy(pBuffer, graph.m_graphProperties.payloadPtr(), graph.m_graphProperties.payloadSize());
        return graph.m_graphProperties.payloadSize();
      };

      graph.m_pExternalBuffer->m_getDeviceProperties=[&](poets_device_address_t address, size_t cbBuffer, void *pBuffer) -> size_t
      {
        auto &dev=graph.m_devices.at(address.value);
        if(cbBuffer < dev.properties.payloadSize()){
          throw std::runtime_error("Buffer is too small");
        }
        memcpy(pBuffer, dev.properties.payloadPtr(), dev.properties.payloadSize());
        return dev.properties.payloadSize();
      };

      inProcExternalThread=std::thread([&](){
        std::vector<const char *> fargv;
        fargv.push_back( argv[0] );
        for(auto &x : externalArgs){
          fargv.push_back( x.c_str() );
        }
        pProc(*graph.m_pExternalBuffer, fargv.size(), &fargv[0]);
      });

      fprintf(stderr, "Waiting for inproc external to call 'connect'\n");
      {
        std::unique_lock<std::mutex> lk(graph.m_pExternalBuffer->m_mutex);
        graph.m_pExternalBuffer->m_cond.wait(lk, [&]{ return connected; });
      }
      fprintf(stderr, "Inproc external is connected.\n");
    }

    graph.init();

    if(snapshotWriter){
      graph.writeSnapshot(snapshotWriter.get(), 0.0, 0);
    }
    int nextStats=0;
    int nextSnapshot=snapshotDelta ? snapshotDelta-1 : -1;
    int snapshotSequenceNum=1;
    unsigned contiguous_hardware_idle_steps=0;

    bool capturePreEventState=enableAccurateAssertions || !checkpointName.empty();

    for(int i=0; i<maxSteps; i++){
      if(graph.m_haltMessage){
        break;
      }

      bool running =  !graph.m_deviceExitCalled && graph.step(rng, probSend, capturePreEventState, probDelay);

      if(logLevel>2 || i==nextStats){
        fprintf(stderr, "Epoch %u : sends/device/epoch = %f (%g / %u), delayedMsgs/epoch = %g,  meanShear = %g, maxShear = %g\n\n", i,
          graph.m_statsSends / graph.m_devices.size() / statsDelta, graph.m_statsSends/statsDelta, (unsigned)graph.m_devices.size(),
          graph.m_statsDelays / (double)statsDelta,
          graph.m_statsShearSum / graph.m_statsShearCount,
          graph.m_statsShearMax
        );
      }
      if(i==nextStats){
        nextStats=nextStats+statsDelta;
        graph.m_statsSends=0;
        graph.m_statsDelays=0;
        graph.m_statsShearCount=0;
        graph.m_statsShearSum=0;
        graph.m_statsShearSumSqr=0;
        graph.m_statsShearMax=0;
      }

      if(snapshotWriter && i==nextSnapshot){
        graph.writeSnapshot(snapshotWriter.get(), i, snapshotSequenceNum);
        nextSnapshot += snapshotDelta;
        snapshotSequenceNum++;
      }

      if(graph.m_deviceExitCalled){
        exit(graph.m_deviceExitCode);
      }

      if(running){
        contiguous_hardware_idle_steps=0;
      }else{
        if((int)contiguous_hardware_idle_steps<max_contiguous_idle_steps){
          graph.do_hardware_idle(); 
          contiguous_hardware_idle_steps++;
        }else{
          if(graph.m_pExternalBuffer){
            if(logLevel>1){
              fprintf(stderr, "Simulator has done %u contiguous hardware idle steps. Blocking until the external connection sends a message.\n", contiguous_hardware_idle_steps);
            }
            graph.m_pExternalBuffer->wait_for_client_to_send();
            contiguous_hardware_idle_steps=0;
          }else{
            if(logLevel>1){
              fprintf(stderr, "Simulator has done %u contiguous hardware idle steps. Quiting as the graph seems to have deadlocked.\n", contiguous_hardware_idle_steps);
            }
            break; // finished
          }
        }
      }
    }

    if(logLevel>1){
      fprintf(stderr, "Done\n");
    }

    if(inProcExternalThread.joinable()){
      fprintf(stderr, "Joining with in-proc external thread.\n");
      inProcExternalThread.join();
      fprintf(stderr, "Join complete.\n");
    }

    close_resources();

    if(graph.m_haltMessage){
      auto m=(const halt_message_type*)graph.m_haltMessage.payloadPtr();
      fprintf(stderr, " exiting with code %u.\n", m->code);
      exit(m->code);
    }else{
      if(expectIdleExit){
        exit(0);
      }else{
        fprintf(stderr, "Simulator exited due to idle devices, but this was not expected (use --expect-idle-exit to indicate this).\n");
        exit(1); // This is not a successful exit
      }
    }
  }catch(std::exception &e){
    close_resources();
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    close_resources();
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
