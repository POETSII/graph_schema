#ifndef graph_core_hpp
#define graph_core_hpp

#include <libxml++/nodes/element.h>
#include "libxml/xmlwriter.h"
#include <cstdarg>

#include <memory>
#include <cstring>
#include <atomic>
#include <cassert>
#include <type_traits>

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"

#include "typed_data_spec.hpp"

#include "poets_hash.hpp"

/* What is the point of all this?

   - C/C++ doesn't support introspection, so there is no way of getting at data-structures
   at run-time.

   - We want richly typed structures for debugging purposes. Persisted forms should be
   human readable (for debuggable size graphs)

   - We need to be able to bind code to arbitrary input/output pins in the graph

   - We want speed for simulation purposes. Structured data can get in the way of that.

   The approach taken here is a mix of opaque and structured data types:

   - The kernel implementations are all strongly typed. Messages/state/properties are all
   proper structs. The kernel implementations only work in terms of those structs.

   - The external implementations are all type-agnostic, and do not know the detailed
   structure of anything. However, they have access to methods which allow them to
   save/load the opaque data-types, and the opaque structures can be passed directly
   to handlers with no need for translation.

   Handlers are made available at run-time via a registry. A given graph's types
   can be compiled into a shared object. The shared object contains the various
   type descriptors and handlers, and registers them in a run-time registry. As
   the graph is loaded, the various structs and device types are mapped to the
   equivalent handler.

   The intent is that when simulating, the only abstraction cost is for a
   virtual dispatch (usually quick), plus the loss of cross-function optimisation
   (which seems fair enough).

*/


#pragma pack(push,1)
struct typed_data_t
{
  // This is opaque data, and should be a POD
  std::atomic<unsigned> _ref_count; // This is exposed in order to allow cross-module optimisations
  uint32_t _total_size_bytes;  // All typed data instances must be a POD, and this is the total size, including header
};
#pragma pack(pop)

template<class T>
class DataPtr
{
  template<class TO>
  friend class DataPtr;

  static_assert(std::is_base_of<typed_data_t,T>::value, "This class only handles things derived from typed_data_t");

private:
  T *m_p;
public:
  DataPtr()
    : m_p(0)
  {}

  DataPtr(T *p)
    : m_p(p)
  {
    if(p){
      assert(p->_ref_count==0);
      p->_ref_count=1;
    }
  }

  template<class TO>
  DataPtr(TO *p)
    : m_p(p)
  {
    if(p){
      assert(p->_ref_count==0);
      p->_ref_count=1;
    }
  }

  DataPtr(const DataPtr &o)
    : m_p(o.m_p)
  {
    if(m_p){
      std::atomic_fetch_add(&m_p->_ref_count, 1u);
    }
  }

  template<class TO>
  DataPtr(const DataPtr<TO> &o)
    : m_p(o.m_p)
  {
    if(m_p){
      std::atomic_fetch_add(&m_p->_ref_count, 1u);
    }
  }

  DataPtr &operator=(const DataPtr &o)
  {
    if(m_p!=o.m_p){
      release();
      m_p=o.m_p;
      assert(m_p!=(T*)1);
      if(m_p){
        std::atomic_fetch_add(&m_p->_ref_count, 1u);
      }
    }
    return *this;
  }

  template<class TO>
  DataPtr &operator=(const DataPtr<TO> &o)
  {
    if(m_p!=o.m_p){
      release();
      m_p=o.m_p;
      if(m_p){
	std::atomic_fetch_add(&m_p->_ref_count, 1u);
      }
    }
    return *this;
  }

  DataPtr(DataPtr &&o)
    : m_p(o.m_p)
  {
    o.m_p=0;
  }

  template<class TO>
  DataPtr(DataPtr<TO> &&o)
    : m_p((T*)o.m_p)
  {
    o.m_p=0;
  }

  const T *get() const
  { return m_p; }

  T *get()
  { return m_p; }

  const T *operator->() const
  { return m_p; }

  T *operator->()
  { return m_p; }

  operator bool() const
  { return m_p!=0; }

  T *detach()
  {
    T *res=m_p;
    m_p=0;
    return res;
  }

  void attach(T *p)
  {
    release();
    m_p=p;
  }

  bool is_unique() const
  {
    assert(m_p);
    return m_p->_ref_count.load()==1;
  }

  void release()
  {
    if(m_p){
      if(std::atomic_fetch_sub(&m_p->_ref_count, 1u)==1){
        free(m_p);
      }
      m_p=0;
    }
  }

  void reset()
  { release(); }

  ~DataPtr()
  {
    release();
  }

  DataPtr clone() const
  {
    if(!m_p)
      return DataPtr();
    typed_data_t *p=(typed_data_t*)malloc(m_p->_total_size_bytes);
    memcpy(p, m_p, m_p->_total_size_bytes);
    p->_ref_count=0;
    return DataPtr((T*)p);
  }

  const uint8_t *payloadPtr() const
  {
    if(m_p){
      return ((const uint8_t*)m_p)+sizeof(typed_data_t);
    }else{
      return 0;
    }
  }

  size_t payloadSize() const
  {
    if(m_p){
      return m_p->_total_size_bytes-sizeof(typed_data_t);
    }else{
      return -1;
    }
  }

  POETSHash::hash_t payloadHash() const
  {
    POETSHash hash;
    if(m_p){
      hash.add(payloadPtr(), payloadSize());
    }
    return hash.getHash();
  }

  // null pointers compare less than non-null
  bool operator < (const DataPtr &o) const
  {
    if(!m_p){
      return o.m_p!=0;
    }
    if(!o.m_p){
      return false;
    }
    if(payloadSize() != o.payloadSize()){
      return payloadSize() < o.payloadSize();
    }
    // otherwise we need a byte-wise compare
    return memcmp(payloadPtr(), o.payloadPtr(), payloadSize()) < 0;
  }

  bool operator == (const DataPtr &o) const
  {
    if(m_p == o.m_p)
      return true;
    if(!m_p || !o.m_p)
      return false;
    if(payloadSize()!=o.payloadSize())
      return false;
    return 0==memcmp(payloadPtr(), o.payloadPtr(), payloadSize());
  }
};

namespace std
{
  template<class T>
  struct hash<DataPtr<T>>
  {
    typedef DataPtr<T> argument_type;

    typedef std::size_t result_type;

    result_type operator()(argument_type const& s) const
    {
      auto h=s.payloadHash();
      if(sizeof(h) > sizeof(size_t)){
        static_assert( sizeof(h) == sizeof(size_t) ||  sizeof(size_t)==4,"Assuming size_t is uint32_t");
        h=h ^ (h>>32);
      }
      return h;
    }
  };
};

template<class T>
DataPtr<T> make_data_ptr()
{
  return DataPtr<T>((T*)malloc(sizeof(T)));
}

typedef DataPtr<typed_data_t> TypedDataPtr;

class TypedDataSpec;
class MessageType;
class DeviceType;
class GraphType;

typedef std::shared_ptr<TypedDataSpec> TypedDataSpecPtr;
typedef std::shared_ptr<MessageType> MessageTypePtr;
typedef std::shared_ptr<DeviceType> DeviceTypePtr;
typedef std::shared_ptr<GraphType> GraphTypePtr;

class TypedDataSpec
{
public:
  virtual ~TypedDataSpec()
  {}

  //! Gets the detailed type of the data spec
  /*! For very lightweight implementations this may not be available
  */
  virtual std::shared_ptr<TypedDataSpecElementTuple> getTupleElement()
  { throw std::runtime_error("Not implemented."); }

  //! Size of the actual content, not including typed_data_t header
  virtual size_t payloadSize() const=0;

  //! Size of the entire typed_data_t instance, including standard header
  virtual size_t totalSize() const=0;

  virtual TypedDataPtr create() const=0;

  virtual TypedDataPtr load(xmlpp::Element *parent) const=0;

  virtual void save(xmlpp::Element *parent, const TypedDataPtr &data) const=0;

  virtual std::string toJSON(const TypedDataPtr &data) const=0;

  virtual void addDataHash(const TypedDataPtr &data, POETSHash &hash) const=0;

  POETSHash::hash_t getDataHash(const TypedDataPtr &data) const
  {
    POETSHash hash;
    addDataHash(data,hash);
    return hash.getHash();
  }


  virtual uint64_t getTypeHash() const
  {
    // TODO
    throw std::runtime_error("getTypeHash - Not implemented.");
  }
};


class MessageType
{
public:
  virtual ~MessageType()
  {}

  static void registerMessageType(const std::string &name, MessageTypePtr dev);
  static MessageTypePtr lookupMessageType(const std::string &name);

  virtual const std::string &getId() const=0;

  virtual const TypedDataSpecPtr &getMessageSpec() const=0;

  virtual rapidjson::Document &getMetadata() =0;
};

class Pin
{
public:
  virtual ~Pin()
  {}

  virtual const DeviceTypePtr &getDeviceType() const=0;

  virtual const std::string &getName() const=0;
  virtual unsigned getIndex() const=0;
  virtual bool isApplication() const=0;

  virtual const MessageTypePtr &getMessageType() const=0;

  virtual rapidjson::Document &getMetadata() =0;
};
typedef std::shared_ptr<Pin> PinPtr;

/* This provides and OS level services to the handler that it might need,
   such as logging.

   These services should be seen as non-functional, and it is legal to
   not actually pass in an orchestrator.

   Q: Why is this C++ when we work so hard to make the other parts pure C?
   A: This is not visible in the handler. All they can see are the primitive
   C functions defined by the handler spec.
*/
class OrchestratorServices
{
protected:
  unsigned m_logLevel;

  OrchestratorServices(unsigned logLevel)
    : m_logLevel(logLevel)
  {
  }
public:
  virtual ~OrchestratorServices()
  {}

  // Current logging level being used by the orchestrator
  // (used to allow handler to disable logging slightly more efficiently)
  unsigned getLogLevel() const
  { return m_logLevel; }

  // Log a handler message with the given log level
  virtual void vlog(unsigned level, const char *msg, va_list args) =0;

  // Export a key value pair from the application. This will be made
  // available in some way as (deviceInstId,sequence,key,value)
  // where sequence is a sequence number for that device instance
  // running from zero upwards
  virtual void export_key_value(uint32_t key, uint32_t value) =0;

  /*! Log the state of the currently sending/receiving device the
    current event, and associate with the given string tag. The id should
    be unique for any check-point on the calling device.

    \param preEvent If true, then log the state before the event. Otherwise log after

    \param level Used to establish different levels of checkpointing.

    \param tagFmt format string used to tag the event

    It is legal to call handler_checkpoint multiple times within a handler,
    as long as the id is different. For example, you might want to call with
    both pre and post event checkpoints.

  */
  virtual void vcheckpoint(bool preEvent, int level, const char *tagFmt, va_list tagArgs) =0;


  // Mark the application as complete. As soon as any device calls this,
  // the whole graph is considered complete. If multiple devices call
  // exit, then the run-time can non-determinstically choose any one of
  // them.
  virtual void application_exit(int code) =0;
};


class InputPin
  : public Pin
{
public:
  virtual void onReceive(OrchestratorServices *orchestrator,
			 const typed_data_t *graphProperties,
			 const typed_data_t *deviceProperties,
			 typed_data_t *deviceState,
			 const typed_data_t *edgeProperties,
			 typed_data_t *edgeState,
			 const typed_data_t *message
			 ) const=0;

  virtual const TypedDataSpecPtr &getPropertiesSpec() const=0;
  virtual const TypedDataSpecPtr &getStateSpec() const=0;

  virtual const std::string &getHandlerCode() const=0;
};
typedef std::shared_ptr<InputPin> InputPinPtr;

class OutputPin
  : public Pin
{
public:
  virtual void onSend(OrchestratorServices *orchestrator,
		      const typed_data_t *graphProperties,
		      const typed_data_t *deviceProperties,
		      typed_data_t *deviceState,
		      typed_data_t *message,
		      bool *doSend
		      ) const=0;

  virtual const std::string &getHandlerCode() const=0;
};
typedef std::shared_ptr<OutputPin> OutputPinPtr;

class DeviceType
{
public:
  virtual ~DeviceType()
  {}

  virtual const std::string &getId() const=0;

  virtual const TypedDataSpecPtr &getPropertiesSpec() const=0;
  virtual const TypedDataSpecPtr &getStateSpec() const=0;

  virtual const std::string &getReadyToSendCode() const=0;

  virtual const std::string &getSharedCode() const=0;

  virtual unsigned getInputCount() const=0;
  virtual const InputPinPtr &getInput(unsigned index) const=0;
  virtual InputPinPtr getInput(const std::string &name) const=0;
  virtual const std::vector<InputPinPtr> &getInputs() const=0;

  virtual unsigned getOutputCount() const=0;
  virtual const OutputPinPtr &getOutput(unsigned index) const=0;
  virtual OutputPinPtr getOutput(const std::string &name) const=0;
  virtual const std::vector<OutputPinPtr> &getOutputs() const=0;

  virtual void init(
           OrchestratorServices *orchestrator,
           const typed_data_t *graphProperties,
           const typed_data_t *deviceProperties,
           typed_data_t *deviceState
           ) const=0;

  virtual uint32_t calcReadyToSend(
				   OrchestratorServices *orchestrator,
				   const typed_data_t *graphProperties,
				   const typed_data_t *deviceProperties,
				   const typed_data_t *deviceState
				   ) const=0;

  virtual rapidjson::Document &getMetadata() =0;
};

class GraphType
{
public:
  virtual ~GraphType()
  {}

  virtual const std::string &getId() const=0;

  virtual const TypedDataSpecPtr getPropertiesSpec() const=0;

  virtual const std::string &getSharedCode() const=0;

  virtual unsigned getDeviceTypeCount() const=0;
  virtual const DeviceTypePtr &getDeviceType(unsigned index) const=0;
  virtual const DeviceTypePtr &getDeviceType(const std::string &name) const=0;
  virtual const std::vector<DeviceTypePtr> &getDeviceTypes() const=0;

  virtual unsigned getMessageTypeCount() const=0;
  virtual const MessageTypePtr &getMessageType(unsigned index) const=0;
  virtual const MessageTypePtr &getMessageType(const std::string &name) const=0;
  virtual const std::vector<MessageTypePtr> &getMessageTypes() const=0;

  virtual rapidjson::Document &getMetadata() =0;
};

/* These allow registration/discovery of different data types at run-time */

class Registry
{
public:
  virtual ~Registry()
  {}

  virtual void registerGraphType(GraphTypePtr graph) =0;
  virtual GraphTypePtr lookupGraphType(const std::string &id) const=0;

  virtual void registerMessageType(MessageTypePtr edge) =0;
  virtual MessageTypePtr lookupMessageType(const std::string &id) const=0;

  virtual void registerDeviceType(DeviceTypePtr dev) =0;
  virtual DeviceTypePtr lookupDeviceType(const std::string &id) const=0;
};

/*! This is an entry-point exposed by graph shared objects that allows them
  to register their various types.
*/
extern "C" void registerGraphTypes(Registry *registry);


#endif
