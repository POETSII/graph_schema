#ifndef graph_persist_sax_writer_base85_hpp
#define graph_persist_sax_writer_base85_hpp

#include "graph.hpp"
#include "graph_persist_sax_writer_v4.hpp"
#include "base85_codec.hpp"

#include "robin_hood.hpp"

#include <cmath>

namespace detail
{

struct graph_type_info
{
  static bool is_default(size_t n, const char *p, const std::vector<char> &def)
  {
    if(n!=def.size()){
      throw std::runtime_error("Data size does not match default.");
    }
    return !memcmp(p, def.data(), def.size());
  }

  struct output_pin_info_t
  {
    OutputPinPtr pin;
  };

  struct input_pin_info_t
  {
    std::vector<char> default_properties;
    std::vector<char> default_state;
    InputPinPtr pin;

    bool is_default_properties(size_t n, const char *p) const
    { return is_default(n, p, default_properties); }

    bool is_default_state(size_t n, const char *p) const
    { return is_default(n, p, default_state); }
  };

  struct device_type_info_t
  {
    unsigned index;
    std::string id;
    std::vector<char> default_properties;
    std::vector<char> default_state;
    unsigned input_pin_bits;
    unsigned output_pin_bits;
    std::vector<input_pin_info_t> input_pins;
    std::vector<output_pin_info_t> output_pins;
    DeviceTypePtr deviceType;

    bool is_default_properties(size_t n, const char *p) const
    { return is_default(n, p, default_properties); }

    bool is_default_state(size_t n, const char *p) const
    { return is_default(n, p, default_state); }
  };
  
  robin_hood::unordered_flat_map<std::string,uint32_t> device_id_to_index;
  std::vector<device_type_info_t> device_types;
  unsigned bits_per_device_type_index;
  unsigned max_input_pin_bits;
  unsigned max_output_pin_bits;

private:
  std::vector<char> get_typed_data_default_as_vector(TypedDataSpecPtr spec)
  {
    std::vector<char> res;
    if(spec && spec->payloadSize()!=0){
      res.resize(spec->payloadSize());
      auto def=spec->create();
      memcpy(res.data(), def.payloadPtr(), def.payloadSize());
    }
    return res;
  }
public:

  graph_type_info()
  {}

  graph_type_info &operator=(graph_type_info &&) = default;

  graph_type_info(GraphTypePtr graph)
    : max_input_pin_bits(0)
    , max_output_pin_bits(0)
  {
    for(auto dt : graph->getDeviceTypes()){
      device_type_info_t di;
      di.index=device_types.size();
      di.id=dt->getId();
      di.default_properties=get_typed_data_default_as_vector(dt->getPropertiesSpec());
      di.default_state=get_typed_data_default_as_vector(dt->getStateSpec());
      di.input_pin_bits=(unsigned)std::ceil(std::log2(dt->getInputCount()+1));
      di.output_pin_bits=(unsigned)std::ceil(std::log2(dt->getOutputCount()+1));
      max_input_pin_bits=std::max(max_input_pin_bits, di.input_pin_bits);
      max_output_pin_bits=std::max(max_output_pin_bits, di.output_pin_bits);
      di.deviceType=dt;
      for(auto ip : dt->getInputs()){
        di.input_pins.push_back(input_pin_info_t{
          get_typed_data_default_as_vector(ip->getPropertiesSpec()),
          get_typed_data_default_as_vector(ip->getStateSpec()),
          ip
        });
      }
      for(auto op : dt->getOutputs()){
        di.output_pins.push_back(output_pin_info_t{
          op
        });
      }

      if(!device_id_to_index.insert({di.id, di.index}).second){
        throw std::runtime_error("Duplicate device type id.");
      }
      device_types.push_back(std::move(di));
    }
    bits_per_device_type_index=(unsigned)std::ceil(std::log2(graph->getDeviceTypeCount()+1));
  }

  const device_type_info_t &get_device_info(const std::string &id) const
  {
    unsigned index=device_id_to_index.at(id); // dont trust incoming ids
    return device_types[index]; // Do trust our data
  }
};

class GraphSAXWriterBase85
  : public GraphSAXWriterV4
{
public:
  enum DeviceFlags
  {
    DeviceInst_ChangeDeviceType = 1,
    DeviceInst_NotIncrementalId = 2,
    DeviceInst_HasProperties = 4,
    DeviceInst_HasState = 8
  };

  enum EdgeFlags
  {
    EdgeInst_ChangeDestEndpoint = 1,
    EdgeInst_ChangeSrcEndpoint = 2,
    EdgeInst_ChangeBothEndpoints = EdgeInst_ChangeDestEndpoint | EdgeInst_ChangeSrcEndpoint,
    EdgeInst_HasProperties = 4,
    EdgeInst_HasState = 8,
    EdgeInst_HasIndex = 0x10
  };

protected:
  const xmlChar *m_ns=(const xmlChar *)"https://poets-project.org/schemas/virtual-graph-schema-v4";

  Base85Codec m_codec;

  graph_type_info m_gi;

  const size_t MAX_LINE_SIZE = 1024;
  const size_t MAX_CHUNK_SIZE = 65536;

  std::vector<char> m_lineBuffer;
  std::vector<std::vector<char>> m_lineBufferChunk;
  size_t m_lineBufferChunkSize=0;
  std::vector<std::vector<char>> m_lineBufferCache;

  std::vector<uint16_t> m_device_instance_to_device_type_index;

  std::vector<std::string> m_device_ids; // debug only

  unsigned m_prevDeviceType=0;
  std::string m_prevDeviceId;

  unsigned m_bitsPerDeviceId;
  unsigned m_bitsPerDstEndpoint;
  unsigned m_bitsPerSrcEndpoint;

  uint64_t m_prevDstEndpoint=0;
  uint64_t m_prevSrcEndpoint=0;

  bool is_id_increment(const std::string &now, const std::string &prev) const
  {
    if(prev.empty()){
      return false;
    }
    assert(!now.empty());

    return now.back()==prev.back()+1;
  }

  void maybe_flush()
  {
    if(m_lineBuffer.size() >= MAX_LINE_SIZE){
      flush_line();
    }
  }

  void flush_line()
  {
    if(!m_lineBuffer.empty()){
      m_lineBufferChunkSize += m_lineBuffer.size();
      m_lineBufferChunk.push_back(std::move(m_lineBuffer));
      
      if(m_lineBufferCache.empty()){
        m_lineBuffer.clear();
        m_lineBuffer.reserve((MAX_LINE_SIZE*3)/2);
      }else{
        m_lineBuffer=std::move(m_lineBufferCache.back());
        m_lineBufferCache.pop_back();
      }
    }
    if(m_lineBufferChunkSize>=MAX_CHUNK_SIZE){
      flush_chunk();
    }
  }

  void flush_chunk()
  {
    if(!m_lineBuffer.empty()){
     flush_line();
    }
    if(!m_lineBufferChunk.empty()){
      xmlTextWriterStartElement(m_dst, (const xmlChar*)"C");
      xmlTextWriterStartCDATA(m_dst);
      for(auto &l : m_lineBufferChunk){
        for(char c : l){
          assert(isprint(c));
        }
        xmlTextWriterWriteRawLen(m_dst, (const xmlChar*)l.data(), l.size());
        xmlTextWriterWriteString(m_dst, (const xmlChar*)"\n");
        l.clear();
        m_lineBufferCache.push_back(std::move(l));
      }
      m_lineBufferChunk.clear();
      m_lineBufferChunkSize=0;
      xmlTextWriterEndCDATA(m_dst);
      xmlTextWriterEndElement(m_dst);

      // Reset so that each chunk can be decoded independently
      m_prevDeviceType=0;
      m_prevDeviceId.clear();
      m_prevDstEndpoint=0;
      m_prevSrcEndpoint=0;
    }
  }

  void flush()
  {
    flush_chunk();
  }
public:
    GraphSAXWriterBase85(xmlTextWriterPtr dst, bool sanityChecks=true)
      : GraphSAXWriterV4(dst, sanityChecks)
  {
  }

  
  virtual uint64_t onBeginGraphInstance(
    const GraphTypePtr &graphType,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ) override
  {
    m_gi=graph_type_info(graphType);
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

    xmlTextWriterStartElement(m_dst, (const xmlChar *)"GraphInstanceBase85");
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id.c_str());
    xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"graphTypeId", (const xmlChar *)graphType->getId().c_str());

    GraphSAXWriterBase85::writeTypedData(graphType->getPropertiesSpec(), properties, "P");
    writeMetaData("v3-legacy-metadata--graph-instance", metadata);
    
    return ++m_gId;
  }

  virtual void onBeginDeviceInstances(uint64_t gId) override
  {
    moveState(State_GraphInstance, State_DeviceInstances);
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"DeviceInstancesBase85");
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

    uint32_t index=m_device_instance_to_device_type_index.size();
    if(index > 1u<<31){
      throw std::runtime_error("This encoding assumes that there are at most 2^31 devices instances. Needs auditing for more.");
    }
    uint32_t device_type_index=m_gi.device_id_to_index.at(dt->getId());
    m_device_instance_to_device_type_index.push_back(device_type_index);
    #ifndef NDEBUG
    m_device_ids.push_back(id);
    #endif

    auto iti=m_seenIds.insert( id );
    if(!iti.second){
        throw std::runtime_error("A device called "+id+" has already been added.");
    }

    const auto &di=m_gi.get_device_info(dt->getId());
    
    unsigned flags=0;
    if(device_type_index!=m_prevDeviceType){
      flags |= DeviceInst_ChangeDeviceType;
      m_prevDeviceType=device_type_index;
    }

    if(!is_id_increment(id, m_prevDeviceId)){
      flags |= DeviceInst_NotIncrementalId;
    }
    m_prevDeviceId=id;

    if(properties && !di.is_default_properties(properties.payloadSize(), (const char*)properties.payloadPtr())){
      flags |= DeviceInst_HasProperties;
    }

    if(state && !di.is_default_state(state.payloadSize(), (const char*)state.payloadPtr())){
      flags |= DeviceInst_HasState;
    }

    m_codec.encode_digit(flags, m_lineBuffer);

    if(flags&DeviceInst_ChangeDeviceType){
      m_codec.encode_bits(m_gi.bits_per_device_type_index, device_type_index, m_lineBuffer);
    }

    if(flags&DeviceInst_NotIncrementalId){
      m_codec.encode_c_identifier(id, m_lineBuffer);
    }

    if(flags&DeviceInst_HasProperties){
      m_codec.encode_bytes(properties.payloadSize(), properties.payloadPtr(), m_lineBuffer);
    }

    if(flags&DeviceInst_HasState){
      m_codec.encode_bytes(state.payloadSize(), state.payloadPtr(), m_lineBuffer);
    }

    maybe_flush();

    return index;
  }

  virtual void onEndDeviceInstances(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }

    flush();

    moveState(State_DeviceInstances, State_PostDeviceInstances);
    xmlTextWriterEndElement(m_dst);

    m_seenIds.clear();

    m_bitsPerDeviceId=(unsigned)std::ceil(std::log2(m_device_instance_to_device_type_index.size()+1));
    m_bitsPerDstEndpoint=m_gi.max_input_pin_bits+m_bitsPerDeviceId;
    m_bitsPerSrcEndpoint=m_gi.max_output_pin_bits+m_bitsPerDeviceId;
  }

  virtual void onBeginEdgeInstances(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }
    moveState(State_PostDeviceInstances, State_EdgeInstances);
    xmlTextWriterStartElement(m_dst, (const xmlChar *)"EdgeInstancesBase85");
  }

  virtual void onEdgeInstance
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
    ////////////////////////////////
    // Error checks

    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }
    if(m_state!=State_EdgeInstances){
      throw std::runtime_error("Not in the EdgeInstances state.");
    }

    if(dstDevInst >= m_device_instance_to_device_type_index.size() || srcDevInst >= m_device_instance_to_device_type_index.size() ){
      throw std::runtime_error("Invalid device instance index.");
    }

    /////////////////////
    // Extract various info

    unsigned dst_dev_type_index=m_device_instance_to_device_type_index[dstDevInst];

    const auto &dst_di = m_gi.device_types[dst_dev_type_index];

    unsigned destPinIndex=dstPin->getIndex();
    unsigned srcPinIndex=srcPin->getIndex();

    const auto &dst_pi=dst_di.input_pins.at(destPinIndex);

    unsigned dstEndpoint=(dstDevInst<<m_gi.max_input_pin_bits) + destPinIndex;
    unsigned srcEndpoint=(srcDevInst<<m_gi.max_output_pin_bits) + srcPinIndex;

    //////////////////////////////
    // Build flags

    unsigned flags=0;

    if(dstEndpoint!=m_prevDstEndpoint){
      flags |= EdgeInst_ChangeDestEndpoint;
      m_prevDstEndpoint=dstEndpoint;
    }

    if(srcEndpoint!=m_prevSrcEndpoint){
      flags |= EdgeInst_ChangeSrcEndpoint;
      m_prevSrcEndpoint=srcEndpoint;
    }

    if(sendIndex != -1){
      flags |= EdgeInst_HasIndex;
    }

    if(properties && !dst_pi.is_default_properties(properties.payloadSize(), (const char*)properties.payloadPtr())){
      flags |= EdgeInst_HasProperties; 
    }

    if(state && !dst_pi.is_default_state(state.payloadSize(), (const char *)state.payloadPtr())){
      flags |= EdgeInst_HasState; 
    }
    
    //////////////////////////////////
    // Do the actual encoding

    unsigned bOff=m_lineBuffer.size();

    m_codec.encode_digit(flags, m_lineBuffer);

    if( (flags & EdgeInst_ChangeBothEndpoints) == EdgeInst_ChangeBothEndpoints ){
      uint64_t total=(uint64_t(dstEndpoint)<<m_bitsPerSrcEndpoint) + srcEndpoint;
      //fprintf(stderr, "  total=%llu\n", (unsigned long long)total);
      m_codec.encode_bits( m_bitsPerDstEndpoint+m_bitsPerSrcEndpoint, total, m_lineBuffer  );
    }else if(flags & EdgeInst_ChangeDestEndpoint){
      m_codec.encode_bits( m_bitsPerDstEndpoint, dstEndpoint, m_lineBuffer );
    }else if(flags & EdgeInst_ChangeSrcEndpoint){
      m_codec.encode_bits( m_bitsPerSrcEndpoint, srcEndpoint, m_lineBuffer );
    }

    /*fprintf(stderr, "F=%u, %s:%s-%s:%s = %u:%u-%u:%u\n",
        (flags & EdgeInst_ChangeBothEndpoints),
        m_device_ids[dstDevInst].c_str(), dstPin->getName().c_str(),
        m_device_ids[srcDevInst].c_str(), srcPin->getName().c_str(),
        (unsigned)dstDevInst, destPinIndex,
        (unsigned)srcDevInst, srcPinIndex
    );*/

    if(flags & EdgeInst_HasIndex){
      m_codec.encode_bits( m_bitsPerDeviceId, sendIndex, m_lineBuffer);
    }

    if(flags & EdgeInst_HasProperties){
      m_codec.encode_bytes(properties.payloadSize(), properties.payloadPtr(), m_lineBuffer);
    }

    if(flags & EdgeInst_HasState){
      m_codec.encode_bytes(state.payloadSize(), state.payloadPtr(), m_lineBuffer);
    }

    unsigned eOff=m_lineBuffer.size();

    /*fprintf(stderr, "chars=");
    for(auto o=bOff; o<eOff; o++){
      fprintf(stderr, "%c", m_lineBuffer[o]);
    }
    fprintf(stderr, "\n");
    */

    maybe_flush();
  }

  virtual void onEndEdgeInstances(uint64_t gId) override
  {
    if(gId!=m_gId){
      throw std::runtime_error("Incorrect graph id.");
    }

    flush();

    moveState(State_EdgeInstances, State_PostEdgeInstances);
    xmlTextWriterEndElement(m_dst);
  }
};

}; // detail


std::shared_ptr<GraphLoadEvents> createSAXWriterBase85OnFile(const std::string &path, const sax_writer_options &options=sax_writer_options{})
{
  if(!options.format.empty() && options.format!="base85"){
    throw std::runtime_error("Attempt to create SAX writer with wrong format specified.");
  }

  bool compress=options.compress;
  if(path.size() > 3 && path.substr(path.size()-3)==".gz" ){
    compress=true;
  }

  xmlTextWriterPtr dst=xmlNewTextWriterFilename(path.c_str(), compress?1:0);
  if(!dst)
    throw std::runtime_error("createSAXWriterOnFile("+path+") - Couldn't create xmlTextWriter");

  return std::make_shared<detail::GraphSAXWriterBase85>(dst, options.sanity);
}

#endif
