#ifndef graph_hpp
#define graph_hpp

#include <libxml++/nodes/element.h>

/* What is the point of all this?

   - C/C++ doesn't support introspection, so there is no way of getting at data-structures
     at run-time.

   - We want richly typed structures for debugging purposes. Persisted forms should be
     human readable (for debuggable size graphs)

   - We need to be able to bind code to arbitrary input/output ports in the graph

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


class typed_data_t
{
  // This is opaque data, and should be a POD
};
typedef std::shared_ptr<typed_data_t> TypedDataPtr;

class TypedDataSpec;
class EdgeType;
class DeviceType;
class GraphType;

typedef std::shared_ptr<TypedDataSpec> TypedDataSpecPtr;
typedef std::shared_ptr<EdgeType> EdgeTypePtr;
typedef std::shared_ptr<DeviceType> DeviceTypePtr;
typedef std::shared_ptr<GraphType> GraphTypePtr;

class TypedDataSpec
{
public:
  static TypedDataSpecPtr lookupTypedDataSpec(const std::string &id);
  static void registerTypedDataSpec(const std::string &id, const TypedDataSpecPtr &spec);
  
  virtual TypedDataPtr create() const=0;
  virtual TypedDataPtr load(xmlpp::Element *parent) const=0;
  virtual void save(xmlpp::Element *parent, const TypedDataPtr &data) const=0;
};


class EdgeType
{
public:
  static void registerEdgeType(const std::string &name, EdgeTypePtr dev);
  static EdgeTypePtr lookupEdgeType(const std::string &name);
  
  virtual const std::string &getId() const=0;

  virtual const TypedDataSpecPtr &getPropertiesSpec() const=0;
  virtual const TypedDataSpecPtr &getStateSpec() const=0;
  virtual const TypedDataSpecPtr &getMessageSpec() const=0;
};

class Port
{
public:
  virtual const DeviceTypePtr &getDeviceType() const=0;
  
  virtual const std::string &getName() const=0;
  virtual unsigned getIndex() const=0;

  virtual const EdgeTypePtr &getEdgeType() const=0;
};
typedef std::shared_ptr<Port> PortPtr;

class InputPort
  : public Port
{
public:
  virtual void onReceive(const typed_data_t *graphProperties,
			 const typed_data_t *deviceProperties,
			 typed_data_t *deviceState,
			 const typed_data_t *edgeProperties,
			 typed_data_t *edgeState,
			 const typed_data_t *message,
			 bool *requestSendPerOutput
		      ) const=0;
};
typedef std::shared_ptr<InputPort> InputPortPtr;

class OutputPort
  : public Port
{
public:
  virtual void onSend(const typed_data_t *graphProperties,
		      const typed_data_t *deviceProperties,
		      typed_data_t *deviceState,
		      typed_data_t *message,
		      bool *requestSendPerOutput,
		      bool *abortThisSend
		      ) const=0;
};
typedef std::shared_ptr<OutputPort> OutputPortPtr;

class DeviceType
{
public:
  virtual const std::string &getId() const=0;

  virtual const TypedDataSpecPtr &getPropertiesSpec() const=0;
  virtual const TypedDataSpecPtr &getStateSpec() const=0;

  virtual unsigned getInputCount() const=0;
  virtual const InputPortPtr &getInput(unsigned index) const=0;
  virtual const InputPortPtr &getInput(const std::string &name) const=0;

  virtual unsigned getOutputCount() const=0;
  virtual const OutputPortPtr &getOutput(unsigned index) const=0;
  virtual const OutputPortPtr &getOutput(const std::string &name) const=0;
};

class GraphType
{
  virtual const std::string &getId() const=0;

  virtual unsigned getDeviceTypeCount() const=0;
  virtual const DeviceTypePtr &getDeviceType(unsigned index) const=0;
  virtual const DeviceTypePtr &getDeviceType(const std::string &name) const=0;
  
  virtual unsigned getEdgeTypeCount() const=0;
  virtual const EdgeTypePtr &getEdgeType(unsigned index) const=0;
  virtual const EdgeTypePtr &getEdgeType(const std::string &name) const=0;
};

/* These allow registration/discovery of different data types at run-time */

class Registry
{
public:
  virtual void registerGraphType(GraphTypePtr graph);
  virtual GraphTypePtr lookupGraphType(const std::string &id);
  
  virtual void registerEdgeType(EdgeTypePtr edge);
  virtual EdgeTypePtr lookupEdgeType(const std::string &id);

  virtual void registerDeviceType(DeviceTypePtr dev);
  virtual DeviceTypePtr lookupDeviceType(const std::string &id);
};

/*! This is an entry-point exposed by graph shared objects that allows them
  to register their various types.
*/
extern void registerGraphTypes(Registry *registry);


class GraphLoadEvents
{
public:
  virtual void onGraphTypeProperties(const GraphTypePtr &graph);
  
  virtual void onDeviceType(const DeviceTypePtr &device);

  virtual void onEdgeType(const EdgeTypePtr &edge);

  //! Tells the consumer that a new graph is starting
  virtual uint64_t onGraphInstance(const GraphTypePtr &graph, const std::string &id, const TypedDataPtr &properties);
  
  // Tells the consumer that a new instance is being added
  /*! The return value is a unique identifier that means something
    to the consumer. */
  virtual uint64_t onDeviceInstance
  (
   uint64_t graphInst,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state
  ) =0;

  //! Tells the consumer that the a new edge is being added
  /*! It is required that both device instances have already been
    added (otherwise the ids would not been known).
  */
  virtual void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr properties,
   TypedDataPtr state
  ) =0;
};

void loadGraph(xmlpp::Element *elt, GraphLoadEvents *events);

#endif
