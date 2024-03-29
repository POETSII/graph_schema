#ifndef graph_persist_dom_reader_v3_hpp
#define graph_persist_dom_reader_v3_hpp

#include "graph_persist.hpp"
#include "graph_provider_helpers.hpp"
#include "graph_compare.hpp"

//#include <boost/filesystem.hpp>
#include <libxml++/parsers/domparser.h>

#include <string>

namespace xml_v3
{

inline void split_path(const std::string &src, std::string &dstDevice, std::string &dstPin, std::string &srcDevice, std::string &srcPin)
{
  int colon1=src.find(':');
  int arrow=src.find('-',colon1+1);
  int colon2=src.find(':',arrow+1);

  if(colon1==-1 || arrow==-1 || colon2==-1)
    throw std::runtime_error("malformed path");

  dstDevice=src.substr(0,colon1);
  dstPin=src.substr(colon1+1,arrow-colon1-1);
  srcDevice=src.substr(arrow+1,colon2-arrow-1);
  srcPin=src.substr(colon2+1);
}


inline rapidjson::Document parse_meta_data(xmlpp::Element *parent, const char *name, xmlpp::Node::PrefixNsMap &ns)
{
  auto *eMetaData=find_single(parent, name, ns);
  if(eMetaData){
    std::string text="{"+eMetaData->get_child_text()->get_content()+"}";
    rapidjson::Document document;
    document.Parse(text.c_str());
    assert(document.IsObject());
    return document;
  }else{
    rapidjson::Document res;
    res.SetObject();
    return res;
  }
}


inline TypedDataSpecElementPtr loadTypedDataSpecElement(xmlpp::Element *eMember)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  std::string name=get_attribute_required(eMember, "name");

  if(eMember->get_name()=="Scalar"){
    std::string type=get_attribute_required(eMember, "type");
    std::string defaultValue=get_attribute_optional(eMember, "default");
    return makeScalar(name, type, defaultValue);

  }else if(eMember->get_name()=="Array"){
    std::string lengthS=get_attribute_required(eMember, "length");
    unsigned length=std::stoul(lengthS);

    TypedDataSpecElementPtr elt;
    std::string typeS=get_attribute_optional(eMember, "type");
    if(!typeS.empty()){
      elt=makeScalar("_", typeS);
    }else{ // Array of non-scalar
      auto subArrays = eMember->find("./g:Scalar | ./g:Union | ./g:Array | ./g:Tuple",ns);
      assert(subArrays.size() == 1);
      for(auto *nSubMember : eMember->find("./g:Scalar | ./g:Union | ./g:Array | ./g:Tuple",ns))
      {
        auto eSubMember=(xmlpp::Element*)nSubMember;
        elt = loadTypedDataSpecElement( eSubMember );
      }
    }
    return makeArray(name, length, elt);

  }else if(eMember->get_name()=="Tuple"){
    std::vector<TypedDataSpecElementPtr> members;

    for(auto *nSubMember : eMember->find("./g:Scalar | ./g:Union | ./g:Array | ./g:Tuple",ns))
    {
      auto eSubMember=(xmlpp::Element*)nSubMember;
      members.push_back( loadTypedDataSpecElement( eSubMember ) );
    }

    return makeTuple(name, members.begin(), members.end() );
  }else if(eMember->get_name()=="Union"){
    throw std::runtime_error("Union not impl.");
  }else{
    throw std::runtime_error("Unknown data spec part.");
  }

}

inline TypedDataSpecPtr loadTypedDataSpec(xmlpp::Element *eTypedDataSpec)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  std::vector<TypedDataSpecElementPtr> members;

  for(auto *nMember : eTypedDataSpec->find("./g:Scalar|./g:Tuple|./g:Array|./g:Union", ns))
  {
    auto eMember=(xmlpp::Element*)nMember;
    members.push_back( loadTypedDataSpecElement( eMember ) );
  }

  auto elt=makeTuple("_", members.begin(), members.end());

  return std::make_shared<TypedDataSpecImpl>(elt);
}

inline TypedDataSpecPtr makeTypedDataSpec(
  const std::vector<TypedDataSpecElementPtr> &members
){
  auto elt=makeTuple("_", members.begin(), members.end());

  return std::make_shared<TypedDataSpecImpl>(elt);
}

inline MessageTypePtr loadMessageTypeElement(xmlpp::Element *eMessageType)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  std::string id=get_attribute_required(eMessageType, "id");

  rapidjson::Document metadata=parse_meta_data(eMessageType, "./g:MetaData", ns);

  TypedDataSpecPtr spec;
  auto *eSpec=find_single(eMessageType, "./g:Message", ns);
  if(eSpec){
    spec=loadTypedDataSpec(eSpec);
  }

  return std::make_shared<MessageTypeImpl>(id, spec);
}

class InputPinDynamic
  : public InputPinImpl
{
public:
  InputPinDynamic(
    std::function<DeviceTypePtr ()> deviceTypeSrc,
    const std::string &name,
    unsigned index,
    MessageTypePtr messageType,
    TypedDataSpecPtr propertiesType,
    TypedDataSpecPtr stateType,
    const std::string &code,
    bool isSupervisor=false
  )
  : InputPinImpl(deviceTypeSrc, name, index, messageType, propertiesType, stateType, code, isSupervisor)
  {}

  virtual void onReceive(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*, const typed_data_t*, typed_data_t*, const typed_data_t*) const override
  {
    throw std::runtime_error("onReceive - input pin not loaded from compiled provider, so functionality not available.\n"
   "Ensure you have a compiled provider <appname>.graph.so. Use POETS_PROVIDER_PATH env var to point to a specific directory.");
   }
};

class OutputPinDynamic
  : public OutputPinImpl
{
public:
  OutputPinDynamic(
    std::function<DeviceTypePtr ()> deviceTypeSrc,
    const std::string &name,
    unsigned index,
    MessageTypePtr messageType,
    const std::string &code,
    bool isIndexedSend,
    bool isSupervisor=false
  )
  : OutputPinImpl(deviceTypeSrc, name, index, messageType, code, isIndexedSend, isSupervisor)
  {}

  virtual void onSend(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*, typed_data_t*, bool*, unsigned *) const override
  {
    throw std::runtime_error("onSend - output pin not loaded from compiled provider, so functionality not available.\n"
   "Ensure you have a compiled provider <appname>.graph.so. Use POETS_PROVIDER_PATH env var to point to a specific directory.");
  }
};

class InternalDeviceTypeDynamic
  : public DeviceTypeImpl
{
public:
  InternalDeviceTypeDynamic(const std::string &id,
    TypedDataSpecPtr properties, TypedDataSpecPtr state,
    const std::vector<InputPinPtr> &inputs, const std::vector<OutputPinPtr> &outputs,
    std::string readyToSendCode, std::string onInitCode, std::string sharedCode, std::string onHardwareIdleCode, std::string onDeviceIdleCode
  )
    : DeviceTypeImpl(id, properties, state, inputs, outputs, false, readyToSendCode, onInitCode, sharedCode, onHardwareIdleCode, onDeviceIdleCode)
  {
  }

  virtual void init(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*) const override
  {
    throw std::runtime_error("init - input pin not loaded from compiled provider, so functionality not available.\n"
   "Ensure you have a compiled provider <appname>.graph.so. Use POETS_PROVIDER_PATH env var to point to a specific directory.");
  }

  virtual uint32_t calcReadyToSend(OrchestratorServices*, const typed_data_t*, const typed_data_t*, const typed_data_t*) const override
  {
    throw std::runtime_error("calcReadyToSend - input pin not loaded from compiled provider, so functionality not available.\n"
   "Ensure you have a compiled provider <appname>.graph.so. Use POETS_PROVIDER_PATH env var to point to a specific directory.");
  }

  virtual void onHardwareIdle(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*) const override
  {
    throw std::runtime_error("onHardwareIdle - input pin not loaded from compiled provider, so functionality not available.\n"
   "Ensure you have a compiled provider <appname>.graph.so. Use POETS_PROVIDER_PATH env var to point to a specific directory.");
  }
};

class ExternalDeviceTypeDynamic
  : public DeviceTypeImpl
{
public:
  ExternalDeviceTypeDynamic(const std::string &id,
    TypedDataSpecPtr properties,
    const std::vector<InputPinPtr> &inputs, const std::vector<OutputPinPtr> &outputs
  )
    : DeviceTypeImpl(id, properties, TypedDataSpecPtr(), inputs, outputs, true, "", "", "", "", "")
  {
  }

  virtual void init(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*) const override
  {
    throw std::runtime_error("init - this is an external device, so init should not be called.");
  }

  virtual uint32_t calcReadyToSend(OrchestratorServices*, const typed_data_t*, const typed_data_t*, const typed_data_t*) const override
  {
    throw std::runtime_error("calcReadyToSend - this is an external device, so calcReadyToSend shoud not be called.");
  }

  virtual void onHardwareIdle(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*) const override
  {
    throw std::runtime_error("onHardwareIdle - this is an external device, so does not take part in hardware idle.");
  }
};

inline std::string readTextContent(xmlpp::Element *p)
{
  std::string acc;
  for(auto *n : p->get_children()){
    if(dynamic_cast<xmlpp::Attribute*>(n)){
      continue;
    }else if(dynamic_cast<xmlpp::ContentNode*>(n)){
      acc += dynamic_cast<xmlpp::ContentNode*>(n)->get_content();
    }else{
      throw std::runtime_error("Unexpected element in text node.");
    }
  }
  return acc;
}

inline DeviceTypePtr loadDeviceTypeElement(
  const std::map<std::string,MessageTypePtr> &messageTypes,
  xmlpp::Element *eDeviceType
)
{
  bool isExternal=false;

  if(eDeviceType->get_name()=="DeviceType"){
    isExternal=false;
  }else if(eDeviceType->get_name()=="ExternalType"){
    isExternal=true;
  }else{
    throw std::runtime_error("Unexpected element name "+eDeviceType->get_name());
  }

  // TODO : This is stupid. Weak pointer to get rid of cycle of references.
  auto futureSrc=std::make_shared<std::weak_ptr<DeviceType>>();

  // Passed into pins...
  std::function<DeviceTypePtr ()> delayedSrc=  [=]() -> DeviceTypePtr { return futureSrc->lock(); };

  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  std::string id=get_attribute_required(eDeviceType, "id");
  rapidjson::Document metadata=parse_meta_data(eDeviceType, "./g:MetaData", ns);

  std::string readyToSendCode, onInitCode, onHardwareIdleCode, onDeviceIdleCode, sharedCode;
  if(!isExternal){
    auto *eReadyToSendCode=find_single(eDeviceType, "./g:ReadyToSend", ns);
    if(eReadyToSendCode){
      auto ch=xmlNodeGetContent(eReadyToSendCode->cobj());
      readyToSendCode=(char*)ch;
      xmlFree(ch);
    }
    auto *eOnInitCode=find_single(eDeviceType, "./g:OnInit", ns);
    if(eOnInitCode){
      auto ch=xmlNodeGetContent(eOnInitCode->cobj());
      onInitCode=(char*)ch;
      xmlFree(ch);
    }
    auto *eOnHardwareIdleCode=find_single(eDeviceType, "./g:OnHardwareIdle", ns);
    if(eOnHardwareIdleCode){
      auto ch=xmlNodeGetContent(eOnHardwareIdleCode->cobj());
      onHardwareIdleCode=(char*)ch;
      xmlFree(ch);
    }
    auto *eOnDeviceIdleCode=find_single(eDeviceType, "./g:OnDeviceIdle", ns);
    if(eOnDeviceIdleCode){
      auto ch=xmlNodeGetContent(eOnDeviceIdleCode->cobj());
      onDeviceIdleCode=(char*)ch;
      xmlFree(ch);
    }

    for(auto *n : eDeviceType->find("./g:SharedCode", ns)){
      auto ch=xmlNodeGetContent(n->cobj());
      sharedCode += std::string((char*)ch) + "\n";
      xmlFree(ch);
    }
  }

  TypedDataSpecPtr properties=std::make_shared<TypedDataSpecImpl>();
  auto *eProperties=find_single(eDeviceType, "./g:Properties", ns);
  if(eProperties){
    properties=loadTypedDataSpec(eProperties);
  }

  TypedDataSpecPtr state=std::make_shared<TypedDataSpecImpl>();
  if(!isExternal){
    auto *eState=find_single(eDeviceType, "./g:State", ns);
    if(eState){
      state=loadTypedDataSpec(eState);
    }
  }

  std::vector<InputPinPtr> inputs;

  for(auto *n : eDeviceType->find("./g:InputPin",ns)){
    auto *e=(xmlpp::Element*)n;

    std::string name=get_attribute_required(e, "name");
    std::string messageTypeId=get_attribute_required(e, "messageTypeId");

    if(messageTypes.find(messageTypeId)==messageTypes.end()){
      throw std::runtime_error("Unknown messageTypeId '"+messageTypeId+"'");
    }
    auto messageType=messageTypes.at(messageTypeId);

    rapidjson::Document inputMetadata=parse_meta_data(e, "./g:MetaData", ns);

    TypedDataSpecPtr inputProperties=std::make_shared<TypedDataSpecImpl>();
    if(!isExternal){
      auto *eInputProperties=find_single(e, "./g:Properties", ns);
      if(eInputProperties){
        inputProperties=loadTypedDataSpec(eInputProperties);
      }
    }

    TypedDataSpecPtr inputState=std::make_shared<TypedDataSpecImpl>();
    if(!isExternal){
      auto *eInputState=find_single(e, "./g:State", ns);
      if(eInputState){
        inputState=loadTypedDataSpec(eInputState);
      }
    }

    std::string onReceive;
    if(!isExternal){
      auto *eHandler=find_single(e, "./g:OnReceive", ns);
      if(eHandler==NULL){
        throw std::runtime_error("Missing OnReceive handler.");
      }
      onReceive=readTextContent(eHandler);
    }

    inputs.push_back(std::make_shared<InputPinDynamic>(
      delayedSrc,
      name,
      inputs.size(),
      messageType,
      inputProperties,
      inputState,
      onReceive
    ));
  }


  std::vector<OutputPinPtr> outputs;

  for(auto *n : eDeviceType->find("./g:OutputPin",ns)){
    auto *e=(xmlpp::Element*)n;

    std::string name=get_attribute_required(e, "name");
    std::string messageTypeId=get_attribute_required(e, "messageTypeId");
    bool isIndexedSend=get_attribute_optional_bool(e, "indexed");

    if(messageTypes.find(messageTypeId)==messageTypes.end()){
      throw std::runtime_error("Unknown messageTypeId '"+messageTypeId+"'");
    }
    auto messageType=messageTypes.at(messageTypeId);

    rapidjson::Document outputMetadata=parse_meta_data(e, "./g:MetaData", ns);

    std::string onSend;
    if(!isExternal){
      auto *eHandler=find_single(e, "./g:OnSend", ns);
      if(eHandler==NULL){
        throw std::runtime_error("Missing OnSend handler.");
      }
      onSend=readTextContent(eHandler);
    }


    outputs.push_back(std::make_shared<OutputPinDynamic>(
      delayedSrc,
      name,
      outputs.size(),
      messageType,
      onSend,
      isIndexedSend
    ));
  }

  DeviceTypePtr res;
  if(!isExternal){
    res=std::make_shared<InternalDeviceTypeDynamic>(
      id, properties, state, inputs, outputs, readyToSendCode, onInitCode, sharedCode, onHardwareIdleCode, onDeviceIdleCode
    );
  }else{
    res=std::make_shared<ExternalDeviceTypeDynamic>(
      id, properties, inputs, outputs
    );
  }

  // Lazily fill in the thing that delayedSrc points to
  *futureSrc=res;

  return res;
}

class GraphTypeDynamic
  : public GraphTypeImpl
{
public:
  GraphTypeDynamic(
    const std::string &id,
    std::vector<TypedDataSpecPtr> typedefs,
    TypedDataSpecPtr properties,
    const rapidjson::Document &metadata,
    const std::vector<std::string> &sharedCode,
    const std::vector<MessageTypePtr> &messageTypes,
    const std::vector<DeviceTypePtr> &deviceTypes,
    const std::vector<SupervisorTypePtr> &supervisorTypes={}
  )
    : GraphTypeImpl(id, properties)
  {
    getMetadata().CopyFrom( metadata, getMetadata().GetAllocator() );
    for(auto s : sharedCode){
      addSharedCode(s);
    }
    for(auto mt : messageTypes){
      addMessageType(mt);
    }
    for(auto dt : deviceTypes){
      addDeviceType(dt);
    }
    for(auto st : supervisorTypes){
      addSupervisorType(st);
    }
  }
};

inline GraphTypePtr loadGraphTypeElement(const filepath &srcPath, xmlpp::Element *eGraphType, GraphLoadEvents *events=0)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";


  std::string id=get_attribute_required(eGraphType, "id");

  std::vector<TypedDataSpecPtr> typedefs;
  auto *eTypes=find_single(eGraphType, "./g:Types", ns);
  int i = 0;
  for (auto *eType : eGraphType->find("./g:TypeDef", ns)) {
    for (auto *eTypeDef : eType->find("./g:Scalar|./g:Tuple|./g:Array|./g:Union", ns)) {
      TypedDataSpecPtr TypeDef = loadTypedDataSpec((xmlpp::Element*)eTypeDef);
      typedefs.push_back(TypeDef);
    }
  }

  TypedDataSpecPtr properties;
  auto *eGraphProperties=find_single(eGraphType, "./g:Properties", ns);
  if(eGraphProperties){
    properties=loadTypedDataSpec(eGraphProperties);
  }else{
    properties=std::make_shared<TypedDataSpecImpl>();
  }

  std::vector<std::string> sharedCode;
  for(auto *nSharedCode : eGraphType->find("./g:SharedCode", ns)){
    std::string x=readTextContent((xmlpp::Element*)nSharedCode);
    sharedCode.push_back(x);
  }

  rapidjson::Document metadata=parse_meta_data(eGraphType, "./g:MetaData", ns);

  std::map<std::string,MessageTypePtr> messageTypesById;
  std::vector<MessageTypePtr> messageTypes;
  std::vector<DeviceTypePtr> deviceTypes;

  auto *eMessageTypes=find_single(eGraphType, "./g:MessageTypes", ns);
  for(auto *nMessageType : eMessageTypes->find("./g:MessageType", ns)){
    auto mt=loadMessageTypeElement( (xmlpp::Element*)nMessageType);

    messageTypesById[mt->getId()]=mt;

    messageTypes.push_back(mt);
  }

  auto *eDeviceTypes=find_single(eGraphType, "./g:DeviceTypes", ns);
  for(auto *nDeviceType : eDeviceTypes->find("./g:DeviceType|./g:ExternalType", ns)){
    auto eDeviceType=(xmlpp::Element*)nDeviceType;
    auto dt=loadDeviceTypeElement(messageTypesById, eDeviceType);
    deviceTypes.push_back( dt );
  }

  auto res=std::make_shared<GraphTypeDynamic>(
    id,
    typedefs,
    properties,
    metadata,
    sharedCode,
    messageTypes,
    deviceTypes
    );

  if(events){
    events->onGraphType(res);
  }

  return res;
}

inline GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events, const std::string &id);

inline GraphTypePtr loadGraphTypeReferenceElement(const filepath &srcPath, xmlpp::Element *eGraphTypeReference, GraphLoadEvents *events)
{
  std::string id=get_attribute_required(eGraphTypeReference, "id");
  std::string src=get_attribute_required(eGraphTypeReference, "src");

  filepath newRelPath(src);
  filepath newPath=absolute(newRelPath, srcPath);

  if(!exists(newPath)){
    throw std::runtime_error("Couldn't resolve graph reference src '"+src+"', tried looking in '"+newPath.native()+"'");
  }

  auto parser=std::make_shared<xmlpp::DomParser>(newPath.native());
  if(!*parser){
    throw std::runtime_error("Couldn't parse XML at '"+newPath.native()+"'");
  }

  auto root=parser->get_document()->get_root_node();

  return loadGraphType(newPath, root, events, id);
}

//! Given a graph an element of type "g:Graphs", look for a graph type with given id.
inline GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events, const std::string &id)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  //std::cerr<<"parent = "<<parent<<"\n";
  for(auto *nGraphType : parent->find("./*")){
    std::cerr<<"  "<<nGraphType->get_name()<<"\n";
  }

  auto *eGraphType=find_single(parent, "./g:GraphType[@id='"+id+"'] | ./g:GraphTypeReference[@id='"+id+"']", ns);
  if(eGraphType!=0){
    if(eGraphType->get_name()=="GraphTypeReference"){
      return loadGraphTypeReferenceElement(srcPath, eGraphType, events);
    }else{
      return loadGraphTypeElement(srcPath, eGraphType, events);
    }
  }

  throw unknown_graph_type_error(id);
}

//! Given a graph an element of type "g:Graphs", look for a graph type with given id.
inline GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  //std::cerr<<"parent = "<<parent<<"\n";
  for(auto *nGraphType : parent->find("./*")){
    std::cerr<<"  "<<nGraphType->get_name()<<"\n";
  }

  auto *eGraphType=find_single(parent, "./g:GraphType|./g:GraphTypeReference", ns);
  if(eGraphType!=0){
    if(eGraphType->get_name()=="GraphTypeReference"){
      return loadGraphTypeReferenceElement(srcPath, eGraphType, events);
    }else{
      return loadGraphTypeElement(srcPath, eGraphType, events);
    }
  }

  throw std::runtime_error("No GraphType to load.");
}

// Helper function to load graph type from path
inline GraphTypePtr loadGraphType(const filepath &srcPath, const std::string &id, GraphLoadEvents *events)
{
  auto parser=std::make_shared<xmlpp::DomParser>(srcPath.native());
  if(!*parser){
    throw std::runtime_error("Couldn't parse XML at '"+srcPath.native()+"'");
  }

  auto root=parser->get_document()->get_root_node();

  return loadGraphType(srcPath, root, events, id);
}

inline std::map<std::string,GraphTypePtr> loadAllGraphTypes(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  std::map<std::string,GraphTypePtr> res;

  for(auto *nGraphType : parent->find("./g:GraphType", ns)){
    auto r=loadGraphTypeElement(srcPath, (xmlpp::Element *)nGraphType, events);
    if( res.find(r->getId())!=res.end() ){
      throw std::string("Duplicate graph type id.");
    }
    res.insert(std::make_pair(r->getId(),r));
    events->onGraphType(r);
  }
  return res;
}


inline void loadGraph(Registry *registry, const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v3";

  auto *eGraph=find_single(parent, "./g:GraphInstance", ns);
  if(eGraph==0)
    throw std::runtime_error("No graph element.");

  bool parseMetaData=events->parseMetaData();

  std::string graphId=get_attribute_required(eGraph, "id");
  std::string graphTypeId=get_attribute_required(eGraph, "graphTypeId");

  auto graphTypeEmb=loadGraphType(srcPath, parent, events, graphTypeId);
  if(!graphTypeEmb){
    throw std::runtime_error("Couldn't find embedded graph or graph reference to graph type "+graphTypeId);
  }

  GraphTypePtr graphTypeReg;
  if(registry){
    try{
      graphTypeReg=registry->lookupGraphType(graphTypeId);


    }catch(const unknown_graph_type_error &){
      // pass, try to load dyamically
    }
  }

  if(graphTypeReg){
    try{
      check_graph_types_structurally_similar(graphTypeEmb, graphTypeReg, true);
    }catch(std::exception &e){
      throw std::runtime_error("Error while comparing reference graph type in file and compiled graph type in provider : "+std::string(e.what()));
    }
  }

  GraphTypePtr graphType = graphTypeReg ? graphTypeReg : graphTypeEmb;

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

  uint64_t gId;
  rapidjson::Document graphMetadata;
  if(parseMetaData){
    graphMetadata=parse_meta_data(eGraph, "g:MetaData", ns);
  }
  gId=events->onBeginGraphInstance(graphType, graphId, graphProperties, std::move(graphMetadata));

  using devices_t = std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> >;
  devices_t devices;

  auto *eDeviceInstances=find_single(eGraph, "./g:DeviceInstances", ns);
  if(!eDeviceInstances)
    throw std::runtime_error("No DeviceInstances element");

  events->onBeginDeviceInstances(gId);

  for(auto *nDevice : eDeviceInstances->find("./g:DevI|./g:ExtI", ns)){
    auto *eDevice=(xmlpp::Element *)nDevice;

    std::string id=get_attribute_required(eDevice, "id");
    std::string deviceTypeId=get_attribute_required(eDevice, "type");

    auto dt=graphType->getDeviceType(deviceTypeId);

    TypedDataPtr deviceProperties;
    auto *eProperties=find_single(eDevice, "./g:P", ns);
    if(eProperties){
      deviceProperties=dt->getPropertiesSpec()->load(eProperties);
    }else{
      auto ps=dt->getPropertiesSpec();
      if(ps){
        deviceProperties=dt->getPropertiesSpec()->create();
      }
    }

    TypedDataPtr deviceState;
    auto *eState=find_single(eDevice, "./g:S", ns);
    if(eState){
      if(dt->isExternal()){
        throw std::runtime_error("External instance "+id+" had a state struct.");
      }
      deviceState=dt->getStateSpec()->load(eState);
    }else{
      auto ss=dt->getStateSpec();
      if(ss){
        deviceState=dt->getStateSpec()->create();
      }
    }

    uint64_t dId;
    rapidjson::Document deviceMetadata;
    if(parseMetaData){
      deviceMetadata=parse_meta_data(eDevice, "g:M", ns);
    }

    dId=events->onDeviceInstance(gId, dt, id, deviceProperties, deviceState, std::move(deviceMetadata));

    devices.insert(std::make_pair( id, std::make_pair(dId, dt)));
  }

  events->onEndDeviceInstances(gId);

  events->onBeginEdgeInstances(gId);

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

    std::string srcDeviceId, srcPinName, dstDeviceId, dstPinName;
    std::string path=get_attribute_required(eEdge, "path");
    split_path(path, dstDeviceId, dstPinName, srcDeviceId, srcPinName);

    int sendIndex=-1;
    std::string sendIndexStr=get_attribute_optional(eEdge, "sendIndex");
    if(!sendIndexStr.empty()){
      sendIndex=atoi(sendIndexStr.c_str());
      if(sendIndex<0){
        throw std::runtime_error("Invalid sendIndex.");
      }
    }

    auto at_devices_or_throw=[&](const std::string &device) -> devices_t::mapped_type &
    {
      auto it=devices.find(device);
      if(it==devices.end()){
        throw std::runtime_error("No device instance called "+device+" known, but it was used in path "+path);
      }
      return it->second;
    };

    auto &srcDevice=at_devices_or_throw(srcDeviceId);
    auto &dstDevice=at_devices_or_throw(dstDeviceId);

    auto srcPin=srcDevice.second->getOutput(srcPinName);
    auto dstPin=dstDevice.second->getInput(dstPinName);

    if(!srcPin){
      throw std::runtime_error("No source pin called '"+srcPinName+"' on device '"+srcDeviceId);
    }
    if(!dstPin){
      throw std::runtime_error("No sink pin called '"+dstPinName+"' on device '"+dstDeviceId);
    }

    if(srcPin->getMessageType()!=dstPin->getMessageType())
      throw std::runtime_error("Edge type mismatch on pins.");

    if(sendIndex!=-1 && !srcPin->isIndexedSend()){
      throw std::runtime_error("Attempt to set send index on non indexed output pin.");
    }

    auto et=dstPin->getPropertiesSpec();
    auto st=dstPin->getStateSpec();

    TypedDataPtr edgeProperties;
    xmlpp::Element *eProperties=0;
    TypedDataPtr edgeState;
    xmlpp::Element *eState=0;
    {
      const auto &children=eEdge->get_children();
      for(const auto &nChild : children){
        assert(nChild->get_name().is_ascii());

        if(!strcmp(nChild->get_name().c_str(),"P")){
          eProperties=(xmlpp::Element*)nChild;
        }else if(!strcmp(nChild->get_name().c_str(),"S")){
          eState=(xmlpp::Element*)nChild;
        }
      }
    }

    if(eProperties){
      edgeProperties=et->load(eProperties);
    }else if(et){
      edgeProperties=et->create();
    }
    if(eState){
      edgeState=st->load(eState);
    }else if(st){
      edgeState=st->create();
    }

    rapidjson::Document metadata;
    if(parseMetaData){
      // TODO: For efficiency this should be rolled into the above loop for properties and state
      metadata=parse_meta_data(eEdge, "g:M", ns);
    }

    events->onEdgeInstance(gId,
                dstDevice.first, dstDevice.second, dstPin,
                srcDevice.first, srcDevice.second, srcPin,
                sendIndex,
                edgeProperties,
                edgeState,
                std::move(metadata)
    );
  }

  events->onEndEdgeInstances(gId);

  events->onEndGraphInstance(gId);
}

}; // namespace xml_v3

#endif
