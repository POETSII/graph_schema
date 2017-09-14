#include "graph.hpp"

#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <unordered_set>
#include <algorithm>
#include <signal.h>

#include <cstring>
#include <cstdlib>

static unsigned  logLevel=2;

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

  virtual uint64_t onDeviceInstance(uint64_t gId, const DeviceTypePtr &dt, const std::string &id, const TypedDataPtr &deviceProperties, rapidjson::Document &&) override
  {
    TypedDataPtr state=dt->getStateSpec()->create();
    device d;
    d.index=m_devices.size();
    d.id=id;
    d.name=intern(id);
    d.type=dt;
    d.properties=deviceProperties;
    d.state=state;
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

  void onEdgeInstance(uint64_t gId, uint64_t dstDevIndex, const DeviceTypePtr &dstDevType, const InputPinPtr &dstInput, uint64_t srcDevIndex, const DeviceTypePtr &srcDevType, const OutputPinPtr &srcOutput, const TypedDataPtr &properties, rapidjson::Document &&) override
  {
    input i;
    i.properties=properties;
    i.state=dstInput->getStateSpec()->create();
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
    m_devices.at(srcDevIndex).outputs.at(srcOutput->getIndex()).push_back(o);

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
      ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, dev.name, "__init__", m_onExportKeyValue, m_onDeviceExit, m_onCheckpoint  };
      auto init=dev.type->getInput("__init__");
      if(init){
        if(logLevel>2){
          fprintf(stderr, "  init device %d = %s\n", dev.index, dev.id.c_str());
        }

        init->onReceive(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get(), 0, 0, 0);
      }
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

  template<class TRng>
  bool step(TRng &rng, double probSend,bool capturePreEventState)
  {
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
    uint32_t threshSend=(uint32_t)std::max((double)0xFFFFFFFFul,std::min(0.0,threshSendDbl));
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
      TypedDataPtr message(output->getMessageType()->getMessageSpec()->create());
      
      std::string idSend;      
      
      bool doSend=true;
      {
        #ifndef NDEBUG
        uint32_t check=src.type->calcReadyToSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get());
        assert(check);
        assert( (check>>sel) & 1);
        #endif

        if(capturePreEventState){
          prevState=src.state.clone();
        }
        sendServices.setDevice(src.name, src.outputNames[sel]);
        try{
          // Do the actual send handler
          output->onSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get(), message.get(), &doSend);
        }catch(provider_assertion_error &e){
          fprintf(stderr, "Caught handler exception during send. devId=%s, devType=%s, outPin=%s.", src.name, src.type->getId().c_str(), output->getName().c_str());
          fprintf(stderr, "  %s\n", e.what());
          
          if(capturePreEventState){
            fprintf(stderr, "  preSendState = %s\n", src.type->getStateSpec()->toJSON(prevState).c_str());
          }
          fprintf(stderr, "     currState = %s\n", src.type->getStateSpec()->toJSON(src.state).c_str());
          throw;
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

      for(auto &out : src.outputs[sel]){
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
    
    std::string logSinkName;
    
    std::string checkpointName;

    unsigned statsDelta=1;

    int maxSteps=INT_MAX;

    double probSend=0.9;
    
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
      }else if(!strcmp("--accurate-assertions",argv[ia])){
        enableAccurateAssertions=true;
        ia+=1;
      }else{
        srcFilePath=argv[ia];
        ia++;
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

    if(snapshotWriter){
      graph.writeSnapshot(snapshotWriter.get(), 0.0, 0);
    }
    int nextStats=0;
    int nextSnapshot=snapshotDelta ? snapshotDelta-1 : -1;
    int snapshotSequenceNum=1;
    
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

      if(!running){
        break;
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
