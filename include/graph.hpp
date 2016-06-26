#ifndef graph_hpp
#define graph_hpp

#include <libxml++/nodes/element.h>


class typed_data_t
{
  // This is opaque data, and should be a POD
};
typedef std::shared_ptr<typed_data_t> TypedDataPtr;

class TypedDataSpec;
class EdgeType;
class DeviceType;

typedef std::shared_ptr<TypedDataSpec> TypedDataSpecPtr;
typedef std::shared_ptr<EdgeType> EdgeTypePtr;
typedef std::shared_ptr<DeviceType> DeviceTypePtr;

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
  static void registerDeviceType(const std::string &name, DeviceTypePtr dev);
  static DeviceTypePtr lookupDeviceType(const std::string &name);
  
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


class GraphLoadEvents
{
public:
  virtual void onGraphProperties(const std::string &id,
				 const TypedDataSpecPtr &propertiesSpec,
				 const TypedDataPtr &properties);
  
  //! Notifies the graph that a new type of 
  virtual void onDeviceType(const DeviceTypePtr &device);

  virtual void onEdgeType(const EdgeTypePtr &edge);
  
  // Tells the consumer that a new instance is being added
  /*! The return value is a unique identifier that means something
    to the consumer. */
  virtual uint64_t onDeviceInstance
  (
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
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr properties,
   TypedDataPtr state
  ) =0;
};

void loadGraph(xmlpp::Element *elt, GraphLoadEvents *events);

#endif
