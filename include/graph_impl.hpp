#ifndef graph_impl_hpp
#define graph_impl_hpp

#include "graph.hpp"

#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <iostream>

#include <dlfcn.h>

#include <sys/types.h>
#include <dirent.h>

template<class T>
const char *typed_data_attribute_node_type();

template<>
const char *typed_data_attribute_node_type<int32_t>()
{ return "Int32"; }

template<>
const char *typed_data_attribute_node_type<float>()
{ return "Float32"; }

template<>
const char *typed_data_attribute_node_type<bool>()
{ return "Bool"; }

const uint64_t FNV64_PRIME=1099511628211ull;
const uint64_t FNV64_OFFSET=14695981039346656037ull;

uint64_t fnv64_hash_byte(uint64_t hash, uint8_t data)
{
  // This is FNV-1a : http://www.isthe.com/chongo/tech/comp/fnv/index.html
  return (hash^data)*FNV64_PRIME;
}

uint64_t fnv64_hash_uint64(uint64_t hash, uint64_t data)
{
  for(unsigned i=0;i<8;i++){
    hash=(hash^(data&0xFF))*FNV64_PRIME;
    data=data>>8;
  }
  return hash;
}

template<class TNum>
uint64_t fnv64_hash_int(uint64_t hash, TNum data)
{
  // Handle the case where data is zero so that...
  if(data==0){
    return hash*FNV64_PRIME;
  }

  // ...we can use this (otherwise the compiler will whinge for unsigned types)
  if(data <= 0){
    return fnv64_hash_num(fnv64_hash_byte(hash, 0xCC), -(data+1));
  }
  
  while(data > 0){
    hash=fnv64_hash_byte(hash, data&0xFF);
    data=data>>8;
  }
  return hash;
}

uint64_t fnv64_hash_string(uint64_t hash, const char *data)
{
  while(*data){
    hash=fnv64_hash_byte(hash, (uint8_t)*data);
    data++;
  }
  return hash;
}

uint64_t fnv64_hash_combine(uint64_t hash, uint64_t data)
{
  for(unsigned i=0;i<8;i++){
    hash=fnv64_hash_byte(hash, data&0xFF);
    data=data>>8;
  }
  return hash;
}




template<class T>
void load_typed_data_attribute(T &dst, xmlpp::Element *parent, const char *name, const xmlpp::Node::PrefixNsMap &ns=xmlpp::Node::PrefixNsMap())
{
  auto all=parent->find(std::string("./*[@name='")+name+"']", ns);
  if(all.size()==0)
    return;
  if(all.size()>1)
    throw std::runtime_error("More than one property.");
  auto got=(xmlpp::Element*)all[0];
  if(got->get_name()!=typed_data_attribute_node_type<T>()){
    std::stringstream tmp;
    tmp<<"Wrong XML node type for "<<name<<", expected "<<typed_data_attribute_node_type<T>()<<", got "<<got->get_name();
    throw std::runtime_error(tmp.str());
  }
  xmlpp::Attribute *a=got->get_attribute("value");
  if(!a)
    return;
  std::stringstream tmp(a->get_value());
  tmp>>dst;
  std::cerr<<"  loaded: "<<a->get_value()<<" -> "<<dst<<"\n";
}

xmlpp::Element *find_single(xmlpp::Element *parent, const std::string &name, const xmlpp::Node::PrefixNsMap &ns=xmlpp::Node::PrefixNsMap())
{
  auto all=parent->find(name,ns);
  if(all.size()==0)
    return 0;
  if(all.size()>1)
    throw std::runtime_error("More than on matching element.");
  auto res=dynamic_cast<xmlpp::Element*>(all[0]);
  if(res==0)
    throw std::runtime_error("Path did not identify an element.");
  return res;
}

xmlpp::Element *load_typed_data_tuple(xmlpp::Element *parent, const std::string &name)
{
  return find_single(parent, std::string("./Tuple[@name='")+name+"']");
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
  
  virtual InputPortPtr getInput(const std::string &name) const override
  {
    auto it=m_inputsByName.find(name);
    if(it==m_inputsByName.end())
      return InputPortPtr();
    return it->second;
  }

  virtual const std::vector<InputPortPtr> &getInputs() const override
  { return m_inputsByIndex; }

  virtual unsigned getOutputCount() const override
  { return m_outputsByIndex.size(); }
  
  virtual const OutputPortPtr &getOutput(unsigned index) const override
  { return m_outputsByIndex.at(index); }
  
  virtual OutputPortPtr getOutput(const std::string &name) const override
  {
    auto it=m_outputsByName.find(name);
    if(it==m_outputsByName.end())
      return OutputPortPtr();
    return it->second;
  }

  virtual const std::vector<OutputPortPtr> &getOutputs() const override
  { return m_outputsByIndex; }
};
typedef std::shared_ptr<DeviceType> DeviceTypePtr;


class GraphTypeImpl : public GraphType
{
private:
  std::string m_id;

  TypedDataSpecPtr m_propertiesSpec;
  
  std::vector<EdgeTypePtr> m_edgeTypesByIndex;
  std::unordered_map<std::string,EdgeTypePtr> m_edgeTypesById;

  std::vector<DeviceTypePtr> m_deviceTypesByIndex;
  std::unordered_map<std::string,DeviceTypePtr> m_deviceTypesById;

protected:
  GraphTypeImpl(std::string id, TypedDataSpecPtr propertiesSpec)
    : m_id(id)
    , m_propertiesSpec(propertiesSpec)
  {}

  const std::string &getId() const override
  { return m_id; }

  const TypedDataSpecPtr getPropertiesSpec() const override
  { return m_propertiesSpec; }

  virtual unsigned getDeviceTypeCount() const override
  { return m_deviceTypesByIndex.size(); }
  
  virtual const DeviceTypePtr &getDeviceType(unsigned index) const override
  { return m_deviceTypesByIndex.at(index); }
  
  virtual const DeviceTypePtr &getDeviceType(const std::string &name) const override
  { return m_deviceTypesById.at(name); }
  
  virtual const std::vector<DeviceTypePtr> &getDeviceTypes() const override
  { return m_deviceTypesByIndex; }
  
  virtual unsigned getEdgeTypeCount() const override
  { return m_edgeTypesByIndex.size(); }
  
  virtual const EdgeTypePtr &getEdgeType(unsigned index) const override
  { return m_edgeTypesByIndex.at(index); }
  
  virtual const EdgeTypePtr &getEdgeType(const std::string &name) const override
  { return m_edgeTypesById.at(name); }
  
  virtual const std::vector<EdgeTypePtr> &getEdgeTypes() const override
  { return m_edgeTypesByIndex; }

  void addEdgeType(EdgeTypePtr et)
  {
    m_edgeTypesByIndex.push_back(et);
    m_edgeTypesById[et->getId()]=et;
  }

  void addDeviceType(DeviceTypePtr et)
  {
    m_deviceTypesByIndex.push_back(et);
    m_deviceTypesById[et->getId()]=et;
  }
};

class RegistryImpl
  : public Registry
{
private:
  std::unordered_map<std::string,GraphTypePtr> m_graphs;
  std::unordered_map<std::string,EdgeTypePtr> m_edges;
  std::unordered_map<std::string,DeviceTypePtr> m_devices;
  
public:
  RegistryImpl()
  {
    // TODO : Do we want a better name for this?
    std::string soExtension=".graph.so";
    
    auto searchPath=getenv("POETS_PROVIDER_PATH");
    if(searchPath){
      DIR *hDir=opendir(searchPath);
      if(!hDir){
	fprintf(stderr, "Warning: Couldn't open POETS_PROVIDER_PATH='%s'", searchPath);
      }else{
	try{
	  struct dirent *de=0;

	  while( (de=readdir(hDir)) ){
	    std::string path=de->d_name;
	    if(soExtension.size() > path.size())
	      continue;
	    
	    if(soExtension != path.substr(path.size()-soExtension.size()))
	      continue;

	    std::string fullPath=std::string(searchPath)+"/"+path;

	    loadProvider(fullPath);
	    
	  }
	  
	}catch(...){
	  closedir(hDir);
	  throw;
	}
      }
    }
  }
  
  void loadProvider(const std::string &path)
  {
    fprintf(stderr, "Loading provider '%s'\n", path.c_str());
    void *lib=dlopen(path.c_str(), RTLD_NOW|RTLD_LOCAL);
    if(lib==0)
      throw std::runtime_error("Couldn't load provider '"+path+"'");

    // Mangled name of the export. TODO : A bit fragile.
    void *entry=dlsym(lib, "_Z18registerGraphTypesP8Registry");
    if(entry==0)
      throw std::runtime_error("Couldn't find registerGraphTypes entry point.");

    typedef void (*entry_func_t)(Registry *);
    
    auto entryFunc=(entry_func_t)(entry);
    entryFunc(this);
  }
  
  virtual void registerGraphType(GraphTypePtr graph) override
  {
    fprintf(stderr, "  registerGraphType(%s)\n", graph->getId().c_str());
    m_graphs.insert(std::make_pair(graph->getId(), graph));
  }
  
  virtual GraphTypePtr lookupGraphType(const std::string &id) const override
  { return m_graphs.at(id); }
  
  virtual void registerEdgeType(EdgeTypePtr edge) override 
  {  m_edges.insert(std::make_pair(edge->getId(), edge)); }
  
  virtual EdgeTypePtr lookupEdgeType(const std::string &id) const override
  { return m_edges.at(id); }

  virtual void registerDeviceType(DeviceTypePtr dev) override 
  { m_devices.insert(std::make_pair(dev->getId(), dev)); }
  
  virtual DeviceTypePtr lookupDeviceType(const std::string &id) const override
  { return m_devices.at(id); }
};



void loadGraph(Registry *registry, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="http://TODO.org/POETS/virtual-graph-schema-v0";
  
  fprintf(stderr, "loadGraph begin\n");
  
  auto *eGraph=find_single(parent, "./g:GraphInstance", ns);
  if(eGraph==0)
    throw std::runtime_error("No graph element.");
  
  std::string graphId=get_attribute_required(eGraph, "id");
  std::string graphTypeId=get_attribute_required(eGraph, "graphTypeId");

  fprintf(stderr, "graphId = %s, graphTypeId = %s\n", graphId.c_str(), graphTypeId.c_str());

  auto graphType=registry->lookupGraphType(graphTypeId);

  for(auto et : graphType->getEdgeTypes()){
    events->onEdgeType(et);
  }
  for(auto dt : graphType->getDeviceTypes()){
    events->onDeviceType(dt);
  }
  events->onGraphType(graphType);

  fprintf(stderr, "Pre properties\n");
  TypedDataPtr graphProperties;
  auto *eProperties=find_single(eGraph, "./g:Properties", ns);
  if(eProperties){
    fprintf(stderr, "Loading properties\n");
    graphProperties=graphType->getPropertiesSpec()->load(eProperties);
  }else{
    fprintf(stderr, "Default constructing properties.\n");
    graphProperties=graphType->getPropertiesSpec()->create();
  }
  fprintf(stderr, "Post properties\n");


  auto gId=events->onGraphInstance(graphType, graphId, graphProperties);

  std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> > devices;

  auto *eDeviceInstances=find_single(eGraph, "./g:DeviceInstances", ns);
  if(!eDeviceInstances)
    throw std::runtime_error("No DeviceInstances element");
  
  for(auto *nDevice : eDeviceInstances->find("./g:DeviceInstance", ns)){
    auto *eDevice=(xmlpp::Element *)nDevice;

    std::string id=get_attribute_required(eDevice, "id");
    std::string deviceTypeId=get_attribute_required(eDevice, "deviceTypeId");

    auto dt=graphType->getDeviceType(deviceTypeId);

    TypedDataPtr deviceProperties;
    auto *eProperties=find_single(eDevice, "./g:Properties", ns);
    if(eProperties){
      fprintf(stderr, "Loading device properties\n");
      deviceProperties=dt->getPropertiesSpec()->load(eProperties);
    }else{
      fprintf(stderr, "Default cosntructing device properties\n");
      deviceProperties=dt->getPropertiesSpec()->create();
    }

    uint64_t dId=events->onDeviceInstance(gId, dt, id, deviceProperties);

    devices.insert(std::make_pair( id, std::make_pair(dId, dt)));
  }

  auto *eEdgeInstances=find_single(eGraph, "./g:EdgeInstances", ns);
  if(!eEdgeInstances)
    throw std::runtime_error("No EdgeInstances element");
  
  for(auto *nEdge : eEdgeInstances->find("./g:EdgeInstance", ns)){
    auto *eEdge=(xmlpp::Element *)nEdge;

    std::string srcDeviceId=get_attribute_required(eEdge, "srcDeviceId");
    std::string srcPortName=get_attribute_required(eEdge, "srcPortName");
    std::string dstDeviceId=get_attribute_required(eEdge, "dstDeviceId");
    std::string dstPortName=get_attribute_required(eEdge, "dstPortName");

    auto &srcDevice=devices.at(srcDeviceId);
    auto &dstDevice=devices.at(dstDeviceId);
    auto srcPort=srcDevice.second->getOutput(srcPortName);
    auto dstPort=dstDevice.second->getInput(dstPortName);

    if(srcPort->getEdgeType()!=dstPort->getEdgeType())
      throw std::runtime_error("Edge type mismatch on ports.");

    auto et=srcPort->getEdgeType();

    

    TypedDataPtr edgeProperties;
    auto *eProperties=find_single(eEdge, "./g:Properties", ns);
    if(eProperties){
      fprintf(stderr, "Loading properties\n");
      edgeProperties=et->getPropertiesSpec()->load(eProperties);
    }else{
      edgeProperties=et->getPropertiesSpec()->create();
    }


    events->onEdgeInstance(gId,
			   dstDevice.first, dstDevice.second, dstPort, 
			   srcDevice.first, srcDevice.second, srcPort,
			   edgeProperties);
  }
}

#endif
