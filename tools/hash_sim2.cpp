#include "graph_core.hpp"

#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <unordered_set>
#include <algorithm>

#include <cstring>
#include <cstdlib>

#include <type_traits>

#include "typed_data_interner.hpp"

using uint128_t = unsigned __int128;

std::ostream &operator<<(std::ostream &dst, uint128_t x)
{
  auto width=dst.width(16);
  auto flags=dst.flags();
  auto fill=dst.fill('0');
  dst<<std::hex<<uint64_t(x>>64)<<uint64_t(x);
  dst.width(width);
  dst.flags(flags);
  dst.fill(fill);
  return dst;
}

static unsigned  logLevel=2;

struct HashSim
  : public GraphLoadEvents
{
  mutable std::unordered_set<std::string> m_internedStrings;
  mutable TypedDataInterner m_internedData;

  
  using interned_typed_data_t = const TypedDataInterner::entry_t *;

  // Return a stable C pointer to the name. Allows us to store
  // pointers in the data structures, and avoid calling .c_str() everywhere
  const char *intern(const std::string &name) const
  {
    auto it=m_internedStrings.insert(name);
    return it.first->c_str();
  }
  
  interned_typed_data_t intern(const TypedDataPtr &data) const
  { return m_internedData.intern(data); }


  /////////////////////////////////////////////////////////////
  // Static structure

  struct edge_info
  {

    unsigned target_device_index;
    unsigned target_device_pin;
    TypedDataPtr properties;
    unsigned edge_address; // Global contiguous edge address across all edge instances, starting from 0

    uint64_t salt;
  };
  
  struct device_info
  {
    const char *name;
    DeviceTypePtr type;
    TypedDataPtr properties;
    TypedDataPtr initial_state;
    unsigned device_address; // Contiguous device address, starting from 0

    std::vector<std::vector<unsigned>> edges; // Contains edge addresses

    uint64_t salt;
  };

  TypedDataPtr m_graph_properties;
  std::vector<device_info> m_device_info;
  std::vector<edge_info> m_edge_info;

  std::mt19937_64 m_urng64;

public:
  const std::vector<device_info> &get_device_info() const
  { return m_device_info; }
private:

  //////////////////////////////////////////////////////////////////////
  // Load events

  uint64_t onBeginGraphInstance(
    const GraphTypePtr &graph,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ) override {
    m_graph_properties=properties;
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
  ) override {
    unsigned address=m_device_info.size();
    device_info di;
    di.device_address=address;
    di.type=dt;
    di.properties=properties;
    di.initial_state=state;
    di.name=intern(id);
    di.salt=m_urng64() | 1; // We use multiplicative combine, so make sure LSB is 1
    di.edges.resize(dt->getOutputCount());
    m_device_info.push_back(di);
    return address;
  }

  void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   int sendIndex, // -1 if it is not indexed pin, or if index is not explicitly specified
   const TypedDataPtr &properties,
   rapidjson::Document &&metadata=rapidjson::Document()
  ) override {
    unsigned address=m_edge_info.size();
    edge_info ei;
    ei.edge_address=address;
    ei.properties=properties;
    ei.target_device_index=dstDevInst;
    ei.target_device_pin=dstPin->getIndex();
    ei.salt=m_urng64() | 1;
    m_device_info.at(srcDevInst).edges.at(srcPin->getIndex()).push_back(address);
    m_edge_info.push_back(ei);
  }

  ///////////////////////////////////////////////////////////////////////
  // Hashing stuff

public:
  typedef uint128_t world_hash_t;

  struct device_state
  {
    unsigned device_address;
    interned_typed_data_t state; // Just a pointer
    uint32_t rts; // Technically can be projected from state, but cheaper to keep here
  };

  struct message_instance
  {
    unsigned edge_address;
    interned_typed_data_t message; // Just a pointer

    bool operator==(const message_instance &o) const
    {
      return edge_address==o.edge_address && message==o.message;
    }
  };


  uint128_t world_hash_add(uint128_t acc, uint128_t x) const
  {
    // This function must be associative
    return acc + x; // TODO: Could make this addition modulo prime? Overkill?
  }

  uint128_t world_hash_remove(uint128_t acc, uint128_t x) const
  {
    // This must reverse world_hash_add,   so  forall x,h : h == world_hash_remove(world_hash_add(h,x),x)
    return acc - x;
  }

  world_hash_t world_hash_add_device(world_hash_t hash, unsigned device_address, const interned_typed_data_t state) const
  {
    return world_hash_add(hash, m_device_info[device_address].salt * uint128_t(state->hash));
  }

  world_hash_t world_hash_update_device(world_hash_t hash, unsigned device_address, const interned_typed_data_t before, const interned_typed_data_t after) const
  {
    auto salt=m_device_info[device_address].salt;
    hash=world_hash_remove(hash, salt * uint128_t(before->hash));
    return world_hash_add(hash, salt * uint128_t(after->hash));
  }

  world_hash_t world_hash_add_message(world_hash_t hash, const message_instance &msg) const
  {
    return world_hash_add(hash, m_edge_info[msg.edge_address].salt * uint128_t(msg.message->hash));
  }

  world_hash_t world_hash_remove_message(world_hash_t hash, const message_instance &msg) const
  {
    return world_hash_remove(hash, m_edge_info[msg.edge_address].salt * uint128_t(msg.message->hash));
  }

  world_hash_t world_hash_add_exit(world_hash_t hash, int code) const
  {
    return world_hash_add( hash, code ? 0x207bb7a8f6ceab58ull : 0x5871d2579015cdc6ull  );
  }

  template<class TDev, class TMsg>
  world_hash_t create_world_hash(
    const TDev &devices,
    const TMsg &messages
  ) const {
    world_hash_t acc=0;

    for(const auto &dev : devices){
      acc = world_hash_add_device(acc, dev.state);
    }
    for(const auto &msg : messages){
      acc = world_hash_add_message(acc, msg);
    }
    return acc;
  }

  struct world_state
  {
    // We want copying world-state to be (roughly) two mallocs and two memcpys
    static_assert(std::is_pod<device_state>::value, "Expected device_state to be cheap.");
    static_assert(std::is_pod<message_instance>::value, "Expected message_instance to be cheap.");

    std::vector<device_state> devices;
    // Deliberate choice to use vector over unordered_multiset:
    // - Vector should never be too big, so cost of finding is not too worrying
    // - We want to make copying cheap with few allocations
    // If finding and removing messages is expensive we could either move to sorted vector,
    // or identify message by index or something else
    std::vector<message_instance> messages;
    bool exited;
    int exitcode;
    world_hash_t hash;
  };
private:

  //////////////////////////////////////////////////////////////////////
  // Event chain

  class EmptyOrchestratorServices:
    public OrchestratorServices
  {
  public:
    bool exited=false;
    int exitcode=0;

    EmptyOrchestratorServices()
      : OrchestratorServices(0)
    {}

    // Log a handler message with the given log level
    virtual void vlog(unsigned level, const char *msg, va_list args)
    {
      if(!strcmp(msg,"_HANDLER_EXIT_SUCCESS_9be65737_")){
        exited=true;
      }else if(!strcmp(msg, "_HANDLER_EXIT_FAIL_9be65737_")){
        exited=true;
        exitcode |= 1; // Make errors sticky
      }


      //vfprintf(stderr, msg, args);
      //fputc('\n', stderr);
    }

    void export_key_value(uint32_t key, uint32_t value)
    {
      // Do nothing
      assert(0); // Plus obsolete
    }

    void vcheckpoint(bool preEvent, int level, const char *tagFmt, va_list tagArgs)
    {
      // Do nothing
      assert(0); // Plus obsolete
    }
  
    void application_exit(int code)
    {
      exited=true;
      exitcode|=code;
    }
  };

public:

  /* Represents an event that transitions between two
  world states.
  
  Events should be quite lightweight to construct, and
  typically should have O(1) storage. As a consequence the
  event does not hold information about its post state; that
  is only available through apply.
  */
  struct Event
  {
  private:
    const HashSim *m_parent;
    std::shared_ptr<Event> m_predecessor;
    world_hash_t m_pre_hash;
    int m_depth;
  protected:
    Event(const HashSim *parent, const std::shared_ptr<Event> &predecessor, const world_hash_t preHash)
      : m_parent(parent)
      , m_predecessor(predecessor)
      , m_pre_hash(preHash)
      , m_depth( predecessor ? predecessor->get_depth()+1 : 0)
    {}

    const device_info &d_info(unsigned device_address) const
    { return m_parent->m_device_info[device_address]; }

    const edge_info &e_info(unsigned edge_address) const
    { return m_parent->m_edge_info[edge_address]; }

    const typed_data_t *graph_properties() const
    { return m_parent->m_graph_properties.get(); }

    void check_exit(world_state &state, EmptyOrchestratorServices &orch) const
    {
      if(orch.exited){
        state.exited=true;
        state.exitcode=orch.exitcode;
        state.hash=m_parent->world_hash_add_exit(state.hash, orch.exitcode);
      }
    }

  public:
    const HashSim *get_parent() const
    { return m_parent; }

    std::shared_ptr<Event> get_predecessor() const
    { return m_predecessor; }

    int get_depth() const
    { return m_depth; }

    world_hash_t get_pre_hash() const
    { return m_pre_hash; }

    virtual std::string get_type() const =0;

    bool is_state_in_history(world_state state) const
    {
      if(state.hash==m_pre_hash){
        return true;
      }
      if(m_predecessor){
        return m_predecessor->is_state_in_history(state);
      }else{
        return false;
      }
    }

    /*! Apply the event to transition the world to the new state. */
    virtual void apply(
      world_state &state
    ) const =0;

  };

  struct InitEvent
    : public Event
  {
  public:
    InitEvent(const HashSim *parent)
      : Event(parent, std::shared_ptr<Event>(), 0)
    {      
    }

    virtual std::string get_type() const override
    { return "Init"; }

    void apply(
      world_state &world
    ) const {
      assert(world.messages.empty());
      assert(world.devices.empty());
      world.devices.reserve(get_parent()->get_device_info().size());

      world_hash_t acc=0;
      for(const auto &dev_info : get_parent()->get_device_info()){
        auto state=dev_info.initial_state.clone();

        device_state ds;
        ds.device_address=dev_info.device_address;
        EmptyOrchestratorServices orch;
        dev_info.type->init(&orch, graph_properties(), dev_info.properties.get(), state.get());
        ds.rts=dev_info.type->calcReadyToSend(&orch, graph_properties(), dev_info.properties.get(), state.get());
        ds.state=get_parent()->intern(state);

        check_exit(world, orch);
        
        world.devices.push_back(ds);
        acc=get_parent()->world_hash_add_device(acc, dev_info.device_address, world.devices.back().state);
      }
      world.hash=acc;
    }

  };

  struct SendEvent
    : public Event
  {
  private:
    unsigned m_source_device;
    unsigned m_source_pin;
    device_state m_pre_state;
  public:
    SendEvent(
        const HashSim *parent,
        const std::shared_ptr<Event> &predecessor,
        const world_state &state,
        unsigned source_device,
        unsigned source_pin
    )
      : Event(parent, predecessor, state.hash)
      , m_source_device(source_device)
      , m_source_pin(source_pin)
      , m_pre_state(state.devices[source_device])
    {}

    virtual std::string get_type() const override
    { return "Send"; }


    void apply(
      world_state &state
    ) const {

      const auto &di=d_info(m_source_device);
      auto &ds=state.devices.at(m_source_device);

      auto pin=di.type->getOutput(m_source_pin);

      auto oldState=ds.state;
      TypedDataPtr newStateR=oldState->data.clone();

      TypedDataPtr messageR=pin->getMessageType()->getMessageSpec()->create();

      bool doSend=true;
      unsigned sendIndex=-1;
      EmptyOrchestratorServices orch;
      pin->onSend(&orch, graph_properties(), di.properties.get(), newStateR.get(), messageR.get(), &doSend, pin->isIndexedSend() ? &sendIndex : nullptr);
      ds.state=get_parent()->intern(newStateR);
      ds.rts=di.type->calcReadyToSend(&orch, graph_properties(), di.properties.get(), newStateR.get());
      state.hash=get_parent()->world_hash_update_device(state.hash, di.device_address, oldState, ds.state);

      check_exit(state, orch);
      
      if(doSend){
        auto message=get_parent()->intern(messageR);

        assert( (sendIndex != -1) == (pin->isIndexedSend()) );
        if(sendIndex==-1){
          state.messages.reserve(state.messages.size()+di.edges.at(m_source_pin).size());

          for(auto ea : di.edges.at(m_source_pin)){
            const edge_info &ei = get_parent()->m_edge_info.at(ea);
            message_instance mi{ea, message};
            state.messages.push_back(mi);
            state.hash=get_parent()->world_hash_add_message(state.hash, mi);
          }
        }else{
          const edge_info &ei = get_parent()->m_edge_info.at( di.edges.at(m_source_pin).at(sendIndex) );
          message_instance mi{ei.edge_address, message};
          state.messages.push_back(mi);
          state.hash=get_parent()->world_hash_add_message(state.hash, mi);
        }
      }
    }
  };

  struct RecvEvent
    : public Event
  {
  private:
    message_instance m_message;
  public:
    RecvEvent(
        const HashSim *parent,
        const std::shared_ptr<Event> &predecessor,
        const world_state &state,
        const message_instance &message
    )
      : Event(parent, predecessor, state.hash)
      , m_message(message)
    {}

    virtual std::string get_type() const override
    { return "Recv"; }


    void apply(
      world_state &state
    ) const {
      auto it=std::find(state.messages.begin(), state.messages.end(), m_message);
      assert(it!=state.messages.end());
      std::swap(*it, state.messages.back());
      state.messages.resize(state.messages.size()-1);

      const edge_info &ei=get_parent()->m_edge_info[m_message.edge_address];

      const auto &di=d_info(ei.target_device_index);
      auto &ds=state.devices[ei.target_device_index];

      auto pin=di.type->getInput(ei.target_device_pin);

      auto oldState=ds.state;
      TypedDataPtr newStateR=oldState->data.clone();

      EmptyOrchestratorServices orch;
      pin->onReceive(&orch, graph_properties(), di.properties.get(), newStateR.get(), ei.properties.get(), nullptr, m_message.message->data.get());
      ds.state=get_parent()->intern(newStateR);
      ds.rts=di.type->calcReadyToSend(&orch, graph_properties(), di.properties.get(), ds.state->data.get());
      state.hash=get_parent()->world_hash_update_device(state.hash, di.device_address, oldState, ds.state);
      state.hash=get_parent()->world_hash_remove_message(state.hash, m_message);

      check_exit(state, orch);
    }
  };

  struct HardwareIdleEvent
    : public Event
  {
  public:
    HardwareIdleEvent(
        const HashSim *parent,
        const std::shared_ptr<Event> &predecessor,
        const world_state &state
    )
      : Event(parent, predecessor, state.hash)
    {}

    virtual std::string get_type() const override
    { return "HwIdle"; }


    void apply(
      world_state &state
    ) const {
      assert(state.messages.empty());

      for(unsigned i=0; i<get_parent()->m_device_info.size(); i++){
        const auto &di=get_parent()->m_device_info[i];
        auto &ds=state.devices[i];
        auto oldState=ds.state;
        TypedDataPtr newStateR=oldState->data.clone();

        throw std::runtime_error("Not implemented - need to hookin on hardware idle.");

        /*
        auto pin=di.type->getHard

        EmptyOrchestratorServices orch;
        pin->onHardwareIdle(&orch, graph_properties(), di.properties.get(), newStateR.get());
        ds.state=m_parent->intern(newStateR);
        ds.rts=di.type->calcReadyToSend(&orch, graph_properties(), di.properties, ds.state->data);
        state.hash=world_hash_update_device(state.hash, oldState, ds.state);

        check_exit(state, orch);
        */
      }
    }
  };

  template<class TCB>
  void enum_successor_events(const world_state &state, const std::shared_ptr<Event> &predecessor, TCB cb)
  {
    if(state.exited){
      return;
    }

    if(state.devices.empty()){
      cb(std::make_shared<InitEvent>(this));
      return;
    }

    bool any=false;
    for(unsigned i=0; i<m_device_info.size(); i++){
      unsigned rts_bits=state.devices[i].rts;
      unsigned rts_index=0;
      while(rts_bits){
        if(rts_bits&1){
          cb(std::make_shared<SendEvent>(this, predecessor, state, i, rts_index));
          any=true;
        }
        rts_bits>>=1;
        rts_index++;
      }
    }
    for(const auto &m : state.messages){
      cb(std::make_shared<RecvEvent>(this, predecessor,state,m));
      any=true;
    }

    if(!any){
      cb(std::make_shared<HardwareIdleEvent>(this, predecessor, state));
      fprintf(stderr, "Hardware Idle\n");
      exit(1);
    }
  }

  template<class TCB>
  void enum_successor_state_events(const world_state &state, const std::shared_ptr<Event> &predecessor, TCB cb)
  {
    world_state tmp; // This adds to the stack cost, so for depth-first enum might cause problems

    // Closure probably doesn't add to cost here, should be inlined out
    enum_successor_events(state, predecessor, [&](const std::shared_ptr<Event> &ev){
      // Not too happy about the cost of the alloc/copy here, but should
      // be only a couple of allocs plus memcpy as the structures are PODs
      tmp=state;
      ev->apply(tmp);
      cb( ev, std::move(tmp) ); // Either they grab it or they don't. If they don't, we re-use the storage
    });
  }
};


namespace std
{
  template<>
  class hash<HashSim::world_hash_t>{
  public:
    size_t operator()(const HashSim::world_hash_t &h) const
    {
      return uint64_t(h>>64) ^ uint64_t(h);
    }
  };
};

void breadth_first_visit(HashSim &sim)
{
  using world_hash_t = HashSim::world_hash_t;
  using world_state = HashSim::world_state;
  using Event = HashSim::Event;

  std::unordered_set<world_hash_t> visited;

  struct state_path
  {
    world_state state;
    std::shared_ptr<Event> predecessor;
  };

  std::vector<state_path> curr, next;

  world_state init;
  curr.push_back(state_path{init, std::shared_ptr<Event>()});

  bool doDot=false;
  if(doDot){
    std::cout<<"digraph state_space {\n";
  }

  unsigned d=0;
  while(d<1000 && !curr.empty()){
    next.clear();

    fprintf(stderr, "Beginning depth %d, state size=%u\n", d, curr.size());
    if(doDot){
      std::cout<<"// Round "<<d<<"\n";
    }
    unsigned merged=0;
    for(const auto &sp : curr){
      sim.enum_successor_state_events(sp.state, sp.predecessor,
        [&](const std::shared_ptr<Event> &ev, world_state &&state){
          if(doDot){
            std::cout<<"  n"<<sp.state.hash<<" -> n"<<state.hash<<" [ label=\""<<ev->get_type()<<"\"];\n";

            if(state.exited){
              std::cout<<"  n"<<state.hash<<"[peripheries=2];\n";
            }
          }

          //std::cerr<<"    "<< (ev ? ev->get_pre_hash() : uint128_t(0))<<" -> "<<(ev ? ev->get_type() : "<none>")<<" -> "<<state.hash<<"\n";

          auto it=visited.find(state.hash);
          if(it==visited.end()){
            // New state
            visited.insert(state.hash);
            next.push_back(state_path{ std::move(state), ev });
          }else{
            merged++;
          }
        }
      );
    }
    fprintf(stderr, "Finished depth %d, next size=%u, skipped=%u\n", d, next.size(), merged);
    d++;
    std::swap(curr, next);

    //if(curr.size()>5){
    //  curr.resize(5);
    //}
    /* std::cerr<<"  curr=\n";
    for(auto c : curr){
      std::cerr<<"    "<< (c.predecessor ? c.predecessor->get_pre_hash() : uint128_t(0))<<" -> "<<(c.predecessor ? c.predecessor->get_type() : "<none>")<<" -> "<<c.state.hash<<"\n";
    }*/
  }

  if(doDot){
    std::cout<<"}\n";
  }
};

int main(int argc, char *argv[])
{
  try{
    RegistryImpl registry;

    xmlpp::DomParser parser;

    filepath srcFilePath(argv[1]);

    filepath p(srcFilePath);
    p=absolute(p);
    if(logLevel>1){
      fprintf(stderr,"Parsing XML from '%s' ( = '%s' absolute)\n", srcFilePath.c_str(), p.c_str());
    }
    auto srcPath=p.parent_path();
    parser.parse_file(p.c_str());

    HashSim sim;

    loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), &sim);

    breadth_first_visit(sim);

  }catch(std::exception &e){
    std::cerr<<"Exception: "<<e.what()<<"\n";
    return 1;
  }
}