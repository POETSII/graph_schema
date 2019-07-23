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

#include "typed_data_interner.hpp"

static unsigned  logLevel=2;

struct HashSim
  : public GraphLoadEvents
{
  std::unordered_set<std::string> m_internedStrings;
  TypedDataInterner m_internedData;

  typedef TypedDataPtr interned_typed_data_t;
  

  // Return a stable C pointer to the name. Allows us to store
  // pointers in the data structures, and avoid calling .c_str() everywhere
  const char *intern(const std::string &name)
  {
    auto it=m_internedStrings.insert(name);
    return it.first->c_str();
  }
  
  TypedDataPtr intern(const TypedDataPtr &data)
  { return m_internedData.intern(data); }

  struct edge_info
  {

    unsigned target_device_index;
    unsigned target_device_pin;
    TypedDataPtr properties;
    unsigned device_address; // Global contiguous edge address across all edge instances, starting from 0

    uint64_t salt;
  };
  
  struct device_info
  {
    const char *name;
    DeviceTypePtr type;
    interned_typed_data_t initial_state;
    unsigned device_address; // Contiguous device address, starting from 0

    std::vector<std::vector<unsigned>> edges; // Contains edge addresses

    uint64_t salt;
  };

  std::vector<device_info> m_device_info;
  std::vector<device_state> m_edge_info;

  typedef uint128_t world_hash_t;

  struct device_state
  {
    unsigned device_address;
    interned_typed_data_t state;
    uint32_t rts; // Technically can be projected from state, but cheaper to keep here
    uint64_t state_hash;
  };

  struct message_instance
  {
    unsigned edge_address;
    interned_typed_data_t message;
    uint64_t message_hash;
  };

  typedef uint128_t world_hash_t;

  void world_hash_add(uint128_t acc, uint128_t x)
  {
    // This function must be associative
    return acc + x; // TODO: Could make this addition modulo prime? Overkill?
  }

  void world_hash_remove(uint128_t acc, uint128_t x)
  {
    // This must reverse world_hash_add,   so  forall x,h : h == world_hash_remove(world_hash_add(h,x),x)
    return acc - x;
  }

  world_hash_t world_hash_add_device(world_hash_t hash, const device_state &state)
  {
    unsigned address=state.device_address;
    return world_hash_add(hash, m_device_info[address].salt * uint128_t(state.state_hash));
  }

  world_hash_t world_hash_update_device(world_hash_t hash, const device_state &before, const device_state after)
  {
    unsigned address=state.device_address;
    auto salt=m_device_info[address].salt;
    hash=world_hash_remove(hash, m_device_info[address].salt * uint128_t(before.state_hash));
    return world_hash_add(hash, m_device_info[address].salt * uint128_t(after.state_hash));
  }

  world_hash_t world_hash_add_message(world_hash_t hash, const message_instance &msg)
  {
    return world_hash_add(hash, m_edge_info[msg.edge_address] * uint128_t(msg.message_hash));
  }

  world_hash_t world_hash_remove_message(world_hash_t hash, const message_instance &msg)
  {
    return world_hash_remove(hash, m_edge_info[msg.edge_address] * uint128_t(msg.message_hash));
  }

  template<class TDev, class TMsg>
  world_hash_t create_world_hash(
    const TDev &devices,
    const TMsg &messages
  ) const {
    world_hash_t acc=0;

    for(const auto &dev : devices){
      acc = world_hash_add_device(acc, dev);
    }
    for(const auto &msg : messages){
      acc = world_hash_add_message(acc, messages[i]);
    }
    return acc;
  }

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
  protected:
    Event(const HashSim *parent, const std::shared_ptr<Event> &predecessor, world_hash_t pre_hash)
      : m_parent(parent)
      , m_predecessor(predecessor)
      , m_pre_hash(pre_hash)
    {}

    const device_info &d_info(unsigned device_address) const
    { return m_parent->m_device_info[device_address]; }

    const edge_info &e_info(unsigned edge_address) const
    { return m_parent->m_edge_info[edge_address]; }

    const typed_data_t &graph_properties() const
    { return m_parent->graph_propertes.get(); }

  public:
    std::shared_ptr<Event> get_predecessor() const
    { return m_predecessor; }

    bool is_state_in_history(world_state_t state) const
    {
      if(state==m_pre_hash){
        return true;
      }
      if(m_predecessor){
        return m_predecessor->is_state_in_history(state);
      }else{
        return false;
      }
    }

    /*! Apply the event to transition the world to the new state. */
    world_hash_t apply(
      world_hash_t prevHash,
      std::vector<device_state> &devices,
      std::unordered_multiset<message_instance> &messages
    ) const;

    virtual world_hash_t reconstruct_state(
      world_hash_t prevHash,
      std::vector<device_state> &devices,
      std::unordered_multiset<message_instance> &messages
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

    world_hash_t apply(
      world_hash_t prevHash,
      std::vector<device_state> &devices,
      std::unordered_multiset<message_instance> &messages
    ) const {
      assert(messages.empty());
      assert(devices.empty());
      devices.reserve(m_parent->m_device_info.size());

      world_hash_t acc=get_prev_hash();
      for(const auto &dev_info : parent->m_device_info){
        auto state=dev_info.initial_state.clone();

        device_state_t ds;
        ds.rts=dev_info.type->init(&orch, parent->graph_properties, dev_info.properties, state.get());
        auto state_index=parent->m_internedData.internToIndex(state);
        ds.state_hash=parent->m_internedData.indexToHash(state_index);
        ds.state=parent->m_internedData.indexToData(state_index);

        devices.push_back(ds);
        acc=world_hash_add_device(acc, m_post_init_states.back());
      }
      return acc;
    }

    world_hash_t reconstruct_state(
      world_hash_t prevHash,
      std::vector<device_state> &devices,
      std::unordered_multiset<message_instance> &messages
    ) const override {
      assert(prevHash==0);
      devices.clear();
      messages.clear();
      return apply(devices,messages);
    }
  };

  struct SendEvent
    : public Event
  {
  private:
    unsigned m_source_device;
    unsigned m_source_pin;

  public:
    SendEvent(
        const HashSim *parent,
        const std::shared_ptr<Event> &predecessor,
        world_hash_t prevHash,
        unsigned source_device,
        unsigned source_pin
    )
      : Event(parent, std::shared_ptr<Event>(), prevHash)
      , m_source_device(source_device)
      , m_source_pin(source_pin)
    {}

    world_hash_t apply(
      world_hash_t prevHash,
      std::vector<device_state> &devices,
      std::unordered_multiset<message_instance> &messages
    ) const {
      const auto &di=d_info(m_source_device);
      auto &ds=devices[m_source_device];

      auto pin=di.type->getOutput(m_source_pin);

      TypedDataPtr oldState=ds.state;
      TypedDataPtr newState=oldState.clone();

      TypedDataPtr message=pin->getMessageType()->getMessageSpec()->create();

      bool doSend=true;
      unsigned sendIndex=-1;
      pin->onSend(&orch, graph_properties(), di.properties.get(), newState.get(), message.get(), &doSend, &sendIndex);

      return acc;
    }

    world_hash_t reconstruct_state(
      world_hash_t prevHash,
      std::vector<device_state> &devices,
      std::unordered_multiset<message_instance> &messages
    ) const override {
      assert(prevHash==0);
      devices.clear();
      messages.clear();
      return apply(devices,messages);
    }
  };

}
