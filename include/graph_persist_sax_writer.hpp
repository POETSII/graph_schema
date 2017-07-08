#ifndef graph_persist_sax_writer_hpp
#define graph_persist_sax_writer_hpp

#include "graph.hpp"

#include "libxml/xmlwriter.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <unordered_set>

namespace detail
{

class GraphSAXWriter :
  public GraphLoadEvents
{
private:
  enum State{
    State_Graph,
    State_GraphInstance,
    State_DeviceInstances,
    State_PostDeviceInstances,
    State_EdgeInstances,
    State_PostEdgeInstances
  };
  
  const xmlChar *m_ns=(const xmlChar *)"http://TODO.org/POETS/virtual-graph-schema-v0";


  
  xmlTextWriterPtr m_dst;
  bool m_parseMetaData;

  State m_state;

  uint64_t m_gId=0;
  GraphTypePtr m_graphType;

  // This is use to sanity check what the client is giving us, and avoid duplicate ids.
  // Used while adding device instances, then again for edge instances.
  // For a trusted client it could be skipped.
  std::unordered_set<std::string> m_seenIds;

  // This holds the actual ids.
  std::vector<std::string> m_deviceIds;

  std::string stateName(State s)
  {
    switch(s){
    case State_Graph: return "Graph";
    case State_GraphInstance: return "GraphInstance";
    case State_DeviceInstances: return "DeviceInstances";
    case State_PostDeviceInstances: return "PostDeviceInstances";
    case State_EdgeInstances: return "EdgeInstances";
    case State_PostEdgeInstances: return "PostEdgeInstances";
    default: return "<Unknown:CorruptState>";
    }
  }
    

  void moveState(State expected, State next)
  {
    if(m_state!=expected){
      throw std::runtime_error("Can't move from state "+stateName(m_state)+" to state "+stateName(next)+"; should have been in "+stateName(expected));
    }
    m_state=next;
  }

  void writeTypedData(const TypedDataSpecPtr &spec, const TypedDataPtr &data, const char *name)
  {
    if(data){
      std::string json{spec->toJSON(data)};
      if(json.size()>2){ // If it is <=2 it is either empty or just "{}"
	assert(json[json.size()-1]=='}');
	assert(json[0]=='{');
	
	json.resize(json.size()-1); // Chop off the final '}'. Probably no realloc

	// The +1 skips over the first '{'
        xmlTextWriterStartElement(m_dst, (const xmlChar *)name);
        xmlTextWriterWriteRaw(m_dst, (const xmlChar *)(json.c_str()+1));
        xmlTextWriterEndElement(m_dst);
      }
    }
  }
  
  void writeMetaData(const rapidjson::Document &data, const char *name)
  {
    if(data.IsNull())
      return;
    if(!data.IsObject())
      throw std::runtime_error("Metadata must be null or an object.");
    if(data.MemberCount()==0)
      return;
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data.Accept(writer);
    
    std::string json(buffer.GetString(), buffer.GetSize());
    assert(json.size()>2);
    assert(json[json.size()-1]=='}');
    assert(json[0]=='{');
        
    json.resize(json.size()-1); // Chop off the final '}'. Probably no realloc

    // The +1 skips over the first '{'
    xmlTextWriterStartElement(m_dst, (const xmlChar *)name);
    xmlTextWriterWriteRaw(m_dst, (const xmlChar *)(json.c_str()+1));
    xmlTextWriterEndElement(m_dst);
  }
public:
    GraphSAXWriter(xmlTextWriterPtr dst)
      : m_dst(dst)
      , m_state(State_Graph)
  {
      xmlTextWriterSetIndent(m_dst, 1);
      xmlTextWriterStartDocument(m_dst, NULL, NULL, NULL);
      xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar*)"Graph", m_ns);
  }


  virtual ~GraphSAXWriter()
  {
    if(m_dst){
      xmlTextWriterEndElement(m_dst);
      xmlTextWriterEndDocument(m_dst);
      xmlFreeTextWriter(m_dst);
      m_dst=0;
    }
  }
  
  virtual bool parseMetaData() const override
  { return m_parseMetaData; }
  
  virtual uint64_t onBeginGraphInstance(
    const GraphTypePtr &graphType,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ) override
  {
    moveState(State_Graph, State_GraphInstance);
    m_graphType=graphType;

    xmlTextWriterStartElement(m_dst, (const xmlChar *)"GraphInstance");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id.c_str());
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"graphTypeId", (const xmlChar *)graphType->getId().c_str());

    writeTypedData(graphType->getPropertiesSpec(), properties, "Properties");
    writeMetaData(metadata, "MetaData");
    
    return ++m_gId;
  }

  virtual void onBeginDeviceInstances(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }

    moveState(State_GraphInstance, State_DeviceInstances);
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"DeviceInstances");
  }

  virtual uint64_t onDeviceInstance
  (
   uint64_t gId,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   rapidjson::Document &&metadata
   ) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }
    if(m_state!=State_DeviceInstances){
      throw std::runtime_error("Not in the DeviceInstances state.");
    }
    
    auto it_inserted=m_seenIds.insert(id);
    if(!it_inserted.second)
      throw std::runtime_error("A device called "+id+" has already been added.");
    
    m_deviceIds.push_back(id);
    uint64_t idNum=m_deviceIds.size()-1;

    xmlTextWriterStartElement(m_dst, (const xmlChar *)"DevI");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id.c_str());
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"type", (const xmlChar *)dt->getId().c_str());
    
    writeTypedData(dt->getPropertiesSpec(), properties, "P");
    
    writeMetaData(metadata, "M");

    xmlTextWriterEndElement(m_dst);

    return idNum;
  }

  virtual void onEndDeviceInstances(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }

    moveState(State_DeviceInstances, State_PostDeviceInstances);
    xmlTextWriterEndElement(m_dst);

    m_seenIds.clear(); // We're going to use it for edge instance ids
  }

  virtual void onBeginEdgeInstances(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }
    moveState(State_PostDeviceInstances, State_EdgeInstances);
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"EdgeInstances");
  }

  virtual void onEdgeInstance
  (
   uint64_t gId,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   const TypedDataPtr &properties,
   rapidjson::Document &&metadata
  ) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }
    if(m_state!=State_EdgeInstances){
      throw std::runtime_error("Not in the EdgeInstances state.");
    }
    
    if(dstDevInst >= m_deviceIds.size())
      throw std::runtime_error("Invalid dst device id.");
    if(srcDevInst >= m_deviceIds.size())
      throw std::runtime_error("Invalid src device id.");

    std::string id=m_deviceIds[dstDevInst]+":"+dstPin->getName()+"-"+m_deviceIds[srcDevInst]+":"+srcPin->getName();

    if(dstPin->getMessageType()->getId() != srcPin->getMessageType()->getId())
      throw std::runtime_error("The pin edge types do not match.");

    auto it_inserted=m_seenIds.insert(id);
    if(!it_inserted.second)
      throw std::runtime_error("An edge called "+id+" has already been added.");

    //    fprintf(stderr, "  adding : %s\n", id.c_str());
    
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"EdgeI");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"path", (const xmlChar *)id.c_str());

    writeTypedData(dstPin->getPropertiesSpec(), properties, "P");
    
    writeMetaData(metadata, "M");

    xmlTextWriterEndElement(m_dst);
  }

  virtual void onEndEdgeInstances(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }

    moveState(State_EdgeInstances, State_PostEdgeInstances);
    xmlTextWriterEndElement(m_dst);

    m_seenIds.clear();
  }

  virtual void onEndGraphInstance(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }

    moveState(State_PostEdgeInstances, State_Graph);
    m_graphType.reset();
  }
};

}; // detail

std::shared_ptr<GraphLoadEvents> createSAXWriterOnFile(const std::string &path)
{
  xmlTextWriterPtr dst=xmlNewTextWriterFilename(path.c_str(), 0);
  if(!dst)
    throw std::runtime_error("createSAXWriterOnFile("+path+") - Couldn't create xmlTextWriter");

  return std::make_shared<detail::GraphSAXWriter>(dst);
}



#endif
