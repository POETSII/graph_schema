#include "graph.hpp"

#include "external_connection.hpp"
#include "external_device_proxy.hpp"

#include <libxml++/parsers/domparser.h>
#include <libxml++/document.h>

#include <unordered_map>
#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <unordered_set>
#include <algorithm>
#include <signal.h>

#include <cstring>
#include <cstdlib>

static unsigned logLevel=2;
static unsigned messageInit;

struct external_edge_properties_t
  : typed_data_t
{
  unsigned dstDev;
  unsigned dstPort;
  unsigned srcDev;
  unsigned srcPort;
};

TypedDataPtr create_external_edge_properties(unsigned dstDev, unsigned dstPort, unsigned srcDev, unsigned srcPort )
{
  auto res=make_data_ptr<external_edge_properties_t>();
  res->dstDev=dstDev;
  res->dstPort=dstPort;
  res->srcDev=srcDev;
  res->srcPort=srcPort;
  return res;
}


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
    DeviceTypePtr type;
    TypedDataPtr properties;
    TypedDataPtr state;
    unsigned keyValueSeq; // Sequence number for key value

    unsigned outputCount;
    uint32_t readyToSend;

    std::vector<const char *> outputNames; // interned names

    std::vector<std::vector<output> > outputs;
    std::vector<std::vector<input> > inputs;

    std::pair<MessageTypePtr, TypedDataPtr> prev_message;

    // Only used for externals
    std::queue<external_message_t> externalSendQueue;

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

  std::function<void (const char*,uint32_t,uint32_t)> m_onExportKeyValue;

  std::function<void (const char*,bool,int,const char *)> m_onCheckpoint;

  int m_checkpointLevel=0;
  std::vector<std::pair<bool,std::string> > m_checkpointKeys;

  bool m_deviceExitCalled=false;
  bool m_deviceExitCode=0;
  std::function<void (const char*,int)> m_onDeviceExit;

  uint64_t m_unq;

  std::shared_ptr<ExternalConnection> m_pExternalConnection;

  DeviceTypePtr createExternalInterceptor(DeviceTypePtr dt, unsigned index)
  {
    for(auto op : dt->getOutputs()){
      if(op->isIndexedSend()){
        throw std::runtime_error("Currently external devices with indexed send outputs are not supported.");
      }
    }

    // Create an interceptor device which will send to the external connection
    auto onSend=[this](OrchestratorServices *orchestrator, const typed_data_t *graphProperties,
      const typed_data_t *deviceProperties, unsigned deviceAddress,
      unsigned sendPortIndex,
      typed_data_t *message, bool *doSend, unsigned *sendIndex
    ){
      auto &dev=this->m_devices.at(deviceAddress);
      assert(dev.type->isExternal());
      assert(!dev.externalSendQueue.empty());
      external_message_t msg=dev.externalSendQueue.front();
      dev.externalSendQueue.pop();

      assert(msg.srcDev==deviceAddress);
      assert(msg.srcPort==sendPortIndex);
      assert(msg.isMulticast); // We can only take multi-cast from this route, as it is treated like any other send
      msg.data.copy_to(message);
      *doSend=true;

      // Currently indexed sends involving externals are not supported
      assert(sendIndex==0);
    };

    auto onRTS=[this](OrchestratorServices *orchestrator, const typed_data_t *graphProperties,
      const typed_data_t *deviceProperties, unsigned deviceAddress
    ) -> uint32_t 
    {
      auto &dev=this->m_devices.at(deviceAddress);
      assert(dev.type->isExternal());
      if(dev.externalSendQueue.empty()){
        return 0;
      }
      const auto &msg=dev.externalSendQueue.front();
      assert(msg.srcDev==deviceAddress);
      assert(msg.srcPort < 32);
      return 1ul<<msg.srcPort;
    };

    auto onRecv=[this](OrchestratorServices *orchestrator, const typed_data_t *graphProperties,
      const typed_data_t *deviceProperties, unsigned deviceAddress,
      const typed_data_t *edgeProperties, unsigned portIndex,
      const typed_data_t *message
    ){
      auto pEdgeInfo=(const external_edge_properties_t *)edgeProperties;

      external_message_t msg={
        false, // not multi-cast
        pEdgeInfo->dstDev, pEdgeInfo->dstPort,
        pEdgeInfo->srcDev, pEdgeInfo->srcPort,
        clone(message)
      };
      m_pExternalConnection->write(msg);
    };

    return std::make_shared<ExternalDeviceImpl>(dt,index, onSend,onRecv,onRTS);
  }

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
    // TypedDataPtr state=dt->getStateSpec()->create();
    device d;
    d.index=m_devices.size();
    d.id=id;
    d.name=intern(id);
    if(dt->isExternal()){
      d.type=createExternalInterceptor(dt,d.index);
    }else{
      d.type=dt;
    }

    d.properties=deviceProperties;
    d.state=deviceState;
    d.keyValueSeq=0;
    d.readyToSend=0;
    d.outputCount=dt->getOutputCount();
    d.outputs.resize(dt->getOutputCount());
    for(unsigned i=0;i<dt->getOutputCount();i++){
      d.outputNames.push_back(intern(dt->getOutput(i)->getName()));
    }
    d.inputs.resize(dt->getInputCount());
    m_devices.push_back(d);
    m_deviceIdToIndex[d.name]=d.index;

    return d.index;
  }

  void onEdgeInstance(uint64_t gId, uint64_t dstDevIndex, const DeviceTypePtr &dstDevType, const InputPinPtr &dstInput, uint64_t srcDevIndex, const DeviceTypePtr &srcDevType, const OutputPinPtr &srcOutput, int sendIndex, const TypedDataPtr &properties, rapidjson::Document &&) override
  {

    // In principle we support external->external connections!
    // They just get routed through. Why would this happen though?

    // For an external's input we need to create fake properties
    TypedDataPtr props(properties);
    if(dstDevType->isExternal()){
      // Note that this destroys the original edge properties. However, we are
      // not the external, so we just don't care! Only the external can do
      // something meaninful with them.
      props=create_external_edge_properties(dstDevIndex, dstInput->getIndex(), srcDevIndex, srcOutput->getIndex() );
    }

    input i;
    i.properties=props;
    i.state=dstInput->getStateSpec()->create();
    i.firings=0;
    i.id=intern( m_devices.at(dstDevIndex).id + ":" + dstInput->getName() + "-" + m_devices.at(srcDevIndex).id+":"+srcOutput->getName() );
    auto &slots=m_devices.at(dstDevIndex).inputs.at(dstInput->getIndex());
    unsigned dstPinSlot=slots.size();
    slots.push_back(i);

    // This could be an external's output, but we don't deal with it here

    output o;
    o.dstDevice=dstDevIndex;
    o.dstPinIndex=dstInput->getIndex();
    o.dstPinSlot=dstPinSlot;
    o.dstDeviceId=m_devices.at(dstDevIndex).name;
    o.dstInputName=intern(dstInput->getName());
    o.sendIndex=sendIndex;
    m_devices.at(srcDevIndex).outputs.at(srcOutput->getIndex()).push_back(o);

    if(dstDevType->isExternal() || srcDevType->isExternal())
    {
      m_pExternalConnection->onExternalEdgeInstance(
        m_devices[dstDevIndex].name, dstDevIndex, dstDevType, dstInput,
        m_devices[srcDevIndex].name, srcDevIndex, srcDevType, srcOutput
      );
    }
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
            for(unsigned i=0; i<ov.size(); i++){
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
    m_onExportKeyValue=[&](const char *id, uint32_t key, uint32_t value) -> void
    {
      auto it=m_deviceIdToIndex.find(id);
      if(it==m_deviceIdToIndex.end()){
        fprintf(stderr, "ERROR : Attempt export key value on non-existent (or non interned) id '%s'\n", id);
        exit(1);
      }
      unsigned index=it->second;
      unsigned seq=m_devices[index].keyValueSeq++;

      m_keyValueEvents.emplace_back(id, seq, key, value);
    };

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
      ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, dev.name, "Init handler", m_onExportKeyValue, m_onDeviceExit, m_onCheckpoint  };
      dev.type->init(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get());
      // TODO: Allow for "__init__" backwards compatability
      // auto init=dev.type->getInput("__init__");
      // if(init){
      //   if(logLevel>2){
      //     fprintf(stderr, "  init device %d = %s\n", dev.index, dev.id.c_str());
      //   }

      //   init->onReceive(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get(), 0, 0, 0);
      // }
      dev.readyToSend = dev.type->calcReadyToSend(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get());

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

    fprintf(stderr, "onHardwareIdle\n");
    for(auto &d : m_devices){
      if(!d.type->isExternal()){
        ReceiveOrchestratorServicesImpl services{logLevel, stderr, d.name, "Idle handler", m_onExportKeyValue, m_onDeviceExit, m_onCheckpoint  };
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
      }
    }
  }

  template<class TRng>
  bool step(TRng &rng, double probSend,bool capturePreEventState)
  {
    // Drain the external connection
    // TODO: Is this too eager?
    while(m_pExternalConnection->canRead()){
      external_message_t msg;
      m_pExternalConnection->read(msg);

      auto &dev=m_devices.at(msg.srcDev);
      if(!dev.type->isExternal()){
        throw std::runtime_error("Received message from external connection that wasn't for a valid external device.");
      }
      if(dev.type->getOutputCount() <= msg.srcPort){
        throw std::runtime_error("Received message from external connection for non-existent output port on external.");
      }
      if(!msg.isMulticast){
        throw std::runtime_error("Received message from external connection that is not multi-cast (current limitation of epoch_sim).");
      }

      dev.externalSendQueue.push(msg);
      dev.readyToSend=1ul<<msg.srcPort;
    }

    // Within each step every object gets the chance to send a message with probability probSend
    std::uniform_real_distribution<> udist;

    ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, 0, 0, m_onExportKeyValue, m_onDeviceExit, m_onCheckpoint};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Recv: ";
      receiveServices.setPrefix(tmp.str().c_str());
    }

    SendOrchestratorServicesImpl sendServices{logLevel, stderr, 0, 0, m_onExportKeyValue, m_onDeviceExit, m_onCheckpoint};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Send: ";
      sendServices.setPrefix(tmp.str().c_str());
    }

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
        uint32_t check=src.type->calcReadyToSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get());
        assert(check);
        assert( (check>>sel) & 1);
        #endif

        if(output->isIndexedSend()){
          sendIndex=&sendIndexStg;
        }

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
      
      for(auto &out : *pOutputVec){
        
        auto &dst=m_devices[out.dstDevice];
        auto &in=dst.inputs[out.dstPinIndex];
        auto &slot=in[out.dstPinSlot];

        slot.firings++;

        if(logLevel>3){
          fprintf(stderr, "    sending to device %d = %s\n", dst.index, dst.id.c_str());
        }

        const auto &pin=dst.type->getInput(out.dstPinIndex);

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

        anyReady = anyReady || dst.anyReady();
      }
    }
    ++m_epoch;
    return sent || anyReady;
  }


};


void usage()
{
  fprintf(stderr, "epoch_sim [options] sourceFile?\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  --log-level n\n");
  fprintf(stderr, "  --max-steps n\n");
  fprintf(stderr, "  --snapshots interval destFile\n");
  fprintf(stderr, "  --log-events destFile\n");
  fprintf(stderr, "  --prob-send probability\n");
  fprintf(stderr, "  --key-value destFile\n");
  fprintf(stderr, "  --accurate-assertions : Capture device state before send/recv in case of assertions.\n");
  fprintf(stderr, "  --message-init n: 0 (default) - Zero initialise all messages, 1 - All messages are randomly inisitalised, 2 - Randomly zero or random inisitalise\n");
  exit(1);
}

std::shared_ptr<LogWriter> g_pLog; // for flushing purposes on exit

void close_resources()
{
  if(g_pLog){
    g_pLog->close();
    g_pLog=0;
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

    std::string srcFilePath="-";

    std::string snapshotSinkName;
    unsigned snapshotDelta=0;

    std::string externalInSpec="";
    std::string externalOutSpec="-";

    std::string logSinkName;

    std::string checkpointName;

    unsigned statsDelta=1;

    int maxSteps=INT_MAX;

    //double probSend=0.9;
    double probSend=1.0;

    bool enableAccurateAssertions=false;

    std::string keyValueName;

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
      }else if(!strcmp("--key-value",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --key-value\n");
          usage();
        }
        keyValueName=argv[ia+1];
        ia+=2;
      }else if(!strcmp("--external-in",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --external_in\n");
          usage();
        }
        externalInSpec=argv[ia+1];
        ia+=2;
      }else if(!strcmp("--external-out",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --external-out\n");
          usage();
        }
        externalOutSpec=argv[ia+1];
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
      }else{
        srcFilePath=argv[ia];
        ia++;
      }
    }

    FILE *externalInFile=0;
    if(externalInSpec!=""){
      if(externalInSpec=="-"){
        externalInFile=stdin;
      }else{
        externalInFile=fopen(externalInSpec.c_str(),"rb");
        if(externalInFile==0){
          fprintf(stderr, "COuldn't open file '%s' for reading as external in.\n", externalInSpec.c_str());
          exit(1);
        }
      }
    }

    FILE *externalOutFile=stdout;
    if(externalOutSpec!="-"){
      if(externalOutSpec==""){
        externalOutFile=0;
      }else{
        externalOutFile=fopen(externalInSpec.c_str(),"wb");
        if(externalOutFile==0){
          fprintf(stderr, "COuldn't open file '%s' for writing as external out.\n", externalOutSpec.c_str());
          exit(1);
        }
      }
    }

    RegistryImpl registry;

    xmlpp::DomParser parser;

    filepath srcPath(current_path());

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

    graph.m_pExternalConnection=std::make_shared<JSONExternalConnection>(externalInFile, externalOutFile);

    if(!logSinkName.empty()){
      graph.m_log.reset(new LogWriterToFile(logSinkName.c_str()));
      g_pLog=graph.m_log;
    }

    FILE *keyValueDst=0;
    if(!keyValueName.empty()){
        keyValueDst=fopen(keyValueName.c_str(), "wt");
        if(keyValueDst==0){
            fprintf(stderr, "Couldn't open key value dest '%s'\n", keyValueName.c_str());
            exit(1);
        }
    }

    loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), &graph);
    if(logLevel>1){
      fprintf(stderr, "Loaded\n");
    }

    std::unique_ptr<SnapshotWriter> snapshotWriter;
    if(snapshotDelta!=0){
      snapshotWriter.reset(new SnapshotWriterToFile(snapshotSinkName.c_str()));
    }

    std::mt19937 rng;

    graph.init();

    graph.m_pExternalConnection->startPump();

    if(snapshotWriter){
      graph.writeSnapshot(snapshotWriter.get(), 0.0, 0);
    }
    int nextStats=0;
    int nextSnapshot=snapshotDelta ? snapshotDelta-1 : -1;
    int snapshotSequenceNum=1;
    unsigned contiguous_hardware_idle_steps=0;

    bool capturePreEventState=enableAccurateAssertions || !checkpointName.empty();

    for(int i=0; i<maxSteps; i++){
      bool running = graph.step(rng, probSend, capturePreEventState) && !graph.m_deviceExitCalled;

      if(logLevel>2 || i==nextStats){
        fprintf(stderr, "Epoch %u : sends/device/epoch = %f (%f / %u)\n\n", i, graph.m_statsSends / graph.m_devices.size() / statsDelta, graph.m_statsSends/statsDelta, (unsigned)graph.m_devices.size());
      }
      if(i==nextStats){
        nextStats=nextStats+statsDelta;
        graph.m_statsSends=0;
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
        if(contiguous_hardware_idle_steps<10){
          graph.do_hardware_idle(); 
          contiguous_hardware_idle_steps++;
        }else if(graph.m_pExternalConnection->isReadOpen() ){
          if(logLevel>1){
            fprintf(stderr, "  Internal events have finished, but external read connection is open.\n");
          }
          while( graph.m_pExternalConnection->isReadOpen() & !graph.m_pExternalConnection->canRead()) {
            usleep(1000);// TODO: horrible
          }
          if(graph.m_pExternalConnection->canRead()){
            if(logLevel>1){
              fprintf(stderr, "  New event from external unblocked us.\n");
            }
            continue;
          }else{
            if(logLevel>1){
              fprintf(stderr, "  External connection closed.\n");
            }
            break;
          }
        }else{
          break; // finished
        }
      }
    }

    if(logLevel>1){
      fprintf(stderr, "Done\n");
    }

    if(keyValueDst){
        if(logLevel>2){
            fprintf(stderr, "Writing key value file\n");
        }
        std::sort(graph.m_keyValueEvents.begin(), graph.m_keyValueEvents.end());
        for(auto t : graph.m_keyValueEvents){
            fprintf(keyValueDst, "%s, %u, %u, %u\n", std::get<0>(t), std::get<1>(t), std::get<2>(t), std::get<3>(t));
        }
        fflush(keyValueDst);
    }

    close_resources();
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
