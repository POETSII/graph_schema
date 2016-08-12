#ifndef graph_impl_hpp
#define graph_impl_hpp

#include "graph.hpp"

#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <ctype.h>

#include <dlfcn.h>

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>


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

std::string get_attribute_optional(xmlpp::Element *eParent, const char *name)
{
  auto a=eParent->get_attribute(name);
  if(a==0)
    return std::string();

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

  unsigned m_nativeDimension;

  TypedDataSpecPtr m_propertiesSpec;
  
  std::vector<EdgeTypePtr> m_edgeTypesByIndex;
  std::unordered_map<std::string,EdgeTypePtr> m_edgeTypesById;

  std::vector<DeviceTypePtr> m_deviceTypesByIndex;
  std::unordered_map<std::string,DeviceTypePtr> m_deviceTypesById;

protected:
  GraphTypeImpl(std::string id, unsigned nativeDimension, TypedDataSpecPtr propertiesSpec)
    : m_id(id)
    , m_nativeDimension(nativeDimension)
    , m_propertiesSpec(propertiesSpec)
  {}

  const std::string &getId() const override
  { return m_id; }

  unsigned getNativeDimension() const override
  { return m_nativeDimension; }

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

class ReceiveOrchestratorServicesImpl
  : public OrchestratorServices
{
private:
  unsigned m_logLevel;
  FILE *m_dst;
  std::string m_prefix;
  const char *m_device;
  const char *m_input;
public:
  ReceiveOrchestratorServicesImpl(unsigned logLevel, FILE *dst, const char *device, const char *input)
    : m_logLevel(logLevel)
    , m_dst(dst)
    , m_prefix("Send: ")
    , m_device(device)
    , m_input(input)
  {}
  
  virtual unsigned getLogLevel() const override
  { return m_logLevel; }

  void setPrefix(const char *prefix)
  {
    m_prefix=prefix;
  }

  void setReceiver(const char *device, const char *input)
  {
    m_device=device;
    m_input=input;
  }
  
  virtual void vlog(unsigned level, const char *msg, va_list args) override
  {
    if(m_logLevel >= level){
      fprintf(m_dst, "%sdevice:%s, input:%s : ", m_prefix.c_str(), m_device, m_input);
      vfprintf(m_dst, msg, args);
      fprintf(m_dst, "\n");
    }
  }
};

class SendOrchestratorServicesImpl
  : public OrchestratorServices
{
private:
  unsigned m_logLevel;
  FILE *m_dst;
  std::string m_prefix;
  const char *m_device;
  const char *m_output;
public:
  SendOrchestratorServicesImpl(unsigned logLevel, FILE *dst, const char *device, const char *output)
    : m_logLevel(logLevel)
    , m_dst(dst)
    , m_prefix("Send: ")
    , m_device(device)
    , m_output(output)
  {}

  void setPrefix(const std::string &prefix)
  {
    m_prefix=m_prefix;
  }

  void setSender(const char *device, const char *output)
  {
    m_device=device;
    m_output=output;
  }
  
  virtual unsigned getLogLevel() const override
  { return m_logLevel; }
  
  virtual void vlog(unsigned level, const char *msg, va_list args) override
  {
    if(m_logLevel >= level){
      fprintf(m_dst, "%sdevice:%s, output:%s : ", m_prefix.c_str(), m_device, m_output);
      vfprintf(m_dst, msg, args);
      fprintf(m_dst, "\n");
    }
  }
};

class HandlerLogImpl
{
private:
  OrchestratorServices *m_services;
public:
  HandlerLogImpl(OrchestratorServices *services)
    : m_services(services)
  {}

  void operator()(unsigned level, const char *msg, ...)
  {
    va_list args;
    va_start(args, msg);
    m_services->vlog(level, msg, args);
    va_end(args);
  }
};

class RegistryImpl
  : public Registry
{
private:
  std::unordered_map<std::string,GraphTypePtr> m_graphs;
  std::unordered_map<std::string,EdgeTypePtr> m_edges;
  std::unordered_map<std::string,DeviceTypePtr> m_devices;

  std::string m_soExtension;

  void recurseLoad(std::string path)
  {
    
    std::shared_ptr<DIR> hDir(opendir(path.c_str()), closedir);
    if(!hDir){
      fprintf(stderr, "Warning: Couldn't open provider='%s'\n", path.c_str());
    }else{

	struct dirent *de=0;
	
	while( (de=readdir(hDir.get())) ){
	  std::string part=de->d_name;
	  std::string fullPath=path+"/"+part;

	  //fprintf(stderr, "%s\n", fullPath.c_str());
	  
	  if(m_soExtension.size() < part.size()){	  
	    if(m_soExtension == part.substr(part.size()-m_soExtension.size())){
	      
	      loadProvider(fullPath);
	      continue;
	    }
	  }

	  if(part=="." || part=="..")
	    continue;

	  if(part.size() >= 5){
	    if(part.substr(part.size()-5)==".dSYM")
	      continue;
	  }
	  
	  struct stat ss;
	  if(0!=stat(fullPath.c_str(), &ss))
	    continue;
	  
	  if(S_ISDIR(ss.st_mode)){
	    recurseLoad(fullPath);
	  }
	}
    }
  }
public:
  RegistryImpl()
    : m_soExtension(".graph.so")
  {    
    const char * searchPath=getenv("POETS_PROVIDER_PATH");
    std::shared_ptr<char> cwd;
    if(searchPath==NULL){
      cwd.reset(getcwd(0,0), free);
      searchPath=cwd.get();
    }
    if(searchPath){
      recurseLoad(searchPath);
    }
  }
  
  void loadProvider(const std::string &path)
  {
    fprintf(stderr, "Loading provider '%s'\n", path.c_str());
    void *lib=dlopen(path.c_str(), RTLD_NOW|RTLD_LOCAL);
    if(lib==0)
      throw std::runtime_error("Couldn't load provider '"+path+"'");

    // Mangled name of the export. TODO : A bit fragile.
    void *entry=dlsym(lib, "registerGraphTypes");
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
  {
    auto it=m_graphs.find(id);
    if(it==m_graphs.end()){
      fprintf(stderr, "Couldn't find provide for '%s'", id.c_str());
      for(auto i : m_graphs){
	std::cerr<<"  "<<i.first<<" : "<<i.second<<"\n";
      }
      exit(1);
    }
    return it->second;
  }
  
  virtual void registerEdgeType(EdgeTypePtr edge) override 
  {  m_edges.insert(std::make_pair(edge->getId(), edge)); }
  
  virtual EdgeTypePtr lookupEdgeType(const std::string &id) const override
  { return m_edges.at(id); }

  virtual void registerDeviceType(DeviceTypePtr dev) override 
  { m_devices.insert(std::make_pair(dev->getId(), dev)); }
  
  virtual DeviceTypePtr lookupDeviceType(const std::string &id) const override
  { return m_devices.at(id); }
};

void split_path(const std::string &src, std::string &dstDevice, std::string &dstPort, std::string &srcDevice, std::string &srcPort)
{
  int colon1=src.find(':');
  int arrow=src.find('-',colon1+1);
  int colon2=src.find(':',arrow+1);

  if(colon1==-1 || arrow==-1 || colon2==-1)
    throw std::runtime_error("malformed path");

  dstDevice=src.substr(0,colon1);
  dstPort=src.substr(colon1+1,arrow-colon1-1);
  srcDevice=src.substr(arrow+1,colon2-arrow-1);
  srcPort=src.substr(colon2+1);
}


void loadGraph(Registry *registry, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="http://TODO.org/POETS/virtual-graph-schema-v0";
  
  auto *eGraph=find_single(parent, "./g:GraphInstance", ns);
  if(eGraph==0)
    throw std::runtime_error("No graph element.");
  
  std::string graphId=get_attribute_required(eGraph, "id");
  std::string graphTypeId=get_attribute_required(eGraph, "graphTypeId");

  auto graphType=registry->lookupGraphType(graphTypeId);

  for(auto et : graphType->getEdgeTypes()){
    events->onEdgeType(et);
  }
  for(auto dt : graphType->getDeviceTypes()){
    events->onDeviceType(dt);
  }
  events->onGraphType(graphType);

  TypedDataPtr graphProperties;
  auto *eProperties=find_single(eGraph, "./g:Properties", ns);
  if(eProperties){
    fprintf(stderr, "Loading properties\n");
    graphProperties=graphType->getPropertiesSpec()->load(eProperties);
  }else{
    fprintf(stderr, "Default constructing properties.\n");
    graphProperties=graphType->getPropertiesSpec()->create();
  }

  auto gId=events->onGraphInstance(graphType, graphId, graphProperties);

  std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> > devices;

  auto *eDeviceInstances=find_single(eGraph, "./g:DeviceInstances", ns);
  if(!eDeviceInstances)
    throw std::runtime_error("No DeviceInstances element");
  
  for(auto *nDevice : eDeviceInstances->find("./g:DevI", ns)){
    auto *eDevice=(xmlpp::Element *)nDevice;

    std::string id=get_attribute_required(eDevice, "id");
    std::string deviceTypeId=get_attribute_required(eDevice, "type");

    std::vector<double> nativeLocation;
    const double *nativeLocationPtr = 0;
    std::string nativeLocationStr=get_attribute_optional(eDevice, "nativeLocation");
    if(!nativeLocationStr.empty()){
      size_t start=0;
      while(start<nativeLocationStr.size()){
	size_t end=nativeLocationStr.find(',',start);
	std::string part=nativeLocationStr.substr(start, end==std::string::npos ? end : end-start);
	nativeLocation.push_back(std::stod(part));
	if(end==std::string::npos)
	  break;

	start=end+1;
      }

      if(nativeLocation.size()!=graphType->getNativeDimension()){
	throw std::runtime_error("Device instance location does not match dimension of problem.");
      }

      nativeLocationPtr = &nativeLocation[0];
    }

    auto dt=graphType->getDeviceType(deviceTypeId);

    TypedDataPtr deviceProperties;
    auto *eProperties=find_single(eDevice, "./g:P", ns);
    if(eProperties){
      deviceProperties=dt->getPropertiesSpec()->load(eProperties);
    }else{
      deviceProperties=dt->getPropertiesSpec()->create();
    }

    uint64_t dId=events->onDeviceInstance(gId, dt, id, deviceProperties, nativeLocationPtr);

    devices.insert(std::make_pair( id, std::make_pair(dId, dt)));
  }

  auto *eEdgeInstances=find_single(eGraph, "./g:EdgeInstances", ns);
  if(!eEdgeInstances)
    throw std::runtime_error("No EdgeInstances element");

  // for(auto *nEdge : eEdgeInstances->find("./g:EdgeInstance", ns)){
  for(auto *nEdge : eEdgeInstances->get_children()){
    auto *eEdge=dynamic_cast<xmlpp::Element *>(nEdge);
    if(!eEdge)
      continue;
    if(eEdge->get_name()!="EdgeI")
      continue;

    std::string srcDeviceId, srcPortName, dstDeviceId, dstPortName;
    std::string path=get_attribute_optional(eEdge, "path");
    if(path.c_str()){
      split_path(path, dstDeviceId, dstPortName, srcDeviceId, srcPortName);
      //std::cerr<<srcDeviceId<<" "<<srcPortName<<" "<<dstDeviceId<<" "<<dstPortName<<"\n";
    }else{
      srcDeviceId=get_attribute_required(eEdge, "srcDeviceId");
      srcPortName=get_attribute_required(eEdge, "srcPortName");
      dstDeviceId=get_attribute_required(eEdge, "dstDeviceId");
      dstPortName=get_attribute_required(eEdge, "dstPortName");
    }

    auto &srcDevice=devices.at(srcDeviceId);
    auto &dstDevice=devices.at(dstDeviceId);
    auto srcPort=srcDevice.second->getOutput(srcPortName);
    auto dstPort=dstDevice.second->getInput(dstPortName);

    if(srcPort->getEdgeType()!=dstPort->getEdgeType())
      throw std::runtime_error("Edge type mismatch on ports.");

    auto et=srcPort->getEdgeType();

    

    TypedDataPtr edgeProperties;
    xmlpp::Element *eProperties=0;
    {
      const auto &children=eEdge->get_children();
      if(children.size()<10){
	for(const auto &nChild : children){
	  assert(nChild->getName().is_ascii());
	  
	  if(!strcmp(nChild->get_name().c_str(),"P")){
	    eProperties=(xmlpp::Element*)nChild;
	    break;
	  }
	}
      }else{
	eProperties=find_single(eEdge, "./g:P", ns);
      }
    }
    if(eProperties){
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
