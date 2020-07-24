#ifndef graph_provider_helpers_hpp
#define graph_provider_helpers_hpp

#include "graph.hpp"

#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <ctype.h>
#include <functional>

#include <dlfcn.h>

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"


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

bool get_attribute_optional_bool(xmlpp::Element *eParent, const char *name)
{
  auto a=get_attribute_optional(eParent, name);
  if(a.empty()){
    fprintf(stderr, " Empty\n");
    return false;
  }
  fprintf(stderr, "a = '%s'\n", a.c_str());
  if(a=="true" || a=="1"){
    return true;
  }
  if(a=="false" || a=="0"){
    return false;
  }
  throw std::runtime_error("Couldn't interpret '"+a+"' as a bool.");
}

template<class T>
const T *cast_typed_properties(const typed_data_t *properties)
{  return static_cast<const T *>(properties); }

template<class T>
T *cast_typed_data(typed_data_t *data)
{  return static_cast<T *>(data); }



// Write a round-trippable number
std::string float_to_string(float x)
{
  // https://randomascii.wordpress.com/2012/03/08/float-precisionfrom-zero-to-100-digits-2/
  char buffer[16]={0};
  if(snprintf(buffer, 15, "%.9g", x) < 0){
    throw std::runtime_error("snprintf failed"); // get rid of g++ 8 warning
  }
  return buffer;
}

std::string float_to_string(double x)
{
  // https://randomascii.wordpress.com/2012/03/08/float-precisionfrom-zero-to-100-digits-2/
  char buffer[32]={0};
  if(snprintf(buffer, 31, "%.17g", x) < 0){
    throw std::runtime_error("snprintf failed"); // get rid of g++ 8 warning
  }
  return buffer;
}


class TypedDataSpecImpl
  : public TypedDataSpec
{
private:
  TypedDataSpecElementTuplePtr m_type;
  unsigned m_payloadSize;
  unsigned m_totalSize;

  std::vector<char> m_default;

public:
  TypedDataSpecImpl(TypedDataSpecElementTuplePtr elt=makeTuple("_", {}))
    : m_type(elt)
    , m_payloadSize(elt->getPayloadSize())
    , m_totalSize(elt->getPayloadSize()+sizeof(typed_data_t))
  {
    m_default.resize(m_payloadSize);
    m_type->createBinaryDefault(m_default.empty() ? nullptr : &m_default[0], m_payloadSize);
  }

  //! Gets the detailed type of the data spec
  TypedDataSpecElementTuplePtr getTupleElement() const override
  {
    return m_type;
  }

  //! Size of the actual content, not including typed_data_t header
  virtual size_t payloadSize() const override
  { return m_type->getPayloadSize(); }

  //! Size of the entire typed_data_t instance, including standard header
  virtual size_t totalSize() const override
  { return m_totalSize; }

  virtual TypedDataPtr create() const override
  {
    typed_data_t *p=(typed_data_t*)malloc(m_totalSize);
    p->_ref_count=0;
    p->_total_size_bytes=m_totalSize;

    memcpy(((char*)p)+sizeof(typed_data_t), &m_default[0], m_payloadSize);

    return TypedDataPtr(p);
  }

  virtual bool is_default(const TypedDataPtr &v) const override
  {
    if(!v) return true;
    assert(v.payloadSize()==m_payloadSize);
    return 0==memcmp(v.payloadPtr(), &m_default[0], m_payloadSize);
  }

  virtual TypedDataPtr load(xmlpp::Element *elt) const override
  {
    TypedDataPtr res=create();
    if(elt){
      std::string text="{"+elt->get_child_text()->get_content()+"}";
      rapidjson::Document document;
      document.Parse(text.c_str());
      assert(document.IsObject());

      m_type->JSONToBinary(document, (char*)res.payloadPtr(), m_payloadSize);
    }
    return res;
  }

  virtual void save(xmlpp::Element *parent, const TypedDataPtr &data) const override
  {
    parent->set_child_text(toJSON(data));
  }

  virtual std::string toJSON(const TypedDataPtr &data) const override
  {
    rapidjson::Document doc;
    auto v=m_type->binaryToJSON((char*)data.payloadPtr(), data.payloadSize(), doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    v.Accept(writer);
    return std::string(buffer.GetString());
  }

  virtual void addDataHash(const TypedDataPtr &data, POETSHash &hash) const
  {
    throw std::runtime_error("addDataHash - Not implemented for dynamic types yet.");
  }

  std::string toXmlV4ValueSpec(const TypedDataPtr &data, int minorFormatVersion=0) const override
  {
    std::stringstream acc;
    m_type->binaryToXmlV4Value((const char *)data.payloadPtr(), data.payloadSize(), acc, minorFormatVersion);
    return acc.str();
  }

  TypedDataPtr loadXmlV4ValueSpec(const std::string &value, int minorFormatVersion=0) const override
  {
    std::stringstream src(value);
    TypedDataPtr res=create();
    m_type->xmlV4ValueToBinary(src, (char *)res.payloadPtr(), res.payloadSize(), true, minorFormatVersion);
    return res;
  }

};

class InputPinImpl
  : public InputPin
{
private:
  std::function<DeviceTypePtr ()> m_deviceTypeSrc; // Avoid circular initialisation with device type
  mutable std::weak_ptr<DeviceType> m_deviceType;
  std::string m_name;
  unsigned m_index;
  MessageTypePtr m_messageType;
  TypedDataSpecPtr m_propertiesType;
  TypedDataSpecPtr m_stateType;
  std::string m_code;

  rapidjson::Document m_metadata;
protected:
  InputPinImpl(
    std::function<DeviceTypePtr ()> deviceTypeSrc,
    const std::string &name,
    unsigned index,
    MessageTypePtr messageType,
    TypedDataSpecPtr propertiesType,
    TypedDataSpecPtr stateType,
    const std::string &code
  )
    : m_deviceTypeSrc(deviceTypeSrc)
    , m_name(name)
    , m_index(index)
    , m_messageType(messageType)
    , m_propertiesType(propertiesType)
    , m_stateType(stateType)
    , m_code(code)
  {
    m_metadata.SetObject();
  }
public:

  virtual DeviceTypePtr getDeviceType() const override
  {
    DeviceTypePtr res=m_deviceType.lock();
    if(!res){
      res=m_deviceTypeSrc();
      m_deviceType=res;
    }
    return res;
  }

  virtual const std::string &getName() const override
  { return m_name; }

  virtual unsigned getIndex() const override
  { return m_index; }

  virtual const MessageTypePtr &getMessageType() const override
  { return m_messageType; }

  virtual const TypedDataSpecPtr &getPropertiesSpec() const override
  { return m_propertiesType; }

  virtual const TypedDataSpecPtr &getStateSpec() const override
  { return m_stateType; }

  virtual const std::string &getHandlerCode() const override
  { return m_code; }

  virtual rapidjson::Document &getMetadata() override
  { return m_metadata; }
};


class InputPinDelegate
  : public InputPin
{
private:
  InputPinPtr m_base;
protected:
  InputPinDelegate(
    InputPinPtr base
  )
    : m_base(base)
  {  }
public:
  InputPinPtr getBase() const
  { return m_base; }

  virtual DeviceTypePtr getDeviceType() const override
  { return m_base->getDeviceType(); }

  virtual const std::string &getName() const override
  { return m_base->getName(); }

  virtual unsigned getIndex() const override
  { return m_base->getIndex(); }

  virtual const MessageTypePtr &getMessageType() const override
  { return m_base->getMessageType(); }

  virtual const TypedDataSpecPtr &getPropertiesSpec() const override
  { return m_base->getPropertiesSpec(); }

  virtual const TypedDataSpecPtr &getStateSpec() const override
  { return m_base->getStateSpec(); }

  virtual const std::string &getHandlerCode() const override
  { return m_base->getHandlerCode(); }

  virtual rapidjson::Document &getMetadata() override
  { return m_base->getMetadata(); }
};


class OutputPinImpl
  : public OutputPin
{
private:
  std::function<DeviceTypePtr ()> m_deviceTypeSrc; // Avoid circular initialisation with device type
  mutable std::weak_ptr<DeviceType> m_deviceType;
  std::string m_name;
  unsigned m_index;
  MessageTypePtr m_messageType;
  std::string m_code;
  bool m_isIndexedSend;
  

  rapidjson::Document m_metadata;
protected:
  OutputPinImpl(std::function<DeviceTypePtr ()> deviceTypeSrc, const std::string &name, unsigned index, MessageTypePtr messageType, const std::string &code, bool isIndexedSend)
    : m_deviceTypeSrc(deviceTypeSrc)
    , m_name(name)
    , m_index(index)
    , m_messageType(messageType)
    , m_code(code)
    , m_isIndexedSend(isIndexedSend)
  {
    m_metadata.SetObject();
  }
public:
  virtual DeviceTypePtr getDeviceType() const override
  {
    DeviceTypePtr res=m_deviceType.lock();
    if(!res){
      res=m_deviceTypeSrc();
      m_deviceType=res;
    }
    return res;
  }

  virtual const std::string &getName() const override
  { return m_name; }

  virtual unsigned getIndex() const override
  { return m_index; }

  virtual const MessageTypePtr &getMessageType() const override
  { return m_messageType; }

  virtual const std::string &getHandlerCode() const override
  { return m_code; }

  virtual rapidjson::Document &getMetadata() override
  { return m_metadata; }

  virtual bool isIndexedSend() const override
  { return m_isIndexedSend; }
};

class OutputPinDelegate
  : public OutputPin
{
private:
  OutputPinPtr m_base;
protected:
  OutputPinDelegate(OutputPinPtr base)
    : m_base(base)
  {}
public:
  OutputPinPtr getBase() const
  { return m_base; }

  virtual DeviceTypePtr getDeviceType() const override
  { return m_base->getDeviceType();  }

  virtual const std::string &getName() const override
  { return m_base->getName(); }

  virtual unsigned getIndex() const override
  { return m_base->getIndex(); }

  virtual const MessageTypePtr &getMessageType() const override
  { return m_base->getMessageType(); }

  virtual const std::string &getHandlerCode() const override
  { return m_base->getHandlerCode(); }

  virtual rapidjson::Document &getMetadata() override
  { return m_base->getMetadata(); }

  virtual bool isIndexedSend() const override
  { return m_base->isIndexedSend(); }
};

class MessageTypeImpl
  : public MessageType
{
private:
  std::string m_id;
  TypedDataSpecPtr m_message;

  rapidjson::Document m_metadata;
public:
  MessageTypeImpl(const std::string &id, TypedDataSpecPtr message)
    : m_id(id)
    , m_message(message)
  {
    m_metadata.SetObject();
  }

  virtual const std::string &getId() const override
  { return m_id; }


  virtual const TypedDataSpecPtr &getMessageSpec() const override
  { return m_message; }

  virtual rapidjson::Document &getMetadata() override
  { return m_metadata; }
};


class DeviceTypeImpl
  : public DeviceType
{
private:
  std::string m_id;

  TypedDataSpecPtr m_properties;
  TypedDataSpecPtr m_state;

  std::string m_readyToSendCode;
  std::string m_onInitCode;
  std::string m_onHardwareIdleCode;
  std::string m_onDeviceIdleCode;
  std::string m_sharedCode;

  bool m_isExternal;

  std::vector<InputPinPtr> m_inputsByIndex;
  std::map<std::string,InputPinPtr> m_inputsByName;

  std::vector<OutputPinPtr> m_outputsByIndex;
  std::map<std::string,OutputPinPtr> m_outputsByName;

  rapidjson::Document m_metadata;

protected:
  DeviceTypeImpl(const std::string &id,
      TypedDataSpecPtr properties, TypedDataSpecPtr state,
      const std::vector<InputPinPtr> &inputs, const std::vector<OutputPinPtr> &outputs,
      bool isExternal,
      std::string readyToSendCode=std::string(),
      std::string onInitCode=std::string(),
      std::string sharedCode=std::string(),
      std::string onHardwareIdleCode=std::string(),
      std::string onDeviceIdleCode=std::string()
  )
    : m_id(id)
    , m_properties(properties)
    , m_state(state)
    , m_readyToSendCode(readyToSendCode)
    , m_onInitCode(onInitCode)
    , m_onHardwareIdleCode(onHardwareIdleCode)
    , m_onDeviceIdleCode(onDeviceIdleCode)
    , m_sharedCode(sharedCode)
    , m_isExternal(isExternal)
    , m_inputsByIndex(inputs)
    , m_outputsByIndex(outputs)
  {
    m_metadata.SetObject();
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

  virtual bool isExternal() const override
  { return m_isExternal; }

  virtual const std::string &getReadyToSendCode() const override
  { return m_readyToSendCode; }

  virtual const std::string &getOnInitCode() const override
  { return m_onInitCode; }

  virtual const std::string &getOnHardwareIdleCode() const override
  { return m_onHardwareIdleCode; }

  virtual const std::string &getOnDeviceIdleCode() const override
  { return m_onDeviceIdleCode; }


  virtual const std::string &getSharedCode() const override
  { return m_sharedCode; }

  virtual unsigned getInputCount() const override
  { return m_inputsByIndex.size(); }

  virtual const InputPinPtr &getInput(unsigned index) const override
  { return m_inputsByIndex.at(index); }

  virtual InputPinPtr getInput(const std::string &name) const override
  {
    auto it=m_inputsByName.find(name);
    if(it==m_inputsByName.end())
      return InputPinPtr();
    return it->second;
  }

  virtual const std::vector<InputPinPtr> &getInputs() const override
  { return m_inputsByIndex; }

  virtual unsigned getOutputCount() const override
  { return m_outputsByIndex.size(); }

  virtual const OutputPinPtr &getOutput(unsigned index) const override
  { return m_outputsByIndex.at(index); }

  virtual OutputPinPtr getOutput(const std::string &name) const override
  {
    auto it=m_outputsByName.find(name);
    if(it==m_outputsByName.end())
      return OutputPinPtr();
    return it->second;
  }

  virtual const std::vector<OutputPinPtr> &getOutputs() const override
  { return m_outputsByIndex; }

  virtual void init(
           OrchestratorServices *orchestrator,
           const typed_data_t *graphProperties,
           const typed_data_t *deviceProperties,
           typed_data_t *deviceState
           ) const
  {
    // No implementation
  }

  virtual rapidjson::Document &getMetadata() override
  { return m_metadata; }
};
typedef std::shared_ptr<DeviceType> DeviceTypePtr;


class DeviceTypeDelegate
  : public DeviceType
{
private:
  DeviceTypePtr m_base;

protected:
  DeviceTypeDelegate(DeviceTypePtr base)
    : m_base(base)
  {}

public:
  virtual const std::string &getId() const override
  { return m_base->getId(); }

  virtual const TypedDataSpecPtr &getPropertiesSpec() const override
  { return m_base->getPropertiesSpec(); }

  virtual const TypedDataSpecPtr &getStateSpec() const override
  { return m_base->getStateSpec(); }

  virtual bool isExternal() const override
  { return m_base->isExternal(); }

  virtual const std::string &getReadyToSendCode() const override
  { return m_base->getReadyToSendCode(); }

  virtual const std::string &getOnInitCode() const override
  { return m_base->getOnInitCode(); }

  virtual const std::string &getOnHardwareIdleCode() const override
  { return m_base->getOnHardwareIdleCode(); }

  virtual const std::string &getOnDeviceIdleCode() const override
  { return m_base->getOnDeviceIdleCode(); }

  virtual const std::string &getSharedCode() const override
  { return m_base->getSharedCode(); }

  virtual unsigned getInputCount() const override final
  { return getInputs().size(); }

  virtual const InputPinPtr &getInput(unsigned index) const override final
  { return getInputs().at(index); }

    virtual unsigned getOutputCount() const override final
  { return getOutputs().size(); }

  virtual const OutputPinPtr &getOutput(unsigned index) const override final
  { return getOutputs().at(index); }


  virtual InputPinPtr getInput(const std::string &name) const override
  { return m_base->getInput(name); }

  virtual const std::vector<InputPinPtr> &getInputs() const override
  { return m_base->getInputs(); }

  virtual OutputPinPtr getOutput(const std::string &name) const override
  { return m_base->getOutput(name); }

  virtual const std::vector<OutputPinPtr> &getOutputs() const override
  { return m_base->getOutputs(); }

  virtual rapidjson::Document &getMetadata() override
  { return m_base->getMetadata(); }
};


class GraphTypeImpl : public GraphType
{
private:
  std::string m_id;

  unsigned m_nativeDimension;

  TypedDataSpecPtr m_propertiesSpec;

  std::vector<MessageTypePtr> m_messageTypesByIndex;
  std::unordered_map<std::string,MessageTypePtr> m_messageTypesById;

  std::vector<DeviceTypePtr> m_deviceTypesByIndex;
  std::unordered_map<std::string,DeviceTypePtr> m_deviceTypesById;

  std::string m_sharedCode;

  rapidjson::Document m_metadata;

  in_proc_external_main_t m_in_proc_external_main;
protected:
  GraphTypeImpl(std::string id, TypedDataSpecPtr propertiesSpec)
    : m_id(id)
    , m_propertiesSpec(propertiesSpec)
  {
    m_metadata.SetObject();
  }

  void addInProcExternalMain(  in_proc_external_main_t in_proc_external_main)
  {
    m_in_proc_external_main=in_proc_external_main;
  }

  void addSharedCode(const std::string &code)
  {
    m_sharedCode=m_sharedCode+code;
  }

  void addMessageType(MessageTypePtr et)
  {
    if(m_messageTypesById.find(et->getId())!=m_messageTypesById.end()){
      throw std::runtime_error("Duplicate message type '"+et->getId()+"'");
    }
    m_messageTypesByIndex.push_back(et);
    m_messageTypesById[et->getId()]=et;
  }

  void addDeviceType(DeviceTypePtr et)
  {
    if(m_deviceTypesById.find(et->getId())!=m_deviceTypesById.end()){
      throw std::runtime_error("Duplicate device type '"+et->getId()+"'");
    }
    m_deviceTypesByIndex.push_back(et);
    m_deviceTypesById[et->getId()]=et;
  }

public:
  const std::string &getId() const override
  { return m_id; }

  const TypedDataSpecPtr getPropertiesSpec() const override
  { return m_propertiesSpec; }

  virtual unsigned getDeviceTypeCount() const override
  { return m_deviceTypesByIndex.size(); }

  virtual const DeviceTypePtr &getDeviceType(unsigned index) const override
  { return m_deviceTypesByIndex.at(index); }

  virtual const DeviceTypePtr &getDeviceType(const std::string &name) const override
  {
    auto it=m_deviceTypesById.find(name);
    if(it==m_deviceTypesById.end())
      throw std::runtime_error("Couldn't find device type called '"+name+"' in graph type '"+m_id+"'");
    return it->second;
  }

  virtual const std::vector<DeviceTypePtr> &getDeviceTypes() const override
  { return m_deviceTypesByIndex; }

  virtual unsigned getMessageTypeCount() const override
  { return m_messageTypesByIndex.size(); }

  virtual const MessageTypePtr &getMessageType(unsigned index) const override
  { return m_messageTypesByIndex.at(index); }

  virtual const MessageTypePtr &getMessageType(const std::string &name) const override
  { return m_messageTypesById.at(name); }

  virtual const std::vector<MessageTypePtr> &getMessageTypes() const override
  { return m_messageTypesByIndex; }

  virtual const std::string &getSharedCode() const override
  {
    return m_sharedCode;
  }

  virtual rapidjson::Document &getMetadata() override
  { return m_metadata; }

  virtual in_proc_external_main_t getDefaultInProcExternalProcedure() override
  {
    return m_in_proc_external_main;
  }


};


class OrchestratorServicesImpl
  : public OrchestratorServices
{
private:
  FILE *m_dst;
  std::string m_prefix;
  const char *m_device;
  const char *m_input;
  std::function<void (const char *,int)> m_onApplicationExit;
  std::function<void (const char *,uint32_t,bool,const char *)> m_onCheckpoint;
public:
  OrchestratorServicesImpl(
    const char *prefix,
    unsigned logLevel, FILE *dst, const char *device, const char *input,
    std::function<void (const char *,int)> onApplicationExit,
    std::function<void (const char *,uint32_t,bool,const char *)> onCheckpoint
  )
    : OrchestratorServices(logLevel)
    , m_dst(dst)
    , m_prefix(prefix)
    , m_device(device)
    , m_input(input)
    , m_onApplicationExit(onApplicationExit)
    , m_onCheckpoint(onCheckpoint)
  {}

  static void onCheckpointDefault(const char *,uint32_t,bool,const char *)
  {
    // Do nothing
  }

  void setPrefix(const char *prefix)
  {
    m_prefix=prefix;
  }

  void setDevice(const char *device, const char *input)
  {
    m_device=device;
    m_input=input;
  }

  virtual void vlog(unsigned level, const char *msg, va_list args) override
  {
    if(m_logLevel >= level){
      fprintf(m_dst, "%s%s:%s : ", m_prefix.c_str(), m_device, m_input);
      vfprintf(m_dst, msg, args);
      fprintf(m_dst, "\n");
    }
    if(!strcmp("_HANDLER_EXIT_FAIL_9be65737_", msg)){
      m_onApplicationExit("handler_exit(1)", 1);
    }
    if(!strcmp("_HANDLER_EXIT_SUCCESS_9be65737_", msg)){
      m_onApplicationExit("handler_exit(0)", 0);
    }
  }

  virtual void vcheckpoint(bool preEvent, int level, const char *fmt, va_list args) override
  {
    char buffer[256]={0};
    unsigned p=vsnprintf(buffer, sizeof(buffer)-1, fmt, args);
    if(p>=sizeof(buffer)-2){
      throw std::runtime_error("vcheckpoint buffer overflow");
    }
    m_onCheckpoint(m_device, preEvent, level, buffer);
  }


  virtual void application_exit(int code)
  {
    m_onApplicationExit(m_device, code);
  }
};


class ReceiveOrchestratorServicesImpl
  : public OrchestratorServicesImpl
{
public:
  ReceiveOrchestratorServicesImpl(
    unsigned logLevel, FILE *dst, const char *device, const char *input,
    std::function<void (const char *,int)> onApplicationExit,
    std::function<void (const char *,uint32_t,bool,const char *)> onCheckpoint = onCheckpointDefault
  )
    : OrchestratorServicesImpl(
        "Recv : ",
        logLevel,dst,device,input,
        onApplicationExit,onCheckpoint
    )
  {}
};

// TODO : Fold the services into one class. No reason for separation
class SendOrchestratorServicesImpl
  : public OrchestratorServicesImpl
{
public:
  SendOrchestratorServicesImpl(
    unsigned logLevel, FILE *dst, const char *device, const char *input,
    std::function<void (const char *,int)> onApplicationExit,
    std::function<void (const char *,uint32_t,bool,const char *)> onCheckpoint = onCheckpointDefault
  )
    : OrchestratorServicesImpl(
        "Send : ",
        logLevel,dst,device,input,
        onApplicationExit,onCheckpoint
    )
    {}
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
    if(m_services && (m_services->getLogLevel() >= level)){ // Allows call to be avoided out on client side
      va_list args;
      va_start(args, msg);
      m_services->vlog(level, msg, args);
      va_end(args);
    }
  }
};

class provider_assertion_error
  : public std::runtime_error
{
private:
  const char *m_file;
  int m_line;
  const char *m_assertFunc;
  const char *m_cond;

  static std::string make_what(const char *file, int line, const char *assertFunc,const char *cond)
  {
    std::stringstream tmp;
    tmp<<file<<":"<<line<<": "<<assertFunc<<" Assertion `"<<cond<<"' failed.";
    return tmp.str();
  }
public:
  provider_assertion_error(const char *file, int line, const char *assertFunc,const char *cond)
    : std::runtime_error(make_what(file,line,assertFunc,cond))
    , m_file(file)
    , m_line(line)
    , m_assertFunc(assertFunc)
    , m_cond(cond)
  {

  }
};

void handler_assert_func (const char *file, int line, const char *assertFunc,const char *cond)
{
  throw provider_assertion_error(file,line,assertFunc,cond);
}

#ifndef NDEBUG
#define handler_assert(cond) if(!(cond)){ handler_assert_func(__FILE__,__LINE__,__FUNCTION__,#cond); }
#else
#define handler_assert(cond) (void)0
#endif

class unknown_graph_type_error
  : public std::runtime_error
{
private:
  std::string m_graphId;
public:
  unknown_graph_type_error(const std::string &graphId)
    : std::runtime_error("Couldn't find graph type with id '"+graphId+"'")
    , m_graphId(graphId)
  {}
};

class RegistryImpl
  : public Registry
{
private:
  std::unordered_map<std::string,GraphTypePtr> m_graphs;
  std::unordered_map<std::string,MessageTypePtr> m_edges;
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
  RegistryImpl(bool disableImplicitLoad=false)
    : m_soExtension(".graph.so")
  {
    if(!disableImplicitLoad){
      const char * searchPath=getenv("POETS_PROVIDER_PATH");
      if(searchPath){
        recurseLoad(searchPath);
      }else{
        std::shared_ptr<char> cwd;
        cwd.reset(getcwd(0,0), free);
          recurseLoad(cwd.get()+std::string("/providers"));
      }
    }
  }

  void loadProvider(const std::string &path)
  {
    fprintf(stderr, "Loading provider '%s'\n", path.c_str());
    void *lib=dlopen(path.c_str(), RTLD_NOW|RTLD_LOCAL);
    if(lib==0)
      throw std::runtime_error("Couldn't load provider '"+path+" + '"+dlerror()+"'");

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
      throw unknown_graph_type_error(id);

    }
    return it->second;
  }

  virtual void registerMessageType(MessageTypePtr edge) override
  {  m_edges.insert(std::make_pair(edge->getId(), edge)); }

  virtual MessageTypePtr lookupMessageType(const std::string &id) const override
  { return m_edges.at(id); }

  virtual void registerDeviceType(DeviceTypePtr dev) override
  { m_devices.insert(std::make_pair(dev->getId(), dev)); }

  virtual DeviceTypePtr lookupDeviceType(const std::string &id) const override
  { return m_devices.at(id); }
};

#endif
