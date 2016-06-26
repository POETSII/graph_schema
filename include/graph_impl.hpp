#ifndef graph_impl_hpp
#define graph_impl_hpp

#include "graph.hpp"


#include <cassert>
#include <cstdint>

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

std::string get_attribute_required(xmlpp::Element *eParent, const char *name)
{
  auto a=eParent->get_attribute(name);
  if(a==0)
    throw std::runtime_error("Missing attribute.");

  return a->get_value();
}

template<class T>
const T *cast_typed_properties(const typed_data_t *properties)
{  return static_cast<const T *>(properties); }

template<class T>
T *cast_typed_data(typed_data_t *data)
{  return static_cast<T *>(data); }


class InputPortImpl
  : public InputPort
{
private:
  DeviceTypePtr (*m_deviceTypeSrc)(); // Avoid circular initialisation with device type
  mutable DeviceTypePtr m_deviceType;
  std::string m_name;
  unsigned m_index;
  EdgeTypePtr m_edgeType;
protected:
  InputPortImpl(DeviceTypePtr (*deviceTypeSrc)(), const std::string &name, unsigned index, EdgeTypePtr edgeType)
    : m_deviceTypeSrc(deviceTypeSrc)
    , m_name(name)
    , m_index(index)
    , m_edgeType(edgeType)
  {}
public:
  virtual const DeviceTypePtr &getDeviceType() const override 
  {
    if(!m_deviceType)
      m_deviceType=m_deviceTypeSrc();
    return m_deviceType;
  }
  
  virtual const std::string &getName() const override
  { return m_name; }
    
  virtual unsigned getIndex() const override
  { return m_index; }

  virtual const EdgeTypePtr &getEdgeType() const override
  { return m_edgeType; }
  
};


class OutputPortImpl
  : public OutputPort
{
private:
  DeviceTypePtr (*m_deviceTypeSrc)(); // Avoid circular initialisation with device type
  mutable DeviceTypePtr m_deviceType;
  std::string m_name;
  unsigned m_index;
  EdgeTypePtr m_edgeType;
protected:
  OutputPortImpl(DeviceTypePtr (*deviceTypeSrc)(), const std::string &name, unsigned index, EdgeTypePtr edgeType)
    : m_deviceTypeSrc(deviceTypeSrc)
    , m_name(name)
    , m_index(index)
    , m_edgeType(edgeType)
  {}
public:
  virtual const DeviceTypePtr &getDeviceType() const override 
  {
    if(!m_deviceType)
      m_deviceType=m_deviceTypeSrc();
    return m_deviceType;
  }
  
  virtual const std::string &getName() const override
  { return m_name; }
    
  virtual unsigned getIndex() const override
  { return m_index; }

  virtual const EdgeTypePtr &getEdgeType() const override
  { return m_edgeType; }
  
};

class EdgeTypeImpl
  : public EdgeType
{
private:
  std::string m_id;
  TypedDataSpecPtr m_properties;
  TypedDataSpecPtr m_state;
  TypedDataSpecPtr m_message;
  
protected:
  EdgeTypeImpl(const std::string &id, TypedDataSpecPtr properties, TypedDataSpecPtr state, TypedDataSpecPtr message)
    : m_id(id)
    , m_properties(properties)
    , m_state(state)
    , m_message(message)
  {}
public:
  virtual const std::string &getId() const override
  { return m_id; }

  virtual const TypedDataSpecPtr &getPropertiesSpec() const override
  { return m_properties; }
  
  virtual const TypedDataSpecPtr &getStateSpec() const override
  { return m_state; }
  
  virtual const TypedDataSpecPtr &getMessageSpec() const override
  { return m_message; }
};


class DeviceTypeImpl
  : public DeviceType
{
private:
  std::string m_id;

  TypedDataSpecPtr m_properties;
  TypedDataSpecPtr m_state;
  
  std::vector<InputPortPtr> m_inputsByIndex;
  std::map<std::string,InputPortPtr> m_inputsByName;

  std::vector<OutputPortPtr> m_outputsByIndex;
  std::map<std::string,OutputPortPtr> m_outputsByName;
  
protected:
  DeviceTypeImpl(const std::string &id, TypedDataSpecPtr properties, TypedDataSpecPtr state, const std::vector<InputPortPtr> &inputs, const std::vector<OutputPortPtr> &outputs)
    : m_id(id)
    , m_properties(properties)
    , m_state(state)
    , m_inputsByIndex(inputs)
    , m_outputsByIndex(outputs)
  {
    for(auto &i : inputs){
      m_inputsByName.insert(std::make_pair(i->getName(), i));
    }
    for(auto &o : outputs){
      m_outputsByName.insert(std::make_pair(o->getName(), o));
    }
  }
  
public:
  virtual const std::string &getId() const override
  { return m_id; }
    
  virtual const TypedDataSpecPtr &getPropertiesSpec() const override
  { return m_properties; }
  
  virtual const TypedDataSpecPtr &getStateSpec() const override
  { return m_state; }

  virtual unsigned getInputCount() const override
  { return m_inputsByIndex.size(); }
  
  virtual const InputPortPtr &getInput(unsigned index) const override
  { return m_inputsByIndex.at(index); }
  
  virtual const InputPortPtr &getInput(const std::string &name) const override
  { return m_inputsByName.at(name); }

  virtual unsigned getOutputCount() const override
  { return m_outputsByIndex.size(); }
  
  virtual const OutputPortPtr &getOutput(unsigned index) const override
  { return m_outputsByIndex.at(index); }
  
  virtual const OutputPortPtr &getOutput(const std::string &name) const override
  { return m_outputsByName.at(name); }
};
typedef std::shared_ptr<DeviceType> DeviceTypePtr;



void loadGraph(xmlpp::Element *parent, GraphLoadEvents *events)
{
  auto *eGraph=find_single(parent, "./Graph");
  if(eGraph==0)
    throw std::runtime_error("No graph element.");

  std::string graphId=get_attribute_required(eGraph, "id");

  TypedDataSpecPtr graphPropertiesSpec=TypedDataSpec::lookupTypedDataSpec(graphId+"_properties_t");
  TypedDataPtr graphProperties; // Null by default

  auto *eGraphProperties=find_single(eGraph, "./Properties");
  if(eGraphProperties){
    graphProperties=graphPropertiesSpec->load(eGraphProperties);
  }

  auto *eEdgeTypes=find_single(eGraph, "./EdgeTypes");
  if(eEdgeTypes==0)
    throw std::runtime_error("No EdgeTypes element.");

  for(auto *nEdgeType : eEdgeTypes->find("./EdgeType")){
    xmlpp::Element *eEdgeType=(xmlpp::Element*)nEdgeType;
    std::string id=get_attribute_required(eEdgeType, "id");
    
    EdgeTypePtr et=EdgeType::lookupEdgeType(id);

    events->onEdgeType(et);
  }
}

#endif
