#ifndef graph_hpp
#define graph_hpp

#include <libxml++/nodes/element.h>

template<class T>
void load_typed_data_attribute(T &dst, xmlpp::Element *parent, const char *name)
{
  auto all=parent->find(std::string("./*[@name='")+name+"]");
  if(all.size()==0)
    return;
  if(all.size()>1)
    throw std::runtime_error("More than one property.");
  auto got=(xmlpp::Element*)all[0];
  if(got->get_name()!="Int32")
    throw std::runtime_error("Wrong xml node type.");
  xmlpp::Attribute *a=got->get_attribute("value");
  if(!a)
    return;
  std::stringstream tmp(a->get_value());
  tmp>>dst;
}

xmlpp::Element *find_single(xmlpp::Element *parent, const char *path)
{
  auto all=parent->find(path);
  if(all.size()==0)
    return 0;
  if(all.size()>1)
    throw std::runtime_error("More than on matching element.");
  auto res=dynamic_cast<xmlpp::Element*>(all[0]);
  if(res==0)
    throw std::runtime_error("Path did not identify an element.");
  return res;
}

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
  virtual TypedDataPtr create() const=0;
  virtual TypedDataPtr load(xmlpp::Element *parent) const=0;
  virtual void save(xmlpp::Element *parent, const TypedDataPtr &data) const=0;
};


class EdgeType
{
public:
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


class GraphLoadEvents
{
protected:
  virtual void onAddEdgeType(const EdgeTypePtr &et) =0;

  virtual void onAddDeviceType(const DeviceTypePtr &dt) =0;

  virtual uint64_t onAddDeviceInstance
  (
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state
  ) =0;

  virtual void onAddEdgeInstance
  (
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr properties,
   TypedDataPtr state
  ) =0;
};

void loadGraph(xmlpp::Element *elt, GraphLoadEvents *events);

#endif
