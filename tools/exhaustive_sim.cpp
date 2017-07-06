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

/*
Hashes need to be something we can incrementally update on:
- deliver a message
- send a message
and need to be over:
- the vector of device states (known size, finite)
- the vector of edge states (known size, finite)
- the multi-set of (dest,msg) pairs (as the same message may be in flight twice to the same device)

For the known topology parts, we come up with a set of unique
seeds, which serve to distuinguish them:
- seed[devId]   :    unique seed for a given device
- seedE[(dest,src)] : unique seed for a (dest,src) message in flight, including ports
- seedM[(dest,src)] : unique seed for a (dest,src) edge state, including ports
The seed is guaranteed to be odd, and apart from that should just be bit-balanced.

We incorporate the parts into the hash as follows:
- device state i:
    hash(devState)*seedD[devId]
- edge state (src,dev):
    hash(edgeState)*seedE[(dest,src)]
- message (count,dst,payload):
    hash(payload)*seedM[(dest,src)]*count

Parts are combined using arithmetic addition.

The updates are then:

- deliver message:
  hash += (hash(devStateNew) - hash(devStateOld) ) * seedD[devId]
  hash += (hash(edgeStateNew)- hash(edgeStateOld) )*seedE[(dest,src)]
  hash -= hash(message)*seedM[(dest,src)]
  
- send message:
  hash -= (hash(devStateOld) - hash(devStateOld) ) *seedD[devId]
  if !cancelSend:
    for dst in outgoing:
      hash += hash(message)*seedM[(dest,src)]

The seperate seedE and seedM are to avoid the situation where
hash(message)==hash(edgeStateNew) or something weird.

Both hash(.) and seed?[.] produce 64-bit numbers. Multiplication
produces 128 bit results, and addition is all mod-128.
*/

typedef __uint128_t uint128_t;

uint128_t mul64(uint64_t a, uint64_t b)
{
    return uint128_t(a)*uint128_t(b);
}



struct edge_info_t
{
    TypedDataPtr properties;
    
    uint64_t seedE;
    uint64_t seedM;
    unsigned dstDev;
    unsigned srcDev;
    unsigned dstPort;
    unsigned srcPort;
};

struct device_info_t
{
    TypedDataPtr properties;
    
    uint64_t seedD;
};


struct graph_info_t
{
    std::vector<device_info_t> devices;
    std::vector<edge_info_t> edges;
};


struct MessageList
{
    std::shared_ptr<MessageList> left;
    std::shared_ptr<MessageList> right;
    uint32_t message;
    uint32_t destination;
};

void map(std::shared_ptr<MessageList> curr, std::function<void(uint32_t,uint32_t)> cb)
{
    cb(curr->message, curr->destination);
    auto p=curr->left;
    while(p){
        cb(p->message, p->destination);
        p=p->left;
    }
    p=curr->right;
    while(p){
        cb(p->message, p->destination);
        p=p->right;
    }
}

std::shared_ptr<MessageList> push(std::shared_ptr<MessageList> curr, uint32_t message, uint32_t destination)
{
    return std::make_shared<MessageList>(curr, curr->right, message, destination);
}

std::shared_ptr<MessageList> pop(std::shared_ptr<MessageList> head)
{
    if(head->left){
        return std::make_shared<MessageList>(head->left->left, head->right, head->left->message, head->left->destination);
    }
    if(head->right){
        return std::make_shared<MessageList>(head->left, head->right->right, head->right->message, head->right->destination);
    }
    return std::shared_ptr<MessageList>();
}



template<unsigned TMaxDev>
class State
{
private:    
    uint128_t m_hash;
public:
    uint128_t getHash() const
    { return m_hash; }
    
    virtual TypedDataPtr getDeviceState(unsigned index) const=0;
    
    // Pick a device that is ready to send, or return -1
    virtual int getReadyToSend() const=0;
    
    virtual const State *getMessageLeft() const=0;
    const std::pair<uint32_t,uint32_t> &
    virtual const State *getMessageRight() const=0;
};

template<unsigned TMaxDev>
class SendState
    : public State<TMaxDev>
{
public:
    
};



class State
{
private:
    const graph_info_t *m_pGraph;
    const TypedDataInterner *m_pInterner;

  // Vector of interned states
  std::vector<uint32_t> m_states;

  // Vector of (edgeId,internedMsg) pairs
  std::vector<std::pair<uint32_t,uint32_t> > m_messages;

protected:
    State(
        const graph_info_t *pGraph,
        const TypedDataInterner *pInterner,
        std::vector<uint32_t> &&states,
        std::vector<std::pair<uint32_t,uint32_t> > &&messages
    )
        : m_pGraph(pGraph)
        , m_states(states)
        , m_messages(messages)
    {
        assert(states.size()==m_pGraph->devices.size());

        uint128_t acc=0;
        for(unsigned i=0; i<states.size(); i++){
            
        }
  }
public:
  virtual ~State()
  {}

  uint64_t getStateHash() const
  {
    return m_hash;
  }

  bool isSameState(const State *o) const
  {
    if(m_hash!=o.m_hash){
      return false;
    }
    return m_states==o->m_states && m_messages==o->m_messages;
  }
};


class Context
{
    TypedDataInterner interner;
    std::unordered_set<State> seen;
};


void deliver(
    Context &context,
    State &state,
    uint32_t messageIndex
){
  State next(state);
}


void exhaust(
  State &state,
  std::unordered_set<State> &seen
){
  for(unsigned i=0; i<state.messages.size(); i++){
    deliver(state, seen, i);
  }
  for(auto &s : state.readyToSend()){
    send(state, seen, x);
  }
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
    unsigned dstPortIndex;
    unsigned dstPortSlot; // This is the actual landing zone within the destination, i.e. where the state is
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
  
  bool m_deviceExitCalled=false;
  bool m_deviceExitCode=0;
  std::function<void (const char*,int)> m_onDeviceExit;

  uint64_t m_unq;
  
  uint64_t nextSeqUnq()
  {
    return ++m_unq;
  }

  virtual uint64_t onBeginGraphInstance(const GraphTypePtr &graphType, const std::string &id, const TypedDataPtr &graphProperties) override
  {
    m_graphType=graphType;
    m_id=id;
    m_graphProperties=graphProperties;
    return 0;
  }

  virtual uint64_t onDeviceInstance(uint64_t gId, const DeviceTypePtr &dt, const std::string &id, const TypedDataPtr &deviceProperties) override
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

  void onEdgeInstance(uint64_t gId, uint64_t dstDevIndex, const DeviceTypePtr &dstDevType, const InputPortPtr &dstInput, uint64_t srcDevIndex, const DeviceTypePtr &srcDevType, const OutputPortPtr &srcOutput, const TypedDataPtr &properties) override
  {
    input i;
    i.properties=properties;
    i.state=dstInput->getStateSpec()->create();
    i.firings=0;
    i.id=intern( m_devices.at(dstDevIndex).id + ":" + dstInput->getName() + "-" + m_devices.at(srcDevIndex).id+":"+srcOutput->getName() );
    auto &slots=m_devices.at(dstDevIndex).inputs.at(dstInput->getIndex());
    unsigned dstPortSlot=slots.size();
    slots.push_back(i);

    output o;
    o.dstDevice=dstDevIndex;
    o.dstPortIndex=dstInput->getIndex();
    o.dstPortSlot=dstPortSlot;
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
    
    for(auto &dev : m_devices){
      ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, dev.name, "__init__", m_onExportKeyValue, m_onDeviceExit  };
      auto init=dev.type->getInput("__init__");
      if(init){
        if(logLevel>2){
          fprintf(stderr, "  init device %d = %s\n", dev.index, dev.id.c_str());
        }

        init->onReceive(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get(), 0, 0, 0);
      }
      dev.readyToSend = dev.type->calcReadyToSend(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get());
      
      if(m_log){
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

  template<class TRng>
  bool step(TRng &rng, double probSend)
  {
    // Within each step every object gets the chance to send a message with probability probSend

    std::uniform_real_distribution<> udist;

    ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, 0, 0, m_onExportKeyValue, m_onDeviceExit};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Recv: ";
      receiveServices.setPrefix(tmp.str().c_str());
    }

    SendOrchestratorServicesImpl sendServices{logLevel, stderr, 0, 0, m_onExportKeyValue, m_onDeviceExit};
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


    bool sent=false;
    bool anyReady=false;

    unsigned rot=rng();

    uint32_t threshSend=(uint32_t)ldexp(probSend, 32);
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
        fprintf(stderr, "    output port %d ready\n", sel);
      }

      m_statsSends++;

      const OutputPortPtr &output=src.type->getOutput(sel);
      TypedDataPtr message(output->getMessageType()->getMessageSpec()->create());
      
      std::string idSend;      
      
      bool doSend=true;
      {
        uint32_t check=src.type->calcReadyToSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get());
        assert(check);
        assert( (check>>sel) & 1);

        sendServices.setSender(src.name, src.outputNames[sel]);
        output->onSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get(), message.get(), &doSend);

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
        anyReady = anyReady || src.anyReady();
        continue;
      }

      sent=true;

      for(auto &out : src.outputs[sel]){
        auto &dst=m_devices[out.dstDevice];
        auto &in=dst.inputs[out.dstPortIndex];
        auto &slot=in[out.dstPortSlot];

        slot.firings++;

        if(logLevel>3){
          fprintf(stderr, "    sending to device %d = %s\n", dst.index, dst.id.c_str());
        }

        const auto &port=dst.type->getInput(out.dstPortIndex);

        receiveServices.setReceiver(out.dstDeviceId, out.dstInputName);
        port->onReceive(&receiveServices, m_graphProperties.get(), dst.properties.get(), dst.state.get(), slot.properties.get(), slot.state.get(), message.get());
        dst.readyToSend = dst.type->calcReadyToSend(&receiveServices, m_graphProperties.get(), dst.properties.get(), dst.state.get());

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
            port,
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
  exit(1);
}

std::shared_ptr<LogWriter> g_pLog; // for flushing purposes on exit

void atexit_close_log()
{
  if(g_pLog){
    g_pLog->close();
  }
}

void onsignal_close_log (int)
{
  if(g_pLog){
    g_pLog->close();
  }
  exit(1);
}

int main(int argc, char *argv[])
{
  try{

    std::string srcFilePath="-";

    std::string snapshotSinkName;
    unsigned snapshotDelta=0;
    
    std::string logSinkName;

    unsigned statsDelta=1;

    int maxSteps=INT_MAX;

    double probSend=0.9;

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
      }else{
        srcFilePath=argv[ia];
        ia++;
      }
    }

    RegistryImpl registry;

    std::istream *src=&std::cin;
    std::ifstream srcFile;
    filepath srcPath(current_path());

    if(srcFilePath!="-"){
      filepath p(srcFilePath);
      p=absolute(p);
      if(logLevel>1){
        fprintf(stderr,"Reading from '%s' ( = '%s' absolute)\n", srcFilePath.c_str(), p.c_str());
      }
      srcFile.open(p.c_str());
      if(!srcFile.is_open())
        throw std::runtime_error(std::string("Couldn't open '")+p.native()+"'");
      src=&srcFile;
      srcPath=p.parent_path();    
    }

    xmlpp::DomParser parser;

    if(logLevel>1){
      fprintf(stderr, "Parsing XML\n");
    }
    parser.parse_stream(*src);
    if(logLevel>1){
      fprintf(stderr, "Parsed XML\n");
    }

    EpochSim graph;
    
    if(!logSinkName.empty()){
      graph.m_log.reset(new LogWriterToFile(logSinkName.c_str()));
      g_pLog=graph.m_log;
      atexit(atexit_close_log);
      signal(SIGABRT, onsignal_close_log);
      signal(SIGINT, onsignal_close_log);
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

    for(int i=0; i<maxSteps; i++){
      bool running = graph.step(rng, probSend) && !graph.m_deviceExitCalled;

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
  
  }catch(std::exception &e){
    if(g_pLog){
      g_pLog->close();
    }
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    if(g_pLog){
      g_pLog->close();
    }
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
