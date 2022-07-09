#ifndef graph_persist_dom_reader_v4_hpp
#define graph_persist_dom_reader_v4_hpp

#include "graph_persist_dom_reader_v3.hpp"
#include "graph_compare.hpp"

#include <regex>

namespace xml_v4
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

inline std::vector<std::string> tokenise_c_def(std::string src)
{
  std::regex re(R"X(^[;{}\[\]]|[1-9][0-9]*|[a-zA-Z_][a-zA-Z_0-9]*)X");

  unsigned pos=0;
  while(pos+1<src.size()){
    if(src[pos]=='/'){
      if(pos+1<src.size() && src[pos+1]=='*'){
        pos+=2;

        while(1){
          if(pos==src.size()){
            throw std::runtime_error("Couldn't parse struct, missing closing C-style comment");
          }
          if(src[pos]=='*'){
            if(pos+1<src.size() && src[pos+1]=='/'){
              pos+=2;
              break;
            }
          }
          pos++;
        }
      }
    }
  }

  std::vector<std::string> res;
  pos=0;
  while(pos<src.size()){
    char c=src[pos];
    if(std::isspace(c)){
      pos++;
      continue;
    }
    // Handle C++ line comments
    if(c=='/'){
      if(pos+1<src.size()){
        if(src[pos+1]=='/'){
          pos+=2;
          while(pos < src.size() && src[pos]!='\n'){
            pos++;
          }
        }
      }
    }
    // actual token stuff
    const std::string &srcc=src;
    std::smatch match;
    if(std::regex_search(srcc.begin()+pos, srcc.end(), match, re)){
      res.push_back(match.str());
      pos+=res.back().size();
    }else{
      throw std::runtime_error("Couldn't parse struct, error at '"+src.substr(pos)+"'");
    }
  }

  return res;
}


inline TypedDataSpecElementPtr loadTypedDataSpecElementFromV4Decl(const std::vector<std::string> &tokens, int &pos)
{
  int spos=pos;

  //fprintf(stderr, "Begin parse level\n");

  assert(pos < (int)tokens.size());

  static const std::set<std::string> type_names{
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "half", "float", "double"
  };
  static const std::set<std::string> keyword_names{
    "struct"
  };
  static const std::regex reId("^[a-zA-Z_][a-zA-Z_0-9]*$");

  auto check_is_identifier=[&](const std::string &x){
    if(!std::regex_match(x, reId)){
      throw std::runtime_error("Expected an id, but got "+x);
    }
    if(type_names.find(x)!=type_names.end()){
      throw std::runtime_error("Expected an id, but got type name "+x);
    }
    if(keyword_names.find(x)!=keyword_names.end()){
      throw std::runtime_error("Expected an id, but got keyword "+x);
    }

  };

  auto next_token=[&]() -> std::string
  {
    if(pos>=(int)tokens.size()){
      throw std::runtime_error("Moved off the end of tokens wile parsing type decl.");
    }
    return tokens[pos++];
  };

  auto peek_token=[&]() -> std::string
  {
    if(pos>=(int)tokens.size()){
      throw std::runtime_error("Moved off the end of tokens wile parsing type decl.");
    }
    return tokens[pos];
  };

  std::vector<int> dimensions;
  TypedDataSpecElementPtr res;

  std::string curr=next_token(), peek;
  std::string eltName;
  if(type_names.find(curr)!=type_names.end()){
    // Scalar or array of scalars
    std::string type_name=curr;

    eltName=next_token();
    check_is_identifier(eltName);

    curr=next_token();
    res=makeScalar(curr==";" ? eltName : "_", type_name);
  }else{
    // struct or array of structs
    if(curr!="struct"){
      throw std::runtime_error("Expected to see typename or a struct in C type decl.");
    }

    curr=next_token();
    if(curr!="{"){
      throw std::runtime_error("Expected to see '{' following 'struct'.");
    }

    std::vector<TypedDataSpecElementPtr> elts;

    peek=peek_token();
    while(peek!="}"){
      elts.push_back(loadTypedDataSpecElementFromV4Decl(tokens, pos));
      peek=peek_token();
    }
    curr=next_token(); // Consume the closing curly brace

    eltName=next_token();
    check_is_identifier(eltName);

    curr=next_token();
    res=makeTuple(curr==";" ? eltName : "_", elts.begin(), elts.end());
  }
  while(curr=="["){
    auto size=next_token();
    dimensions.push_back(std::stoi(size));

    curr=next_token();
    if(curr!="]"){
      throw std::runtime_error("Expected ] but got "+curr);
    }
    curr=next_token(); // Consume the ]
  }
  if(curr!=";"){
    throw std::runtime_error("Expected ; but got "+curr);
  }

  while(dimensions.size()>0){
    res=makeArray(dimensions.size()>1 ? "_" : eltName, dimensions.back(), res);
    dimensions.pop_back();
  }

  /*
  fprintf(stderr, "End parse level : [%u,%u)] ", spos, pos);
  for(unsigned i=spos; i<pos; i++){
    fprintf(stderr, " %s", tokens[i].c_str());
  }
  fprintf(stderr, "\n");
  */
  return res;
}


inline TypedDataSpecPtr loadTypedDataSpec(xmlpp::Element *eParent, const std::string &name)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  std::string cTypeName="_";

  TypedDataSpecPtr spec;
  auto *eSpec=find_single(eParent, name, ns);
  if(eSpec){
    cTypeName=get_attribute_optional(eSpec, "cTypeName");
    if(!cTypeName.empty()){
      static bool warned=false;
      if(!warned){
        fprintf(stderr, "WARNING: The c++ parser and AST in graph_schema does not fully support cTypeName,\n"
                        "         which is being used in a graph-type being loaded. This is probably not a\n"
                        "         problem, as the C++ infrastructure is usually only used for running, not compiling\n.");
        warned=true;
      }
    }

    auto ch=xmlNodeGetContent(eSpec->cobj());
    std::string src((char*)ch);
    xmlFree(ch);

    auto tokens=tokenise_c_def(src);
    int pos=0;

    std::vector<TypedDataSpecElementPtr> members;

    try{
      while(pos < (int)tokens.size()){
        members.push_back(loadTypedDataSpecElementFromV4Decl(tokens, pos));
      }
    }catch(...){
      fprintf(stderr, "full type =\n====\n%s\n====\n", src.c_str());
      fprintf(stderr, "tokens:");
      for(unsigned i=0; i<tokens.size();i++){
        fprintf(stderr, "%u : %s",i,tokens[i].c_str());
        if(pos==(int)i){
          fprintf(stderr, " <<---");
        }
        fprintf(stderr, "\n");
      }
      throw;
    }

    auto elt=makeTuple(cTypeName, members.begin(), members.end());

    spec=std::make_shared<TypedDataSpecImpl>(elt);
  }
  return spec;
}

inline MessageTypePtr loadMessageTypeElement(xmlpp::Element *eMessageType)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  std::string id=get_attribute_required(eMessageType, "id");

  auto spec=loadTypedDataSpec(eMessageType, "./g:Message");

  return std::make_shared<MessageTypeImpl>(id, spec);
}

using xml_v3::InputPinDynamic;
using xml_v3::OutputPinDynamic;
using xml_v3::ExternalDeviceTypeDynamic;
using xml_v3::InternalDeviceTypeDynamic;
using xml_v3::GraphTypeDynamic;
using xml_v3::readTextContent;

class SupervisorTypeDynamic
  : public SupervisorTypeImpl
{
public:
  SupervisorTypeDynamic(const std::string &id,
    std::string properties, std::string state,
    const std::vector<SupervisorType::InputPinInfo> &inputs,
    std::string sharedCode,
    std::string onInitCode,
      std::string onSupervisorIdleCode,
      std::string onStopCode
  )
    : SupervisorTypeImpl(id, properties, state, inputs, sharedCode, onInitCode, onSupervisorIdleCode, onStopCode)
  {
  }

  virtual bool isCloneable() const override
  {
    return false;
  }

  virtual std::shared_ptr<SupervisorInstance> create() const override
  {
    throw std::runtime_error("SupervisorType::create - not supported with dynamic provider.");
  }
};

inline std::string load_code_fragment(xmlpp::Element *eParent, const std::string &name)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  auto *eCode=find_single(eParent, name, ns);
  if(!eCode){
    throw std::runtime_error("Required code element "+name+" is missing (v4 requires all elements).");
  }
  auto ch=xmlNodeGetContent(eCode->cobj());
  std::string res=(char*)ch;
  xmlFree(ch);
  return res;
}

inline TypedDataPtr loadTypedDataValue(const TypedDataSpecPtr &spec, xmlpp::Element *e, const std::string &name)
{
  auto value=get_attribute_optional(e, name.c_str());
  if(value.empty()){
    if(spec){
      return spec->create();
    }else{
      return TypedDataPtr();
    }
  }else{
    TypedDataPtr res;
    try{
     res=spec->loadXmlV4ValueSpec(value, 0);
    }catch(...){
      std::cerr<<"Error while loading '"<<value<<"'\n";
      throw;
    }
    //std::cerr<<"Loaded "<<name<<" : value="<<value<<"\n";
    //std::cerr<<"got = "<<spec->toJSON(res)<<"\n";
    return res;
  }
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
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  std::string id=get_attribute_required(eDeviceType, "id");

  std::cerr<<"  "<<eDeviceType->get_name()<<" "<<id<<" isExternal="<<isExternal<<"\n";


  TypedDataSpecPtr properties;
  TypedDataSpecPtr state;
  std::string readyToSendCode, onInitCode, onHardwareIdleCode, onDeviceIdleCode, sharedCode;

  if(!isExternal){
    readyToSendCode=load_code_fragment(eDeviceType, "./g:ReadyToSend");
    onInitCode=load_code_fragment(eDeviceType, "./g:OnInit");
    onHardwareIdleCode=load_code_fragment(eDeviceType, "./g:OnHardwareIdle");
    onDeviceIdleCode=load_code_fragment(eDeviceType, "./g:OnDeviceIdle");
    sharedCode=load_code_fragment(eDeviceType, "./g:SharedCode");
  }

  properties=loadTypedDataSpec(eDeviceType, "./g:Properties");
  if(!isExternal){
    state=loadTypedDataSpec(eDeviceType, "./g:State");
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

    TypedDataSpecPtr inputState, inputProperties;
    if(!isExternal){
      inputProperties=loadTypedDataSpec(e, "./g:Properties");
      inputState=loadTypedDataSpec(e, "./g:State");
    }else{
      inputProperties=std::make_shared<TypedDataSpecImpl>();
      inputState=std::make_shared<TypedDataSpecImpl>();
    }

    std::string onReceive;
    if(!isExternal){
      onReceive=load_code_fragment(e, "./g:OnReceive");
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

    std::string onSend;
    if(!isExternal){
      onSend=load_code_fragment(e, "./g:OnSend");
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

inline SupervisorTypePtr loadSupervisorTypeElement(
  const std::map<std::string,MessageTypePtr> &messageTypes,
  xmlpp::Element *eSupervisorType
)
{
  if(eSupervisorType->get_name()!="SupervisorType"){
    throw std::runtime_error("Unexpected element name "+eSupervisorType->get_name());
  }

  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  std::string id=get_attribute_required(eSupervisorType, "id");

  std::string onInitCode=load_code_fragment(eSupervisorType, "./g:OnInit");
  std::string onSupervisorIdleCode=load_code_fragment(eSupervisorType, "./g:OnSupervisorIdle");
  std::string onStopCode=load_code_fragment(eSupervisorType, "./g:OnStop");
  std::string sharedCode=load_code_fragment(eSupervisorType, "./g:Code");

  std::string properties=load_code_fragment(eSupervisorType, "./g:Properties");
  std::string state=load_code_fragment(eSupervisorType, "./g:State");

  std::vector<SupervisorType::InputPinInfo> inputs;

  for(auto *n : eSupervisorType->find("./g:InputPin",ns)){
    auto *e=(xmlpp::Element*)n;

    std::string name=get_attribute_required(e, "name");
    std::string messageTypeId=get_attribute_required(e, "messageTypeId");

    if(messageTypes.find(messageTypeId)==messageTypes.end()){
      throw std::runtime_error("Unknown messageTypeId '"+messageTypeId+"'");
    }
    auto messageType=messageTypes.at(messageTypeId);

    std::string onReceive=load_code_fragment(e, "./g:OnReceive");

    inputs.push_back({
      (unsigned)inputs.size(),
      messageType,
      onReceive
    });
  }


  SupervisorTypePtr res=std::make_shared<SupervisorTypeDynamic>(
    id, properties, state, inputs, sharedCode, onInitCode, onSupervisorIdleCode, onStopCode
  );

  return res;
}


inline GraphTypePtr loadGraphTypeElement(const filepath &srcPath, xmlpp::Element *eGraphType, GraphLoadEvents *events=0)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  std::string id=get_attribute_required(eGraphType, "id");

  TypedDataSpecPtr properties=loadTypedDataSpec(eGraphType, "./g:Properties");
  std::string sharedCode=load_code_fragment(eGraphType, "./g:SharedCode");

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
    auto dt=loadDeviceTypeElement(messageTypesById, (xmlpp::Element*)nDeviceType);
    deviceTypes.push_back( dt );
  }

  auto res=std::make_shared<GraphTypeDynamic>(
    id,
    std::vector<TypedDataSpecPtr>(),
    properties,
    rapidjson::Document(),
    std::vector<std::string>{sharedCode},
    messageTypes,
    deviceTypes
    );

  if(events){
    events->onGraphType(res);
  }

  return res;
}

inline GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  auto *eGraphType=find_single(parent, "./g:GraphType", ns);
  if(eGraphType!=0){
    return loadGraphTypeElement(srcPath, eGraphType, events);
  }

  throw std::runtime_error("No GraphType element to load.");
}

// Helper function to load graph type from path
inline GraphTypePtr loadGraphType(const filepath &srcPath)
{
  auto parser=std::make_shared<xmlpp::DomParser>(srcPath.native());
  if(!*parser){
    throw std::runtime_error("Couldn't parse XML at '"+srcPath.native()+"'");
  }

  auto root=parser->get_document()->get_root_node();

  return xml_v4::loadGraphType(srcPath, root, nullptr);
}

inline void loadGraph(Registry *registry, const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="https://poets-project.org/schemas/virtual-graph-schema-v4";

  auto *eGraph=find_single(parent, "./g:GraphInstance", ns);
  if(eGraph==0)
    throw std::runtime_error("No GraphInstance element.");

  std::string graphId=get_attribute_required(eGraph, "id");
  std::string graphTypeId=get_attribute_required(eGraph, "graphTypeId");

  auto graphTypeEmb=xml_v4::loadGraphType(srcPath, parent, events);
  if(graphTypeEmb->getId()!=graphTypeId){
    throw std::runtime_error("GraphType id does not match the GraphInstance's graphTypeId");
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
      throw std::runtime_error("Error while comparing graph type in file and compiled graph type in provider : "+std::string(e.what()));
    }
  }

  GraphTypePtr graphType = graphTypeReg ? graphTypeReg : graphTypeEmb;

  events->onGraphType(graphType);

  TypedDataPtr graphProperties=loadTypedDataValue(graphType->getPropertiesSpec(), eGraph, "P");

  uint64_t gId;
  rapidjson::Document graphMetadata;
  gId=events->onBeginGraphInstance(graphType, graphId, graphProperties, std::move(graphMetadata));

  std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> > devices;

  auto *eDeviceInstances=find_single(eGraph, "./g:DeviceInstances", ns);
  if(!eDeviceInstances)
    throw std::runtime_error("No DeviceInstances element");

  events->onBeginDeviceInstances(gId);

  for(auto *nDevice : eDeviceInstances->get_children()){
    auto *eDevice=dynamic_cast<xmlpp::Element *>(nDevice);
    if(!eDevice)
      continue;

    bool isExternal;
    if(eDevice->get_name()=="DevI"){
      isExternal=false;
    }else if(eDevice->get_name()=="ExtI"){
      isExternal=true;
    }else{
      throw std::runtime_error("DeviceInstance that is neither DevI nor ExtI.");
    }

    std::string id=get_attribute_required(eDevice, "id");
    std::string deviceTypeId=get_attribute_required(eDevice, "type");

    auto dt=graphType->getDeviceType(deviceTypeId);

    if( isExternal != (dt->isExternal()) ){
      std::stringstream acc;
      if(dt->isExternal()){
        acc<<"Attempt to instantiate external type '"<<deviceTypeId<<"' as DevI instance '"<<id<<"'";
      }else{
        acc<<"Attempt to instantiate non-external type '"<<deviceTypeId<<"' as ExtI instance '"<<id<<"'";
      }
      throw std::runtime_error(acc.str());
    }

    TypedDataPtr deviceProperties=loadTypedDataValue(dt->getPropertiesSpec(), eDevice, "P");
    TypedDataPtr deviceState=loadTypedDataValue(dt->getStateSpec(), eDevice, "S");

    uint64_t dId;
    rapidjson::Document deviceMetadata;
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
    if(eEdge->get_name()!="EdgeI"){
      throw std::runtime_error("Found an edge that wasn't an EdgeI.");
    }

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

    auto &srcDevice=devices.at(srcDeviceId);
    auto &dstDevice=devices.at(dstDeviceId);

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

    TypedDataPtr edgeProperties=loadTypedDataValue(et, eEdge, "P");
    TypedDataPtr edgeState=loadTypedDataValue(st, eEdge, "S");

    rapidjson::Document metadata;

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

}; // namespace xml_v4

#endif
