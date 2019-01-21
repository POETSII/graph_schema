#include "graph.hpp"

#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <unordered_set>
#include <algorithm>

#include <cstring>
#include <cstdlib>

static unsigned  logLevel=2;

struct HashSim
  : public GraphLoadEvents
{
  std::unordered_set<std::string> m_internedStrings;
  TypedDataInterner m_internedData;
  

  // Return a stable C pointer to the name. Avoids us to store
  // pointers in the data structures, and avoid calling .c_str() everywhere
  const char *intern(const std::string &name)
  {
    auto it=m_interned.insert(name);
    return it.first->c_str();
  }
  
  TypedDataPtr intern(const TypedDataPtr &data)
  { return m_internedData.intern(data); }

  
  struct device_state
  {
    uint32_t deviceId;
    uint32_t rts;
    interned_typed_data_t state;
    
    // NOTE : No support for edge state in this version
  };
  
  struct message_state
  {
    interned_typed_data_t message;
    uint32_t edge;
    
    message_state(uint32_t _edge, interned_typed_data_t _message)
      : edge(_edge)
      , message(_message)
    
    message_state(uint32_t targetDevice, uint32_t targetPin, uint32_t sourceDevice, uint32_t sourcePin, interned_typed_data_t _message)
      : targetEndpoint( (targetDevice<<(5+11+5)) | (targetPin<<(11+5) | (sourceDevice<<(11) | sourcePin )
      , message(_message)
    {
      assert(targetDevice<2048);
      assert(sourceDevice<2048);
      assert(targetPin<32);
      assert(sourcePin<32);
    }
    
    uint32_t getTargetDevice() const
    { return (edge>>(5+11+5))&3FF; }
    
    uint32_t getTargetPin() const
    { return (edge>>(11+5))&0x1F; }
    
    uint32_t getSourceDevice() const
    { return (edge>>(5))&0x3FF; }
    
    uint32_t getSourceDevice() const
    { return (edge)&0x1F; }
    
    bool operator < (const message_state &o) const
    {
      if(edge < o.edge) return true;
      if(edge > o.edge) return true;
      return message < o.message;
    }
  };
  
  struct world_state
  {
    world_hash_t hash;
    std::vector<device_state> devices;
    std::multiset<message_state> messsages;
    
    void setDirty()
    {
      hash=0;
    }
    
    void setHash(world_hash_t _hash)
    {
      assert(_hash==calcHash());
      hash=_hash;
    }
    
    world_hash_t calcHash() const
    {
      POETSHash hash;
      for(const auto &dev : devices){
        hash.add(state);
      }
      for(const auto &msg : messages){
        hash.add(targetEndpoint);
        hash.add(message);
      }
      auto h= hash.getHash();
      if(h==0){
        h=-1;
      }
      return h;
    }
  };  
  
  
  class Event
  {
  private:
    world_hash_t m_preHash;
    world_hash_t m_postHash;
    std::shared_ptr<Event> m_parent;
    std::weak_ptr<Event> m_equivalence; // The examplar for this class
  protected:
    Event(world_hash_t preHash, std::shared_ptr<Event> parent)
      : m_preHash(preHash)
      , m_postHash(0)
      , m_parent(parent)
    {
    }
    
    void setPostHash(world_hash_t postHash)
    {
      assert(!m_postHash);
      assert(postHash);
      m_postHash=postHash;
    }
  public:
    
    world_hash_t getPreHash() const
    { return m_preHash; }
  
    world_hash_t getPostHash() const
    {
      assert(m_postHash);
      return m_postHash;
    }
    
    std::shared_ptr<Event> getParent() const
    { return m_parent; }
    
    void setEquivalence(std::shared_ptr<Event> ev)
    {
      assert(!m_equivalence);
      assert(ev->getPostHash() == getPostHash());
    }
    
    // Builds the state of the world from scratch
    virtual void getState(const world_properties &props, world_state &state) const
    {
      m_prev->getState(props, state);
      apply(props,state);
    }
    
    // Take the state of the world before the event, and move it to after the event
    virtual void apply(const world_properties &props, world_state &state) const=0;
    
    // Take the state of the world after the event, and move it to before the event
    virtual void undo(const world_properties &props, world_state &state) const=0;
  };
  
  class WorldInitEvent
    : public Event
  {
  private:
    std::vector<device_state> m_states;
  public:
    WorldInitEvent const world_topology &topology, world_state &state)
      : Event(state.getHash(), std::shared_ptr<Event>())
    {
      assert(state.devices.empty());
      assert(state.messages.empty());
      
      state.setDirty();
      
      state.devices.resize(topology.devices.size());
      for(unsigned i=0; i<topology.devices.size(); i++){
        ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, topology.devices[i].id.c_str(), "__init__"  };
        
        const auto &devInfo=topology.devices.at(i);
        DeviceTypePtr devType=devInfo.deviceType;
        
        TypedDataPtr state=devType->getStateSpec()->create();
        InputPinPtr init=devType->getInputPin("__init__");
        if(init){
          init->onRecv(&receiveServices, topology.graphProps, devProps->devProps, state);
        }
        
        uint32_t rts=devType->calcRTS(&receiveServices, topology.graphProps, devProps->devProps, state);
        
        state.devices[i].deviceId=i;
        state.devices[i].rts=rts;
        state.devices[i].state=intern(state);
      }
      
      m_states=state.devices;
      
      setPostHash(state.getHash());
    }
  
    virtual void getState(const world_properties &props, world_state &state) const override
    {
      apply(props,state);
    }
    
    // Take the state of the world before the event, and move it to after the event
    virtual void apply(const world_properties &props, world_state &state) const override
    {
      assert(state.getHash() == getPreHash());
      assert(state.devices.empty());
      assert(state.messages.empty());
      
      state.devices=m_states;
      
      state.setHash( getPostHash() );
    }
    
    // Take the state of the world after the event, and move it to before the event
    virtual void undo(const world_properties &props, world_state &state) const override
    {
      assert(state.getHash() == getPostHash());
      assert(state.devices == m_states);
      assert(state.messages.empty());
      
      state.devices.clear();
      
      state.setHash( getPreHash() );
    }
  };
    
  class SendEvent
    : public Event
  {
  private:
    uint32_t deviceIndex;
    uint32_t pinIndex;
    interned_typed_data_t preState;
    uint32_t preRTS;
  
    interned_typed_data_t message;
    bool cancelled;
  
    interned_typed_data_t postState;
    uint32_t postRTS;
  public:
    
    SendEvent(
      std::shared_ptr<Event> parent,
      const world_topology &topology,
      world_state_t &state,
      uint32_t _deviceIndex,
      uint32_t _pinIndex
    )
      : Event( state.getHash(), parent)
      , deviceIndex(_deviceIndex)
      , pinIndex(_pinIndex)
    {
      state.setDirty();
      
      auto &ds=state.devices.at(deviceIndex);
      assert( (ds.rts >> pinIndex ) & 1 );
      
      preState=ds.state;
      preRTS=ds.rts;
      
      const auto &deviceInfo=topology.devices.at(deviceIndex);
      auto deviceType=deviceInfo.deviceType;
      const auto &outputPin=dt.getOutput(_pinIndex);
      
      TypedDataPtr state=uninternFresh(ds.state);
      assert(state.is_unique());
      TypedDataPtr message=outputPin->getMessageType()->getMessageSpec()->create();
      
      cancelled=false;
      op->onSend(
        topology.graphProperties,
        dt.deviceProperties,
        state,
        message,
        &cancelled
      );
      postState=intern(state);
      
      postRTS=deviceType->calcRTS(
        topology.graphProperties,
        dt.deviceProperties,
        state
      );
      
      // Mutate the state
      ds.state=postState;
      ds.rts=postRTS;
      if(!cancelled){
        for(uint32_t edge : deviceInfo.outputs.at(pinIndex).edges){
          state.messages.insert( message_state(edge,message) );
        }
      }
      
      setPostHash(state.getHash());
    }
    
    virtual void apply(const world_properties &props, world_state &state) const override
    {
      auto &ds=state.devices.at(deviceIndex);
      
      assert(state.getHash()==getPreHash());
      assert(ds.state==preState);
      assert(ds.rts==preRTS);
      
      ds.state=postState;
      ds.rts=postRTS;
      if(!cancelled){
        for(uint32_t edge : deviceInfo.outputs.at(pinIndex).edges){
          state.messages.insert( message_state(edge,message) );
        }
      }
      
      state.setHash(getPostHash());
    }
    
    virtual void undo(const world_properties &props, world_state &state) const override
    {
      auto &ds=state.devices.at(deviceIndex);
      
      assert(state.getHash()==getPostHash());
      assert(ds.state==postState);
      assert(ds.rts==postRTS);
      
      ds.state=preState;
      ds.rts=preRTS;
      if(!cancelled){
        for(uint32_t edge : deviceInfo.outputs.at(pinIndex).edges){
          auto it=state.messages.find( message_state(edge,message) );
          assert(it!=state.messages.end());
          state.messages.erase(it);
        }
      }
      
      state.setHash(getPostHash());
    }
  };
  
  
  class RecvEvent
    : public Event
  {
  private:
    message_state message;
  
    interned_typed_data_t preState;
    uint32_t preRTS;
  
    interned_typed_data_t postState;
    uint32_t postRTS;
  public:
    
    RecvEvent(
      std::shared_ptr<Event> parent,
      const world_topology &topology,
      world_state_t &state,
      const message_state &msg
    )
      : Event( state.getHash(), parent)
      , message(msg)
    {
      state.setDirty();
      
      auto itMsg=state.messages.find(msg);
      assert(itMsg!=state.messages.end());
      
      unsigned deviceIndex=msg.getTargetDevice();
      unsigned pinIndex=msg.getTargetPin();
      auto &ds=state.devices.at(deviceIndex);
      
      preState=ds.state;
      preRTS=ds.rts;
      
      const auto &deviceInfo=topology.devices.at(deviceIndex);
      auto deviceType=deviceInfo.deviceType;
      const auto &outputPin=dt.getOutput(_pinIndex);
      
      TypedDataPtr state=uninternFresh(ds.state);
      assert(state.is_unique());
      
      op->onRecv(
        topology.graphProperties,
        dt.deviceProperties,
        state,
        deviceInfo.edges.at(message.edge),
        0, // no support for edge state
        message
      );
      postState=intern(state);
      
      postRTS=deviceType->calcRTS(
        topology.graphProperties,
        dt.deviceProperties,
        state
      );
      
      // Mutate the state
      ds.state=postState;
      ds.rts=postRTS;
      state.message.erase(itMsg);
      
      setPostHash(state.getHash());
    }
    
    virtual void apply(const world_properties &props, world_state &state) const override
    {
      auto &ds=state.devices.at(deviceIndex);
      
      auto itMsg=state.messages.find(message);
      
      assert(state.getHash()==getPreHash());
      assert(ds.state==preState);
      assert(ds.rts==preRTS);
      assert(itMsg!=state.messages.end());
      
      ds.state=postState;
      ds.rts=postRTS;
      state.messages.erase(itMsg);
      
      state.setHash(getPostHash());
    }
    
    virtual void undo(const world_properties &props, world_state &state) const override
    {
      auto &ds=state.devices.at(deviceIndex);
      
      auto itMsg=state.messages.find(message);
      
      assert(state.getHash()==getPostHash());
      assert(ds.state==postState);
      assert(ds.rts==postRTS);
      
      ds.state=preState;
      ds.rts=preRTS;
      state.messages.insert(message);
      
      state.setHash(getPreHash());
    }
  };
  
  struct device_info
  {
    unsigned index;
    std::string id;
    DeviceTypePtr type;
    TypedDataPtr properties;
    
    std::vector< std::vector<uint32_t> > outputs; // pinIndex -> [ (dstDev&dstPin&srcDev&srcPin) ]
    std::vector< std::vector<TypedDataPtr> > inputs; // pinIndex -> [ edgeProperties ]
  };
  
  struct world_topology
  {
    GraphTypePtr graphType;
    std::string id;
    std::vector<device_info> devices;
    std::unordered_map<std::string,unsigned> deviceIdToIndex;
    TypedDataPtr graphProperties;
  };
  
  world_topology topology;

  std::shared_ptr<LogWriter> m_log;

  uint64_t m_unq;
  
  uint64_t nextSeqUnq()
  {
    return ++m_unq;
  }

  virtual uint64_t onBeginGraphInstance(const GraphTypePtr &graphType, const std::string &id, const TypedDataPtr &graphProperties) override
  {
    topology.graphType=graphType;
    topology.id=id;
    topology.graphProperties=graphProperties;
    return 0;
  }

  virtual uint64_t onDeviceInstance(uint64_t gId, const DeviceTypePtr &dt, const std::string &id, const TypedDataPtr &deviceProperties) override
  {
    device_info info;
    info.index=topology.devices.size();
    info.id=id;
    info.deviceType=dt;
    info.deviceProperties=deviceProperties;
    info.outputs.resize( dt->getOutputCount() );
    
    topology.devices.push_back(info);
    topology.deviceIdToIndex[id]=info.index;
    
    return info.index;
  }

  void onEdgeInstance(uint64_t gId, uint64_t dstDevIndex, const DeviceTypePtr &dstDevType, const InputPinPtr &dstInput, uint64_t srcDevIndex, const DeviceTypePtr &srcDevType, const OutputPinPtr &srcOutput, const TypedDataPtr &properties) override
  {
    topology.at(dstDevIndex).inputs.at(dstInput->getIndex()).push_back(properties);
    
    message_state msg(dstDevIndex,dstInput->getIndex(),srcDevIndex,srcOutput->getIndex(), intern(TypeDataPtr()));
    topology.at(srcDevIndex).outputs.at(srcInput->getIndex()).push_back(msg.edge);
  }

  template<class TEventQueue>
  bool step(TEventQueue &eventQueue)
  {
    EventPtr curr=eventQueue.pop();
    if(!curr)
      return;
    
    world_state state;
    curr->getState(topology,state);
    
    // Handle sends
    for(auto &dev : state.devices){
      uint32_t rts=dev.rts;
      unsigned index=0;
      while(rts){
        if(rts&1){
          EventPtr send=std::make_shared<SendEvent>(curr, topology, state, devIndex, pinIndex);
          eventQueue.add(send); // Checks for cycles, and adds it to the known events. Pushes onto queue if not seen.
          send->undo(topology, state);
        }
        ++index;
        rts=rts>>1;
      }
    }
    
    // Handle receives
    for(auto &msg : state.messages){
      EventPtr recv=std::make_shared<RecvEvent>(curr, topology, state, msg);
      eventQueue.add(recv);
      recv->undo(topology, state);
    }
  }
};


struct FIFOQueue
{
  // Things we still need to to, in FIFO order
  std::vector<std::shared_ptr<Event> > queue;
  
  // The first version of each thing we saw with the same world hash
  std::unordered_map<uint64_t,std::shared_ptr<Event> > exemplars; 
  
  // Every event ever seen, including others within an equivalence class
  std::unordered_set<std::shared_ptr<Event>,hash_event_ptr,less_event_ptr> events;
  
  
  void add(std::shared_ptr<Event> ev)
  {
    exemplars.find(
  }
};


void usage()
{
  fprintf(stderr, "hash_sim [options] sourceFile?\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  --log-level n\n");
  fprintf(stderr, "  --max-steps n\n");
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
      }else{
        srcFilePath=argv[ia];
        ia++;
      }
    }

    RegistryImpl registry;

    std::istream *src=&std::cin;
    std::ifstream srcFile;

    if(srcFilePath!="-"){
      if(logLevel>1){
        fprintf(stderr,"Reading from '%s'\n", srcFilePath.c_str());
      }
      srcFile.open(srcFilePath.c_str());
      if(!srcFile.is_open())
        throw std::runtime_error(std::string("Couldn't open '")+srcFilePath+"'");
      src=&srcFile;
    }

    xmlpp::DomParser parser;

    if(logLevel>1){
      fprintf(stderr, "Parsing XML\n");
    }
    parser.parse_stream(*src);
    if(logLevel>1){
      fprintf(stderr, "Parsed XML\n");
    }

    HashSim graph;
    
    if(!logSinkName.empty()){
      graph.m_log.reset(new LogWriterToFile(logSinkName.c_str()));
    }

    loadGraph(&registry, parser.get_document()->get_root_node(), &graph);
    if(logLevel>1){
      fprintf(stderr, "Loaded\n");
    }

    std::unique_ptr<SnapshotWriter> snapshotWriter;
    if(snapshotDelta!=0){
      snapshotWriter.reset(new SnapshotWriterToFile(snapshotSinkName.c_str()));
    }

    std::mt19937 rng;

    while(true){
      bool running = graph.step(rng, probSend);

      if(!running){
        break;
      }
    }

    if(logLevel>1){
      fprintf(stderr, "Done\n");
    }

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
