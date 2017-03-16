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
    gId=events->onBeginGraphInstance(graphType, graphId, graphProperties);
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
