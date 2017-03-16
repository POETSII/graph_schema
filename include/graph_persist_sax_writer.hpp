#ifndef graph_persist_sax_writer_hpp
#define graph_persist_sax_writer_hpp

#include "graph.hpp"

#include "libxml/xmlwriter.h"

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

  State m_state;

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
	xmlTextWriterWriteElement(m_dst, (const xmlChar *)name, (const xmlChar *)(json.c_str()+1));
      }
    }
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
  
  virtual void onBeginGraphInstance(const GraphTypePtr &graphType, const std::string &id, const TypedDataPtr &properties) override
  {
    moveState(State_Graph, State_GraphInstance);
    m_graphType=graphType;

    xmlTextWriterStartElement(m_dst, (const xmlChar *)"GraphInstance");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id.c_str());
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"graphTypeId", (const xmlChar *)graphType->getId().c_str());

    writeTypedData(graphType->getPropertiesSpec(), properties, "Properties");
  }

  virtual void onBeginDeviceInstances() override
  {
    moveState(State_GraphInstance, State_DeviceInstances);
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"DeviceInstances");
  }

  virtual uint64_t onDeviceInstance
  (
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const double *nativeLocation //! If null then no location, otherwise it will match graphType->getNativeDimension()
   ) override
  {
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
    if(nativeLocation){
      std::stringstream acc;
      for(unsigned i=0;i<m_graphType->getNativeDimension();i++){
	if(i!=0) acc<<",";
	acc<<nativeLocation[i];
      }
      xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"nativeLocation", (const xmlChar *)acc.str().c_str());
    }

    writeTypedData(dt->getPropertiesSpec(), properties, "P");

    xmlTextWriterEndElement(m_dst);

    return idNum;
  }

  virtual void onEndDeviceInstances() override
  {
    moveState(State_DeviceInstances, State_PostDeviceInstances);
    xmlTextWriterEndElement(m_dst);

    m_seenIds.clear(); // We're going to use it for edge instance ids
  }

  virtual void onBeginEdgeInstances() override
  {
    moveState(State_PostDeviceInstances, State_EdgeInstances);
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"EdgeInstances");
  }

  virtual void onEdgeInstance
  (
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr &properties
  ) override
  {
    if(m_state!=State_EdgeInstances){
      throw std::runtime_error("Not in the EdgeInstances state.");
    }
    
    if(dstDevInst >= m_deviceIds.size())
      throw std::runtime_error("Invalid dst device id.");
    if(srcDevInst >= m_deviceIds.size())
      throw std::runtime_error("Invalid src device id.");

    std::string id=m_deviceIds[dstDevInst]+":"+dstPort->getName()+"-"+m_deviceIds[srcDevInst]+":"+srcPort->getName();

    if(dstPort->getEdgeType()->getId() != srcPort->getEdgeType()->getId())
      throw std::runtime_error("The port edge types do not match.");

    auto it_inserted=m_seenIds.insert(id);
    if(!it_inserted.second)
      throw std::runtime_error("An edge called "+id+" has already been added.");

    //    fprintf(stderr, "  adding : %s\n", id.c_str());
    
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"EdgeI");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"path", (const xmlChar *)id.c_str());

    writeTypedData(dstPort->getEdgeType()->getPropertiesSpec(), properties, "P");

    xmlTextWriterEndElement(m_dst);
  }

  virtual void onEndEdgeInstances() override
  {
    moveState(State_EdgeInstances, State_PostEdgeInstances);
    xmlTextWriterEndElement(m_dst);

    m_seenIds.clear();
  }

  virtual void onEndGraphInstance() override
  {
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
