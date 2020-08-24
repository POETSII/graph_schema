#ifndef graph_persist_sax_writer_v4_hpp
#define graph_persist_sax_writer_v4_hpp

#include "graph.hpp"

#include "graph_persist_sax_writer.hpp"

#include "libxml/xmlwriter.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <unordered_set>
#include <cstdint>

namespace detail
{

class GraphSAXWriterV4 :
  public GraphLoadEvents
{
protected:
  enum State{
    State_Graph,
    State_GraphInstance,
    State_DeviceInstances,
    State_PostDeviceInstances,
    State_EdgeInstances,
    State_PostEdgeInstances
  };
  
  const xmlChar *m_ns=(const xmlChar *)"https://poets-project.org/schemas/virtual-graph-schema-v4";

  int m_minorFormatVersion=0;
  
  xmlTextWriterPtr m_dst;
  bool m_parseMetaData = true;

  State m_state;

  uint64_t m_gId=0;
  GraphTypePtr m_graphType;

  // This is use to sanity check what the client is giving us, and avoid duplicate ids.
  // Used while adding device instances, then again for edge instances.
  // For a trusted client it could be skipped.
  std::unordered_set<std::string> m_seenIds;

  // This holds the actual ids.
  std::vector<std::string> m_deviceIds;

  bool m_sanityChecks=true;

  std::string stateName(State s)
  {
    switch(s){
    case State_Graph: return "Graphs";
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

  bool is_zero(size_t n, const void *p)
  {
    const char *pp=(const char*)p;
    const char *pe=pp+n;
    while(pe!=pp){
      if(*pp){
        return false;
      }
      ++pp;
    }
    return true;
  }

  void writeTypedData(const TypedDataSpecPtr &spec, const TypedDataPtr &data, const char *name)
  {
    if( data ){
      if(is_zero(data.payloadSize(), data.payloadPtr()) && spec->is_default(data)){
        // safe to omit. Things are complicated with V4/V3 conversions and defaults.
      }else{
        std::string value=spec->toXmlV4ValueSpec(data, m_minorFormatVersion);

        xmlTextWriterWriteAttribute(m_dst, (const xmlChar*)name, (const xmlChar*)value.c_str());
      }
    }
  }

  void writeInputPin(InputPinPtr ip)
  {
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"InputPin");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"name", (const xmlChar *)ip->getName().c_str() );
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"messageTypeId", (const xmlChar *)ip->getMessageType()->getId().c_str() );

    writeTypedDataSpec(ip->getPropertiesSpec(), "Properties");

    if(!ip->getDeviceType()->isExternal()){
      writeTypedDataSpec(ip->getStateSpec(), "State");

      xmlTextWriterStartElement(m_dst, (const xmlChar *)"OnReceive");
      xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)ip->getHandlerCode().c_str());
      xmlTextWriterEndElement(m_dst);
    }

    xmlTextWriterEndElement(m_dst);
  }

  void writeOutputPin(OutputPinPtr op)
  {
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"OutputPin");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"name", (const xmlChar *)op->getName().c_str() );
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"messageTypeId", (const xmlChar *)op->getMessageType()->getId().c_str() );

    if(op->isIndexedSend()){
      xmlTextWriterWriteAttribute(m_dst, (const xmlChar*)"indexed", (const xmlChar*)"true");
    }

    if(!op->getDeviceType()->isExternal()){

      xmlTextWriterStartElement(m_dst, (const xmlChar *)"OnSend");
      xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)op->getHandlerCode().c_str());
      xmlTextWriterEndElement(m_dst);
    }

    xmlTextWriterEndElement(m_dst);
  }

  void writeDeviceType(DeviceTypePtr deviceType)
  {
    xmlTextWriterStartElement(m_dst, deviceType->isExternal() ? (const xmlChar *)"ExternalType" : (const xmlChar *)"DeviceType");

    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)deviceType->getId().c_str() );

    writeTypedDataSpec(deviceType->getPropertiesSpec(), "Properties");

    if(!deviceType->isExternal()){
      writeTypedDataSpec(deviceType->getStateSpec(), "State");

      auto sharedCode=deviceType->getSharedCode();
      xmlTextWriterStartElement(m_dst, (const xmlChar *)"SharedCode");
      xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)sharedCode.c_str());
      xmlTextWriterEndElement(m_dst);
    }

    for(auto ip : deviceType->getInputs()){
      writeInputPin(ip);
    }

    for(auto op : deviceType->getOutputs()){
      writeOutputPin(op);
    }

    if(!deviceType->isExternal()){
      xmlTextWriterStartElement(m_dst, (const xmlChar *)"ReadyToSend");
      xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)deviceType->getReadyToSendCode().c_str());
      xmlTextWriterEndElement(m_dst);

      xmlTextWriterStartElement(m_dst, (const xmlChar *)"OnInit");
      xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)deviceType->getOnInitCode().c_str());
      xmlTextWriterEndElement(m_dst);

      xmlTextWriterStartElement(m_dst, (const xmlChar *)"OnHardwareIdle");
      xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)deviceType->getOnHardwareIdleCode().c_str());
      xmlTextWriterEndElement(m_dst);

      xmlTextWriterStartElement(m_dst, (const xmlChar *)"OnDeviceIdle");
      xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)deviceType->getOnDeviceIdleCode().c_str());
      xmlTextWriterEndElement(m_dst);

      
    }

    xmlTextWriterEndElement(m_dst);
  }

  void writeMessageType(MessageTypePtr messageType)
  {
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"MessageType");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)messageType->getId().c_str() );

    writeTypedDataSpec(messageType->getMessageSpec(), "Message");

    xmlTextWriterEndElement(m_dst);
  }

  void writeTypedDataSpecElementTuple(std::shared_ptr<TypedDataSpecElementTuple> e)
  {
    for(auto elt : *e){
      std::vector<unsigned> indices;

      auto base=elt;
      auto tip=base;
      while(tip->isArray()){
        auto a=std::dynamic_pointer_cast<TypedDataSpecElementArray>(tip);
        assert(a);

        indices.push_back(a->getElementCount());
        tip=a->getElementType();
      }

      if(tip->isScalar()){
        auto s=std::dynamic_pointer_cast<TypedDataSpecElementScalar>(tip);
        assert(s);
        xmlTextWriterWriteFormatString(m_dst, "%s %s", s->getTypeName().c_str(), base->getName().c_str()); 
      }else if(tip->isTuple()){
        auto t=std::dynamic_pointer_cast<TypedDataSpecElementTuple>(tip);
        assert(t);
        xmlTextWriterWriteString(m_dst, (const xmlChar*)"struct {\n");
        writeTypedDataSpecElementTuple(t);
        xmlTextWriterWriteFormatString(m_dst, "} %s", base->getName().c_str());
      }else{
        throw std::runtime_error("Unexpected type.");
      }

      for(auto n : indices){
        xmlTextWriterWriteFormatString(m_dst, "[%u]", n);
      }

      xmlTextWriterWriteString(m_dst, (const xmlChar*)";\n");
    }
  }

  void writeTypedDataSpec(TypedDataSpecPtr spec, const char *name)
  {
    xmlTextWriterStartElement(m_dst, (const xmlChar *)name);
    if(spec){
      xmlTextWriterStartCDATA(m_dst);
      writeTypedDataSpecElementTuple(spec->getTupleElement());
      xmlTextWriterEndCDATA(m_dst);
    }
    xmlTextWriterEndElement(m_dst);
  }
  
  void writeMetaData(const std::string &key, const rapidjson::Document &data)
  {
    if(data.IsNull() || (data.MemberCount()==0)){
      return;
    }

    if(!data.IsObject())
      throw std::runtime_error("Metadata must be null or an object.");

    xmlTextWriterStartElement(m_dst, (const xmlChar*)"Metadata");

    xmlTextWriterWriteAttribute(m_dst, (const xmlChar*)"key", (const xmlChar*)key.c_str());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data.Accept(writer);
    
    std::string json(buffer.GetString(), buffer.GetSize());
    assert(json.size()>2);
    assert(json[json.size()-1]=='}');
    assert(json[0]=='{');
        
    json.resize(json.size()-1); // Chop off the final '}'. Probably no realloc

    xmlTextWriterWriteAttribute(m_dst, (const xmlChar*)"value",  (const xmlChar *)(json.c_str()+1));

    xmlTextWriterEndElement(m_dst);
  }

  virtual void writeGraphType(const GraphTypePtr &graphType)
  {
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"GraphType");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)graphType->getId().c_str() );

    writeMetaData("v3-legacy-metadata--graph-type", graphType->getMetadata());
    for(auto mt : graphType->getMessageTypes()){
      writeMetaData("v3-legacy-metadata--message-type--"+mt->getId(), mt->getMetadata());
    }
    for(auto dt : graphType->getDeviceTypes()){
      writeMetaData("v3-legacy-metadata--device-type--"+dt->getId(), dt->getMetadata());
      for(auto ip : dt->getInputs()){
        writeMetaData("v3-legacy-metadata--device-type--input-port--"+dt->getId()+"-"+ip->getName(), ip->getMetadata());
      }
      for(auto op : dt->getInputs()){
        writeMetaData("v3-legacy-metadata--device-type--output-port--"+dt->getId()+"-"+op->getName(), op->getMetadata());
      }
    }

    writeTypedDataSpec(graphType->getPropertiesSpec(), "Properties");

    auto sharedCode=graphType->getSharedCode();
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"SharedCode");
    xmlTextWriterWriteCDATA(m_dst, (const xmlChar *)sharedCode.c_str());
    xmlTextWriterEndElement(m_dst);

    

    xmlTextWriterStartElement(m_dst, (const xmlChar *)"MessageTypes");
    for(unsigned i=0; i<graphType->getMessageTypeCount(); i++){
      writeMessageType(graphType->getMessageType(i));
    }
    xmlTextWriterEndElement(m_dst);

    xmlTextWriterStartElement(m_dst, (const xmlChar *)"DeviceTypes");
    for(unsigned i=0; i<graphType->getDeviceTypeCount(); i++){
      writeDeviceType(graphType->getDeviceType(i));
    }
    xmlTextWriterEndElement(m_dst);

    xmlTextWriterEndElement(m_dst);
  }
public:
    GraphSAXWriterV4(xmlTextWriterPtr dst, bool sanityChecks=true)
      : m_dst(dst)
      , m_state(State_Graph)
      , m_sanityChecks(sanityChecks)
  {
      xmlTextWriterSetIndent(m_dst, 1);
      xmlTextWriterStartDocument(m_dst, NULL, NULL, NULL);
      xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar*)"Graphs", m_ns);
      xmlTextWriterWriteAttribute(m_dst, (const xmlChar*)"formatMinorVersion", (const xmlChar*)"0");
  }


  virtual ~GraphSAXWriterV4()
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

  virtual void onGraphType(const GraphTypePtr &graphType) override
  {
    if(m_graphType){
      if(m_graphType->getId()!=graphType->getId())
        throw std::runtime_error("V4 graphs can only contain one graph type.");
    }else{
      writeGraphType(graphType);
      m_graphType=graphType;
    }
  }
  
  virtual uint64_t onBeginGraphInstance(
    const GraphTypePtr &graphType,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ) override
  {
    if(m_graphType){
      if(m_graphType->getId() != graphType->getId()){
        throw std::runtime_error("Already emitted a different graph type.");
      }
    }else{
     writeGraphType(graphType);
     m_graphType=graphType; 
    }

    moveState(State_Graph, State_GraphInstance);
    m_graphType=graphType;

    xmlTextWriterStartElement(m_dst, (const xmlChar *)"GraphInstance");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id.c_str());
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"graphTypeId", (const xmlChar *)graphType->getId().c_str());

    writeTypedData(graphType->getPropertiesSpec(), properties, "P");
    writeMetaData("v3-legacy-metadata--graph-instance", metadata);
    
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
   const TypedDataPtr &state,
   rapidjson::Document &&metadata
   ) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }
    if(m_state!=State_DeviceInstances){
      throw std::runtime_error("Not in the DeviceInstances state.");
    }
    
    if(m_sanityChecks){
      auto it_inserted=m_seenIds.insert(id);
      if(!it_inserted.second)
        throw std::runtime_error("A device called "+id+" has already been added.");
    }
    
    m_deviceIds.push_back(id);
    uint64_t idNum=m_deviceIds.size()-1;

    xmlTextWriterStartElement(m_dst, dt->isExternal() ? (const xmlChar *)"ExtI" : (const xmlChar *)"DevI");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id.c_str());
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"type", (const xmlChar *)dt->getId().c_str());
    
    writeTypedData(dt->getPropertiesSpec(), properties, "P");
    if(!dt->isExternal()){
      writeTypedData(dt->getStateSpec(), state, "S");
    }

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

  void onEdgeInstance
  (
   uint64_t gId,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   int sendIndex,
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
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

    if(sendIndex!=-1 && srcPin->isIndexedSend()){
      throw std::runtime_error("Attempt to specify sendIndex on non-indexed output pin.");
    }

    if(m_sanityChecks){
      auto it_inserted=m_seenIds.insert(id);
      if(!it_inserted.second)
        throw std::runtime_error("An edge called "+id+" has already been added.");
    }

    //    fprintf(stderr, "  adding : %s\n", id.c_str());
    
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"EdgeI");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"path", (const xmlChar *)id.c_str());

    if(sendIndex!=-1){
      std::string tmp=std::to_string(sendIndex);
      xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"sendIndex", (const xmlChar *)tmp.c_str());
    }

    writeTypedData(dstPin->getPropertiesSpec(), properties, "P");
    writeTypedData(dstPin->getStateSpec(), state, "S");

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

  static std::shared_ptr<GraphLoadEvents> createSAXWriterV4OnFile(const std::string &path, const sax_writer_options &options=sax_writer_options{})
  {
    if(!options.format.empty() && options.format!="v4"){
      throw std::runtime_error("Attempt to create SAX writer with wrong format specified.");
    }

    bool compress=options.compress;
    if(path.size() > 3 && path.substr(path.size()-3)==".gz" ){
      compress=true;
    }

    xmlTextWriterPtr dst=xmlNewTextWriterFilename(path.c_str(), compress?1:0);
    if(!dst)
      throw std::runtime_error("createSAXWriterOnFile("+path+") - Couldn't create xmlTextWriter");

    return std::make_shared<detail::GraphSAXWriterV4>(dst, options.sanity);
  }
};

}; // detail




#endif
