#include "graph.hpp"

#define HAVE_POETS_INTERFACE_SPEC 1

#if HAVE_POETS_INTERFACE_SPEC
#include "poets_protocol/DownstreamConnection.hpp"
#include "poets_protocol/DownstreamConnectionEventsBase.hpp"
#endif

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

    bool isExternal;

    std::vector<const char *> outputNames; // interned names

    std::vector<std::vector<output> > outputs;
    std::vector<std::vector<input> > inputs;

    std::pair<MessageTypePtr, TypedDataPtr> prev_message;

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

  uint64_t m_unq;
  bool m_anyReady=false;

  bool m_capturePreEventState;

#if HAVE_POETS_INTERFACE_SPEC
  std::shared_ptr<SingleDownstreamConnection> m_pDownstreamConnection;
  std::shared_ptr<DownstreamConnectionEnvironmentBase> m_pDownstreamConnectionEnv;
#endif

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
    #if HAVE_POETS_INTERFACE_SPEC
    d.isExternal=dt->isExternal();
    m_pDownstreamConnectionEnv->add_device(id, poets_device_address_t(d.index), dt);
    #else
    if(dt->isExternal()){
      throw std::runtime_error("Attempt to load graph with external, but this epoch_sim does not have external support built in.");
    }
    #endif

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
    TypedDataPtr props(properties);

    #if HAVE_POETS_INTERFACE_SPEC
    m_pDownstreamConnectionEnv->add_edge(
      makeEndpoint(poets_device_address_t{(uint32_t)srcDevIndex}, poets_pin_index_t{srcOutput->getIndex()}),
      makeEndpoint(poets_device_address_t{(uint32_t)dstDevIndex}, poets_pin_index_t{dstInput->getIndex()})
    );
    #endif

    input i;
    i.properties=props;
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
    
    for(auto &dev : m_devices){
      ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, dev.name, "__init__"};
      auto init=dev.type->getInput("__init__");
      if(init){
        if(dev.isExternal){
          throw std::runtime_error("External device has an init handler.");
        }
        if(logLevel>2){
          fprintf(stderr, "  init device %d = %s\n", dev.index, dev.id.c_str());
        }

        init->onReceive(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get(), 0, 0, 0);
      }
      if(!dev.isExternal){
        dev.readyToSend = dev.type->calcReadyToSend(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get());
      }

      if(m_log){
        // Try to avoid allocation, while gauranteeing m_checkpointKeys is left in empty() state
        auto id=nextSeqUnq();
        auto idStr=std::to_string(id);
        m_log->onInitEvent(
          idStr.c_str(),
          0.0,
          0.0,
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

  void broadcast_message(unsigned srcDev, unsigned srcPin, TypedDataPtr message, std::string idSend)
  {
    ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, 0, 0};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Recv: ";
      receiveServices.setPrefix(tmp.str().c_str());
    }

    const auto &src=m_devices.at(srcDev);

    TypedDataPtr prevState;
    for(auto &out : src.outputs[srcPin]){
        
        auto &dst=m_devices[out.dstDevice];
        auto &in=dst.inputs[out.dstPinIndex];
        auto &slot=in[out.dstPinSlot];

        slot.firings++;

        if(logLevel>3){
          fprintf(stderr, "    sending to device %d = %s\n", dst.index, dst.id.c_str());
        }

        const auto &pin=dst.type->getInput(out.dstPinIndex);

        if(m_capturePreEventState){
          prevState=dst.state.clone();
        }
        if(dst.isExternal){
          #if HAVE_POETS_INTERFACE_SPEC
          if(m_pDownstreamConnection){
            m_pDownstreamConnection->send_message(
              makeEndpoint(poets_device_address_t(src.index), poets_pin_index_t(srcPin)),
              message.payloadSize(), message.payloadPtr()
            );
          }else{
            auto msg=std::make_shared<TextPacketsMessage>();
            msg->owner=m_pDownstreamConnectionEnv->get_owner();
            msg->task=m_pDownstreamConnectionEnv->get_task();
            msg->routing_mode="SourceRouted";
            msg->count=1;
            msg->text_addresses.push_back( src.id+":"+src.type->getOutput(srcPin)->getName() );
            msg->payload_strings.push_back( src.type->getOutput(srcPin)->getMessageType()->getMessageSpec()->toJSON(message) );

            rapidjson::Document doc;
            doc.SetObject();
            msg->write(doc, doc);
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            std::cout << buffer.GetString() << std::endl;
            std::cout.flush();

          }
          #else
          throw std::runtime_error("Attempt to send on external");
          #endif
        }else{
          receiveServices.setDevice(out.dstDeviceId, out.dstInputName);
          try{
            pin->onReceive(&receiveServices, m_graphProperties.get(), dst.properties.get(), dst.state.get(), slot.properties.get(), slot.state.get(), message.get());
          }catch(provider_assertion_error &e){
            fprintf(stderr, "Caught handler exception during Receive. devId=%s, dstDevType=%s, dstPin=%s.\n", dst.name, dst.type->getId().c_str(), pin->getName().c_str());
            fprintf(stderr, "  %s\n", e.what());

            fprintf(stderr, "     message = %s\n", pin->getMessageType()->getMessageSpec()->toJSON(message).c_str());
            if(m_capturePreEventState){
              fprintf(stderr, "  preRecvState = %s\n", dst.type->getStateSpec()->toJSON(prevState).c_str());
            }
            fprintf(stderr, "     currState = %s\n", dst.type->getStateSpec()->toJSON(dst.state).c_str());

            throw;
          }
          if(!dst.isExternal){
            dst.readyToSend = dst.type->calcReadyToSend(&receiveServices, m_graphProperties.get(), dst.properties.get(), dst.state.get());
          }
        }

        if(m_log){
          auto id=nextSeqUnq();
          auto idStr=std::to_string(id);

          m_log->onRecvEvent(
            idStr.c_str(),
            m_epoch,
            0.0,
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

        m_anyReady = m_anyReady || dst.anyReady();
      }
    };

  template<class TRng>
  bool step(TRng &rng, double probSend, bool blockOnExternal)
  {
    ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, 0, 0};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Recv: ";
      receiveServices.setPrefix(tmp.str().c_str());
    }

    SendOrchestratorServicesImpl sendServices{logLevel, stderr, 0, 0};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Send: ";
      sendServices.setPrefix(tmp.str().c_str());
    }

    bool sent=false;
    m_anyReady=false;
    std::string idSend;

#if HAVE_POETS_INTERFACE_SPEC
    // Drain the external connection
    if(m_pDownstreamConnection){
      auto onReceive=[&](
        poets_endpoint_address_t source,
        unsigned cbMessage,
        const uint8_t *pMessage
      ) -> bool {
        auto &dev=m_devices.at(getEndpointDevice(source).value);
        if(!dev.isExternal){
          throw std::runtime_error("Received message from external connection that wasn't for a valid external device.");
        }
        if(dev.type->getOutputCount() <= getEndpointPin(source).value){
          throw std::runtime_error("Received message from external connection for non-existent output port on external.");
        }
        auto pin=dev.type->getOutput(getEndpointPin(source).value);

        if(m_log){
          auto id=nextSeqUnq();
          auto idStr=std::to_string(id);
          idSend=idStr;
        }
        
        TypedDataPtr message=pin->getMessageType()->getMessageSpec()->create();
        memcpy(message.payloadPtr(), pMessage, message.payloadSize());
        broadcast_message(dev.index, getEndpointPin(source).value, message, idSend);
        sent=true;

        return false;
      };

      m_pDownstreamConnection->receive_any_messages(blockOnExternal, onReceive);
    }
#endif

    // Within each step every object gets the chance to send a message with probability probSend
    std::uniform_real_distribution<> udist;

    std::vector<int> sendSel(m_devices.size());

    unsigned rotA=rng();
    for(unsigned i=0;i<m_devices.size();i++){
      auto &src=m_devices[i];

      // Pick a random message
      sendSel[i]=pick_bit(src.outputCount, src.readyToSend, rotA+i);
    }

    // If accurateAssertionInfo, then we capture full state before each send/recv
    TypedDataPtr prevState;

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
        m_anyReady=true;
        continue;
      }

      if(!((src.readyToSend>>sel)&1)){
        m_anyReady=true; // We don't know if it turned any others on
        continue;
      }

      if(logLevel>3){
        fprintf(stderr, "    output pin %d ready\n", sel);
      }

      m_statsSends++;

      const OutputPinPtr &output=src.type->getOutput(sel);
      TypedDataPtr message(getMessage(output->getMessageType()));

      bool doSend=true;
      {
        #ifndef NDEBUG
        uint32_t check=src.type->calcReadyToSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get());
        assert(check);
        assert( (check>>sel) & 1);
        #endif

        if(m_capturePreEventState){
          prevState=src.state.clone();
        }
        sendServices.setDevice(src.name, src.outputNames[sel]);
        try{
          // Do the actual send handler
          output->onSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get(), message.get(), &doSend);
        }catch(provider_assertion_error &e){
          fprintf(stderr, "Caught handler exception during send. devId=%s, devType=%s, outPin=%s.", src.name, src.type->getId().c_str(), output->getName().c_str());
          fprintf(stderr, "  %s\n", e.what());

          if(m_capturePreEventState){
            fprintf(stderr, "  preSendState = %s\n", src.type->getStateSpec()->toJSON(prevState).c_str());
          }
          fprintf(stderr, "     currState = %s\n", src.type->getStateSpec()->toJSON(src.state).c_str());
          throw;
        }

        src.readyToSend = src.type->calcReadyToSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get());

        if(m_log){
          auto id=nextSeqUnq();
          auto idStr=std::to_string(id);
          idSend=idStr;

          m_log->onSendEvent(
            idStr.c_str(),
            m_epoch,
            0.0,
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
        m_anyReady = m_anyReady || src.anyReady();
        continue;
      }

      sent=true;
      broadcast_message(src.index, sel, message, idSend);
      
    }
    ++m_epoch;
    return sent || m_anyReady;
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
  fprintf(stderr, "  --external [spec]  - Connect to external via given channel (e.g. 'FILE:src/dst' or 'STDIO').");
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

    std::string externalSpec;

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
      }else if(!strcmp("--external ",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --external\n");
          usage();
        }
        externalSpec=argv[ia+1];
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
  
    #if HAVE_POETS_INTERFACE_SPEC
    graph.m_pDownstreamConnectionEnv=std::make_shared<DownstreamConnectionEnvironmentBase>();
    #endif

    loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), &graph);
    if(logLevel>1){
      fprintf(stderr, "Loaded\n");
    }

    #if HAVE_POETS_INTERFACE_SPEC
    if(!externalSpec.empty()){
      if(logLevel>1){
        fprintf(stderr, "Establishing connection to external\n");
      }
      graph.m_pDownstreamConnection=std::make_shared<SingleDownstreamConnection>(graph.m_pDownstreamConnectionEnv);
      MessageChannelPair pair=client_connect_message_channel(externalSpec, "");
      graph.m_pDownstreamConnection->add_connection(pair.input, pair.output);
      if(logLevel>1){
        fprintf(stderr, "  external connected\n");
      }
    }else{
      if(graph.m_pDownstreamConnectionEnv->have_externals()){
        if(logLevel>1){
          fprintf(stderr, "WARNING: externals exist, but there is no explicit connection. Sending any output messages to stdout. No input will happen.");
        }
      }
    }
    #endif


    std::unique_ptr<SnapshotWriter> snapshotWriter;
    if(snapshotDelta!=0){
      snapshotWriter.reset(new SnapshotWriterToFile(snapshotSinkName.c_str()));
    }

    std::mt19937 rng;

    #if HAVE_POETS_INTERFACE_SPEC
    if(graph.m_pDownstreamConnection){
      if(logLevel>1){
        fprintf(stderr, "Waiting for OkGo from client\n");
      }
      graph.m_pDownstreamConnection->wait_for_okgo();
      if(logLevel>1){
        fprintf(stderr, "  OkGo!\n");
      }
    }
    #endif

    graph.init();

    if(snapshotWriter){
      graph.writeSnapshot(snapshotWriter.get(), 0.0, 0);
    }
    int nextStats=0;
    int nextSnapshot=snapshotDelta ? snapshotDelta-1 : -1;
    int snapshotSequenceNum=1;

    graph.m_capturePreEventState=enableAccurateAssertions || !checkpointName.empty();

    bool running=true;
    for(int i=0; i<maxSteps; i++){
      running = graph.step(rng, probSend, !running);

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

      #if HAVE_POETS_INTERFACE_SPEC
      if(graph.m_pDownstreamConnection){
        graph.m_pDownstreamConnection->flush();
      }
      #endif

      if(!running){
        #if HAVE_POETS_INTERFACE_SPEC
        if(graph.m_pDownstreamConnection){
          fprintf(stderr, "  Internal events have finished, but external read connection is open.\n");
          continue;
        }
        #endif
        break;
      }


      /*
      TODO
      if(!running){
        if(graph.m_pExternalConnection->isReadOpen() ){
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
      */
    }

    if(logLevel>1){
      fprintf(stderr, "Done\n");
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
