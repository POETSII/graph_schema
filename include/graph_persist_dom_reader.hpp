#ifndef graph_persist_dom_reader_hpp
#define graph_persist_dom_reader_hpp

#include "graph_persist.hpp"
#include "graph_provider_helpers.hpp"

//#include <boost/filesystem.hpp>
#include <libxml++/parsers/domparser.h>

void split_path(const std::string &src, std::string &dstDevice, std::string &dstPin, std::string &srcDevice, std::string &srcPin)
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


rapidjson::Document parse_meta_data(xmlpp::Element *parent, const char *name, xmlpp::Node::PrefixNsMap &ns)
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


TypedDataSpecElementPtr loadTypedDataSpecElement(xmlpp::Element *eMember)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";

  
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
    }else{
      throw std::runtime_error("Arrays of non-scalar not implemented yet.");
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

TypedDataSpecPtr loadTypedDataSpec(xmlpp::Element *eTypedDataSpec)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";
  
  std::vector<TypedDataSpecElementPtr> members;
  
  for(auto *nMember : eTypedDataSpec->find("./g:Scalar|./g:Tuple|./g:Array|./g:Union", ns))
  {
    auto eMember=(xmlpp::Element*)nMember;
    members.push_back( loadTypedDataSpecElement( eMember ) );
  }
  
  auto elt=makeTuple("_", members.begin(), members.end());
  
  return std::make_shared<TypedDataSpecImpl>(elt);
}

MessageTypePtr loadMessageTypeElement(xmlpp::Element *eMessageType)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";
  
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
    const std::string &code
  )
  : InputPinImpl(deviceTypeSrc, name, index, messageType, propertiesType, stateType, code)
  {}

  virtual void onReceive(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*, const typed_data_t*, typed_data_t*, const typed_data_t*) const override
  {
    throw std::runtime_error("onReceive - input pin not loaded from provider, so functionality not available.");
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
    const std::string &code
  )
  : OutputPinImpl(deviceTypeSrc, name, index, messageType, code)
  {}

  virtual void onSend(OrchestratorServices*, const typed_data_t*, const typed_data_t*, typed_data_t*, typed_data_t*, bool*) const
  {
    throw std::runtime_error("onSend - output pin not loaded from provider, so functionality not available.");
  }
};

class DeviceTypeDynamic
  : public DeviceTypeImpl
{
public:
  DeviceTypeDynamic(const std::string &id, TypedDataSpecPtr properties, TypedDataSpecPtr state, const std::vector<InputPinPtr> &inputs, const std::vector<OutputPinPtr> &outputs, bool isExternal)
    : DeviceTypeImpl(id, properties, state, inputs, outputs, isExternal)
  {
    for(auto i : inputs){
      std::cerr<<"  input : "<<i->getName()<<"\n";
    }
    for(auto o : outputs){
      std::cerr<<"  output : "<<o->getName()<<"\n";
    }
  }
  
  virtual uint32_t calcReadyToSend(OrchestratorServices*, const typed_data_t*, const typed_data_t*, const typed_data_t*) const override
  {
    throw std::runtime_error("calcReadyToSend - input pin not loaded from provider, so functionality not available.");
  }
};

std::string readTextContent(xmlpp::Element *p)
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

DeviceTypePtr loadDeviceTypeElement(
  const std::map<std::string,MessageTypePtr> &messageTypes,
  xmlpp::Element *eDeviceType
)
{
  // TODO : This is stupid. Circular initialisation stuff, but we end up with cycle of references.
  auto futureSrc=std::make_shared<DeviceTypePtr>();
  
  // Passed into pins...
  std::function<DeviceTypePtr ()> delayedSrc=  [=]() -> DeviceTypePtr { return *futureSrc; };
  
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";
  
  std::string id=get_attribute_required(eDeviceType, "id");
  rapidjson::Document metadata=parse_meta_data(eDeviceType, "./g:MetaData", ns);
  
  bool isExternal=false;
  if(eDeviceType->get_name()=="ExternalType"){
    isExternal=true;
  }

  std::cerr<<"Loading "<<id<<"\n";
  
  std::vector<std::string> sharedCode;
  for(auto *n : eDeviceType->find("./g:SharedCode", ns)){
    sharedCode.push_back(((xmlpp::Element*)n)->get_child_text()->get_content());
  }
  
  TypedDataSpecPtr properties;
  auto *eProperties=find_single(eDeviceType, "./g:Properties", ns);
  if(eProperties){
    properties=loadTypedDataSpec(eProperties);
  }else{
    properties=std::make_shared<TypedDataSpecImpl>();
  }
  
  TypedDataSpecPtr state;
  auto *eState=find_single(eDeviceType, "./g:State", ns);
  if(eState){
    state=loadTypedDataSpec(eState);
  }else{
    state=std::make_shared<TypedDataSpecImpl>();
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
  
    TypedDataSpecPtr inputProperties;
    auto *eInputProperties=find_single(e, "./g:Properties", ns);
    if(eInputProperties){
      inputProperties=loadTypedDataSpec(eInputProperties);
    }else{
      inputProperties=std::make_shared<TypedDataSpecImpl>();
    }
    
    TypedDataSpecPtr inputState;
    auto *eInputState=find_single(e, "./g:State", ns);
    if(eInputState){
      inputState=loadTypedDataSpec(eInputState);
    }else{
      inputState=std::make_shared<TypedDataSpecImpl>();
    }
    
    auto *eHandler=find_single(e, "./g:OnReceive", ns);
    if(eHandler==NULL){
      throw std::runtime_error("Missing OnReceive handler.");
    }
    std::string onReceive=readTextContent(eHandler);
    
    
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
    
    if(messageTypes.find(messageTypeId)==messageTypes.end()){
      throw std::runtime_error("Unknown messageTypeId '"+messageTypeId+"'");
    }
    auto messageType=messageTypes.at(messageTypeId);
    
    rapidjson::Document outputMetadata=parse_meta_data(e, "./g:MetaData", ns);
 
    auto *eHandler=find_single(e, "./g:OnSend", ns);
    if(eHandler==NULL){
      throw std::runtime_error("Missing OnSend handler.");
    }
    std::string onSend=readTextContent(eHandler);
    
    
    outputs.push_back(std::make_shared<OutputPinDynamic>(
      delayedSrc,
      name, 
      outputs.size(),
      messageType,
      onSend
    ));
  }
  
  auto res=std::make_shared<DeviceTypeDynamic>(
    id, properties, state, inputs, outputs, isExternal
  );
  
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
    TypedDataSpecPtr properties,
    const rapidjson::Document &metadata,
    const std::vector<std::string> &sharedCode,
    const std::vector<MessageTypePtr> &messageTypes,
    const std::vector<DeviceTypePtr> &deviceTypes
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
  }
};

GraphTypePtr loadGraphTypeElement(const filepath &srcPath, xmlpp::Element *eGraphType, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";

  
  std::string id=get_attribute_required(eGraphType, "id");
  
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
    events->onMessageType(mt);
  }
  
  auto *eDeviceTypes=find_single(eGraphType, "./g:DeviceTypes", ns);
  for(auto *nDeviceType : eDeviceTypes->find("./g:DeviceType|g:ExternalType", ns)){
    auto dt=loadDeviceTypeElement(messageTypesById, (xmlpp::Element*)nDeviceType);
    
    std::cerr<<"device type = "<<dt->getId()<<"\n";
    deviceTypes.push_back( dt );
    events->onDeviceType(dt);
  }
  
  auto res=std::make_shared<GraphTypeDynamic>(
    id,
    properties,
    metadata,
    sharedCode,
    messageTypes,
    deviceTypes
    );

  
  events->onGraphType(res);
  
  return res;
}

GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events, const std::string &id);

GraphTypePtr loadGraphTypeReferenceElement(const filepath &srcPath, xmlpp::Element *eGraphTypeReference, GraphLoadEvents *events)
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
GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events, const std::string &id)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";
  
  std::cerr<<"parent = "<<parent<<"\n";
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

std::map<std::string,GraphTypePtr> loadAllGraphTypes(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";
  
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

void loadGraph(Registry *registry, const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v2";

  auto *eGraph=find_single(parent, "./g:GraphInstance", ns);
  if(eGraph==0)
    throw std::runtime_error("No graph element.");

  bool parseMetaData=events->parseMetaData();

  std::string graphId=get_attribute_required(eGraph, "id");
  std::string graphTypeId=get_attribute_required(eGraph, "graphTypeId");
  
  GraphTypePtr graphType;
  if(registry){
    try{
      graphType=registry->lookupGraphType(graphTypeId);
      
      for(auto et : graphType->getMessageTypes()){
        events->onMessageType(et);
      }
      for(auto dt : graphType->getDeviceTypes()){
        events->onDeviceType(dt);
      }
      events->onGraphType(graphType);
    }catch(const unknown_graph_type_error &){
      // pass, try to load dyamically
    }
  }
  if(!graphType){
    graphType=loadGraphType(srcPath, parent, events, graphTypeId);
  }

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
  
  std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> > devices;

  auto *eDeviceInstances=find_single(eGraph, "./g:DeviceInstances", ns);
  if(!eDeviceInstances)
    throw std::runtime_error("No DeviceInstances element");

  events->onBeginDeviceInstances(gId);

  for(auto *nDevice : eDeviceInstances->find("./g:DevI|g:ExtI", ns)){
    auto *eDevice=(xmlpp::Element *)nDevice;

    std::string id=get_attribute_required(eDevice, "id");
    std::string deviceTypeId=get_attribute_required(eDevice, "type");

    auto dt=graphType->getDeviceType(deviceTypeId);

    TypedDataPtr deviceProperties;
    auto *eProperties=find_single(eDevice, "./g:P", ns);
    if(eProperties){
      deviceProperties=dt->getPropertiesSpec()->load(eProperties);
    }else{
      deviceProperties=dt->getPropertiesSpec()->create();
    }

    uint64_t dId;
    rapidjson::Document deviceMetadata;
    if(parseMetaData){
      deviceMetadata=parse_meta_data(eDevice, "g:M", ns);
    }
    dId=events->onDeviceInstance(gId, dt, id, deviceProperties, std::move(deviceMetadata));

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
    std::string path=get_attribute_optional(eEdge, "path");
    if(!path.empty()){
      split_path(path, dstDeviceId, dstPinName, srcDeviceId, srcPinName);
      //std::cerr<<srcDeviceId<<" "<<srcPinName<<" "<<dstDeviceId<<" "<<dstPinName<<"\n";
    }else{
      srcDeviceId=get_attribute_required(eEdge, "srcDeviceId");
      srcPinName=get_attribute_required(eEdge, "srcPinName");
      dstDeviceId=get_attribute_required(eEdge, "dstDeviceId");
      dstPinName=get_attribute_required(eEdge, "dstPinName");
    }

    auto srcDeviceIt=devices.find(srcDeviceId);
    if(srcDeviceIt==devices.end()){
      throw std::runtime_error("No source device called '"+srcDeviceId+"' for edge path '"+path+"'");
    }
    auto &srcDevice=srcDeviceIt->second;
    
    auto dstDeviceIt=devices.find(dstDeviceId);
    if(dstDeviceIt==devices.end()){
      throw std::runtime_error("No source device called '"+dstDeviceId+"' for edge path '"+path+"'");
    }
    auto &dstDevice=dstDeviceIt->second;
    
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

    auto et=dstPin->getPropertiesSpec();


    TypedDataPtr edgeProperties;
    xmlpp::Element *eProperties=0;
    {
      const auto &children=eEdge->get_children();
      if(children.size()<10){
        for(const auto &nChild : children){
          assert(nChild->get_name().is_ascii());

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
      edgeProperties=et->load(eProperties);
    }else{
      edgeProperties=et->create();
    }

    rapidjson::Document metadata;
    if(parseMetaData){
      metadata=parse_meta_data(eEdge, "g:M", ns);
    }
    events->onEdgeInstance(gId,
                dstDevice.first, dstDevice.second, dstPin,
                srcDevice.first, srcDevice.second, srcPin,
                edgeProperties,
                std::move(metadata)
    );
  }

  events->onBeginEdgeInstances(gId);

  events->onEndGraphInstance(gId);
}

#endif
