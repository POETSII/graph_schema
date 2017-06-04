#ifndef graph_persist_dom_reader_hpp
#define graph_persist_dom_reader_hpp

#include "graph_persist.hpp"

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
  ns["g"]="http://TODO.org/POETS/virtual-graph-schema-v1";

  
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
  ns["g"]="http://TODO.org/POETS/virtual-graph-schema-v1";
  
  std::vector<TypedDataSpecElementPtr> members;
  
  for(auto *nMember : eTypedDataSpec->find("./g:Scalar|./g:Tuple|./g:Array|./g:Union", ns))
  {
    auto eMember=(xmlpp::Element*)nMember;
    members.push_back( loadTypedDataSpecElement( eMember ) );
  }
  
  auto elt=makeTuple("_", members.begin(), members.end());
  
  return std::make_shared<TypedDataSpecImpl>(elt);
}

/*

GraphTypePtr loadGraphTypeElement(xmlpp::Element *eGraphType, GraphLoadEvents *events)
{
  std::string id=get_attribute_required(eGraph, "id");
  
  TypedDataPtr properties;
  auto *eGraphProperties=find_single(eGraphType, "./g:Properties", ns);
  if(eGraphProperties){
    properties=loadTypedDataElement(eGraphProperties);
  }
  
  auto res=std::make_shared<GraphTypeImpl>(id, properties);
  
  std::map<std::string,MessageTypePtr> messageTypes;
  std::map<std::string,DeviceTypePtr> deviceTypes;
  
  auto *eMessageTypes=find_single(parent, "./g:MessageTypes", ns);
  for(auto *nMessageType : eMessageTypes->find("./g:MessageType", ns)){
    auto mt=loadMessageTypeElement( (xmlpp::Element*)nMessageType);
    
    if(messageTypes.find(mt->getId())!=messageTypes.end()){
      throw std::runtime_error("Message type id appears twice.");
    }
    
    messageTypes.insert(std::make_pair( mt->getId(), mt ));
    events->onMessageType(mt);
    res->addMessageType(mt);
  }
  
  auto *eDeviceTypes=find_single(parent, "./g:DeviceTypes", ns);
  for(auto *nDeviceType : eDeviceTypes->find("./g:DeviceType", ns)){
    auto dt=loadDeviceTypeElement( (xmlpp::Element*)nDeviceType);
    
    if(deviceTypes.find(dt->getId())!=deviceTypes.end()){
      throw std::runtime_error("Device type id appears twice.");
    }
    
    deviceTypes.insert(std::make_pair( dt->getId(), mt ));
    events->onDeviceType(dt);
    res->addDeviceType(dt);
  }
  
  events->onGraphType(res);
  
  return res;
}

//! Given a graph an element of type "g:Graphs", look for a graph type with given id.
GraphTypePtr loadGraphType(xmlpp::Element *parent, GraphLoadEvents *events, const std::string &name id)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="http://TODO.org/POETS/virtual-graph-schema-v1";
  
  auto *eGraphType=find_single(parent, "./g:GraphType[@id='"+id+"'", ns);
  if(eGraphType==0){   
    return loadGraphTypeElement(eGraphType, events);
  }
  
  throw std::runtime_error("No graph type element for id='"+id+"'");
}

std::map<std::string,GraphTypePtr> loadAllGraphTypes(xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="http://TODO.org/POETS/virtual-graph-schema-v1";
  
  std::map<std::string,GraphTypePtr> res;
  
  for(auto *nGraphType : parent->find("./g:GraphType", ns){
    auto r=loadGraphTypeElement((xmlpp::Element *)eGraphType, events);
    if( res.find(r->getId())!=res.end() ){
      throw std::string("Duplicate graph type id.");
    }
    res.insert(std::make_pair(r->getId(),r));
    events->onGraphType(r);
  }
  return rs;
}
*/
void loadGraph(Registry *registry, xmlpp::Element *parent, GraphLoadEvents *events)
{
  xmlpp::Node::PrefixNsMap ns;
  ns["g"]="http://TODO.org/POETS/virtual-graph-schema-v1";

  auto *eGraph=find_single(parent, "./g:GraphInstance", ns);
  if(eGraph==0)
    throw std::runtime_error("No graph element.");

  bool parseMetaData=events->parseMetaData();

  std::string graphId=get_attribute_required(eGraph, "id");
  std::string graphTypeId=get_attribute_required(eGraph, "graphTypeId");

  auto graphType=registry->lookupGraphType(graphTypeId);

  for(auto et : graphType->getMessageTypes()){
    events->onMessageType(et);
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

  uint64_t gId;
  if(parseMetaData){
    auto metadata=parse_meta_data(eGraph, "g:MetaData", ns);
    gId=events->onBeginGraphInstance(graphType, graphId, graphProperties, std::move(metadata));
  }else{
    gId=events->onBeginGraphInstance(graphType, graphId, graphProperties);
  }

  std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> > devices;

  auto *eDeviceInstances=find_single(eGraph, "./g:DeviceInstances", ns);
  if(!eDeviceInstances)
    throw std::runtime_error("No DeviceInstances element");

  events->onBeginDeviceInstances(gId);

  for(auto *nDevice : eDeviceInstances->find("./g:DevI", ns)){
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
    if(parseMetaData){
      rapidjson::Document metadata=parse_meta_data(eGraph, "g:M", ns);
      dId=events->onDeviceInstance(gId, dt, id, deviceProperties, std::move(metadata));
    }else{
      dId=events->onDeviceInstance(gId, dt, id, deviceProperties);
    }

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

    std::string srcDeviceId, srcPortName, dstDeviceId, dstPortName;
    std::string path=get_attribute_optional(eEdge, "path");
    if(!path.empty()){
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

    if(srcPort->getMessageType()!=dstPort->getMessageType())
      throw std::runtime_error("Edge type mismatch on ports.");

    auto et=dstPort->getPropertiesSpec();


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


    if(parseMetaData){
      auto metadata=parse_meta_data(eGraph, "g:M", ns);
      events->onEdgeInstance(gId,
                 dstDevice.first, dstDevice.second, dstPort,
                 srcDevice.first, srcDevice.second, srcPort,
                 edgeProperties,
                 std::move(metadata)
      );
    }else{
      events->onEdgeInstance(gId,
                 dstDevice.first, dstDevice.second, dstPort,
                 srcDevice.first, srcDevice.second, srcPort,
                 edgeProperties);
    }
  }

  events->onBeginEdgeInstances(gId);

  events->onEndGraphInstance(gId);
}

#endif
