#ifndef graph_persist_binary_writer_hpp
#define graph_persist_binary_writer_hpp

#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "graph_persist.hpp"

#include "robin_hood.hpp"

struct BinarySink
{
public:
  uint64_t write_pos=0;
  std::vector<char> buffer;

  FILE *m_file;

  ~BinarySink()
  {
    if(buffer.size()>0){
      flush();
    }
  }

    uint64_t write_i64(uint64_t v)
    {
      uint8_t data[10];
      uint8_t *dst=data;

      while(1){
        uint64_t digit=v&0x7F;
        v=v>>7;
        if(v==0){
          *dst++=v;
          break;
        }else{
          *dst++=v|0x80;
        }
      }

      return write(dst-data, data);
    }

    template<class T>
    uint64_t write_scalar(const T &v)
    {
      return write(sizeof(v), &v);
    }

    uint64_t write_str(const char *data)
    {
      unsigned len=strlen(data);
      auto res=write_i64(len);
      write(len, data);
      return res;
    }

    uint64_t write(size_t len, const void *data)
    {
      auto begin=write_pos;
      const char *p=(const char *)data;
      buffer.insert(buffer.end(), p, p+len);
      write_pos += len;

      return begin;
    }

    void flush()
    {
      size_t done=fwrite(&buffer[0], 1, buffer.size(), m_file);
      if(done!=buffer.size()){
        throw std::runtime_error("Couldnt write to sink file.");
      }
      buffer.clear();
    }

    void maybe_flush()
    {
      if(buffer.size() > 4096){
        flush();
      }
    }

    uint64_t tell() const
    {
      return write_pos;
    }
};



class BinaryPayloadWriter
    : public GraphLoadEvents
{
  const std::array<uint8_t,16> BEGIN_GRAPH_SENTINEL{0xbc,0x29 ,0x70,0xb2 ,0x87,0xac ,0x22,0x6c ,0xcd,0x62 ,0xc1,0x21 ,0xe2,0x0e ,0xef,0x20};

  const std::array<uint8_t,16> BEGIN_DEVICES_SENTINEL{0x1a,0x99,0xa3,0xf2,0xf1,0x46,0xe3,0x20,0xf9,0x02,0x67,0x1e,0x67,0xba,0x5a,0x62};
  const std::array<uint8_t,16> CONTINUE_DEVICES_SENTINEL{0xa2,0x3b,0x16,0x82,0xbb,0x27,0x4c,0x80,0xb6,0x53,0xf8,0x83,0x93,0xdf,0xb3,0xae};
  const std::array<uint8_t,16> END_DEVICES_SENTINEL{0x34,0x5b, 0x38,0xd6, 0x2a,0xd6, 0x6a,0x68, 0x73,0x0e, 0xa1,0xf3, 0x99,0x8c, 0x3f,0xad,};

  const std::array<uint8_t,16> BEGIN_EDGES_SENTINEL{0x01,0x0b,0x1d,0x91, 0x07,0xca, 0xff,0x5f, 0x14,0x92, 0x07,0xe1, 0x36,0xdb, 0x67,0x51};
  const std::array<uint8_t,16> CONTINUE_EDGES_SENTINEL{0x10,0x78,0xad,0xf4,0xfc,0xd7,0xdd,0x9c,0xb2,0x1d,0x16,0x96,0x7e,0x75,0x92,0xdb};
  const std::array<uint8_t,16> END_EDGES_SENTINEL{0xc2,0xdf, 0xf9,0x5e, 0x96,0xa6, 0xa2,0x08, 0xee,0xfe, 0x40,0xf1, 0xbf,0x74, 0x4d,0x05,};

  const std::array<uint8_t,16> END_GRAPH_SENTINEL{0xe4,0x0b, 0x8d,0xef, 0x26,0x2e, 0x27,0x98, 0x64,0x25, 0xa3,0x90, 0xdc,0x73, 0x44,0x96};


private:
    std::string path

    BinarySink &m_sink;

    std::array<uint8_t,16> m_graphTag;

    uint32_t m_nextDeviceId=0;
    uint64_t m_nextEdgeIndex=0;

    std::unordered_map<std::string,GraphTypePtr> m_graphTypes;

    robin_hood::unordered_flat_map<std::string,unsigned> device_type_id_to_index;

    // Edge encoding

    unsigned m_bitsPerDeviceId;
    unsigned m_bitsPerPinIndex;
    unsigned m_bytesPerFullEndpoint;
    unsigned m_bytesPerDevOnlyEndpoint;
    unsigned m_bytesPerPinOnlyEndpoint;

    std::string m_prevDeviceId;
    unsigned m_prevDeviceTypeIndex=0;

    uint32_t m_prevDstDevInst=0;
    uint32_t m_prevDstDevPin=0;
    uint32_t m_prevSrcDevInst=0;
    uint32_t m_prevSrcDevPin=0;
public:

  BinaryPayloadWriter(BinarySink &sink)
    : m_sink(sink)
  {
    const char *header="POETSPackedBinaryGraphInstanceV0\n";
    m_sink.write(strlen(header), header);
    m_sink.write<char>(0);

    m_sink.write()
  }

  virtual void onGraphType(const GraphTypePtr &graph)
  {}

  //! Tells the consumer that a new graph is starting
  virtual uint64_t onBeginGraphInstance(
    const GraphTypePtr &graph,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ){
    unsigned maxPinIndex=0;
    unsigned deviceTypeIndex=0;
    for(auto dt : graph->getDeviceTypes()){
      maxPinIndex=std::max(maxPinIndex, std::max(dt->getInputCount(), dt->getOutputCount()));
      device_type_id_to_index[dt->getId()]=deviceTypeIndex;
      deviceTypeIndex++;
    }
    m_bitsPerPinIndex = (unsigned)std::ceil(std::log2(maxPinIndex+1));

    m_sink.write(BEGIN_GRAPH_SENTINEL.size(), &BEGIN_GRAPH_SENTINEL[0]);



    return 0;
  }

  //! The graph is now complete
  virtual void onEndGraphInstance(uint64_t /*graphToken*/)
  {
    m_sink.write(END_GRAPH_SENTINEL.size(), &END_GRAPH_SENTINEL[0]);
  }

  //! The device instances within the graph instance will follow
  virtual void onBeginDeviceInstances(uint64_t /*graphToken*/)
  {
    m_sink.write(BEGIN_DEVICES_SENTINEL.size(), &BEGIN_DEVICES_SENTINEL[0]);
  }

  //! There will be no more device instances in the graph.
  virtual void onEndDeviceInstances(uint64_t /*graphToken*/)
  {
    m_sink.write_scalar(flags);
    m_sink.write(END_DEVICES_SENTINEL.size(), &END_DEVICES_SENTINEL[0]);
  }

  enum DeviceFlags{
    DeviceFlags_RepeatDeviceType = 0x1,
    DeviceFlags_HasProperties = 0x2,
    DeviceFlags_HasState = 0x4,

    DeviceFlags_IdFull = 0,
    DeviceFlags_IdIncDec = 0x8,
    DeviceFlags_IdIncHexLower = 0x10,
    DeviceFlags_IdIncHexUpper = 0x18,

    DeviceFlags_IdIncMask = 0x18,

    DeviceFlags_HasSentinel = 0x20
  };  

  DeviceFlags calc_device_id_inc(const std::string &now, const std::string &prev)
  {
    assert(!now.empty());

    if(now.size()!=prev.size()){
      return DeviceFlags_IdFull;
    }

    char cn=now.back();
    char cp=prev.back();

    if(!isxdigit(cn)){
      return DeviceFlags_IdFull;
    }
    if(!isxdigit(cp)){
      return DeviceFlags_IdFull;
    }
    if(strncmp(now.data(), prev.data(), now.size()-1)){
      return DeviceFlags_IdFull;
    }

    // TODO : This misses wrap-around, but we get a large amount of the benefit here.

    if( ('1' <= cn && cn <= '9') && (cn==cp+1)){
      return DeviceFlags_IdIncDec;
    }
    if( ('b' <= cn && cn <= 'f') && (cn==cp+1)){
      return DeviceFlags_IdIncHexLower;
    }
    if( ('B' <= cn && cn <= 'F') && (cn==cp+1)){
      return DeviceFlags_IdIncHexUpper;
    }

    if( ('a' == cn) && ('9'==cp)){
      return DeviceFlags_IdIncHexLower;
    }
    if( ('A' == cn) && ('9'==cp)){
      return DeviceFlags_IdIncHexUpper;
    }

    return DeviceFlags_IdFull;
  }

  // Tells the consumer that a new instance is being added
  /*! The return value is a unique identifier that means something
    to the consumer. */
  virtual uint64_t onDeviceInstance
  (
   uint64_t graphInst,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
   rapidjson::Document &&metadata=rapidjson::Document()
  )
  {
    // Note that index 0 is never used - we start at 1, and reserve 0
    uint32_t index=++m_nextDeviceId;

    uint8_t flags=0;

    auto deviceTypeIndex=device_type_id_to_index.at(dt->getId());
    if(deviceTypeIndex!=m_prevDeviceTypeIndex){
      m_prevDeviceTypeIndex=deviceTypeIndex;
    }else{
      flags |= DeviceFlags_RepeatDeviceType;
    }

    if(!(properties.empty() || dt->getPropertiesSpec()->is_default(properties))){
      flags |= DeviceFlags_HasProperties;
    }

    if(!(state.empty() || dt->getStateSpec()->is_default(state))){
      flags |= DeviceFlags_HasState;
    }

    flags |= calc_device_id_inc(id, m_prevDeviceId);
    m_prevDeviceId=id;

    if(1==(index&0xFFFF)){
      flags |= DeviceFlags_HasSentinel;
    }

    m_sink.write_scalar<uint8_t>(flags);

    if(flags&DeviceFlags_HasSentinel){
      m_sink.write(CONTINUE_DEVICES_SENTINEL.size(), &CONTINUE_DEVICES_SENTINEL[0]);
    }

    if(!(flags&DeviceFlags_RepeatDeviceType)){
      m_sink.write_i64(deviceTypeIndex);
    }

    if(flags&DeviceFlags_HasProperties){
      m_sink.write(properties.payloadSize(), properties.payloadPtr());
    }

    if(flags&DeviceFlags_HasState){
      m_sink.write(state.payloadSize(), state.payloadPtr());
    }

    m_sink.maybe_flush();

    return index;
  }

    //! The edge instances within the graph instance will follow
  virtual void onBeginEdgeInstances(uint64_t /*graphToken*/)
  {
    m_bitsPerDeviceId=(unsigned)std::ceil(std::log2(m_nextDeviceId));
    
    m_bytesPerDevOnlyEndpoint=(m_bitsPerDeviceId+7)/8;
    m_bytesPerPinOnlyEndpoint=(m_bitsPerPinIndex+7)/8;
    m_bytesPerFullEndpoint=(m_bitsPerPinIndex+m_bitsPerDeviceId+7)/8;

    m_sink.write(BEGIN_EDGES_SENTINEL.size(), &BEGIN_EDGES_SENTINEL[0]);
  }

  //! There will be no more edge instances in the graph.
  virtual void onEndEdgeInstances(uint64_t /*graphToken*/)
  {
    uint8_t flags=DeviceFlags_HasSentinel;

    m_sink.write_scalar(flags);
    m_sink.write(END_EDGES_SENTINEL.size(), &END_EDGES_SENTINEL[0]);
  }

  enum EdgeFlags
  {
    EdgeFlags_HasIndex=0x1,
    EdgeFlags_HasProperties=0x2,
    EdgeFlags_HasState=0x4,

    EdgeFlags_RepeatDstDev=0x8,
    EdgeFlags_RepeatDstPin=0x10,
    EdgeFlags_RepeatSrcDev=0x20,
    EdgeFlags_RepeatSrcPin=0x40,

    EdgeFlags_RepeatDstMask = EdgeFlags_RepeatDstDev | EdgeFlags_RepeatDstPin,
    EdgeFlags_RepeatSrcMask = EdgeFlags_RepeatSrcDev | EdgeFlags_RepeatSrcPin,

    EdgeFlags_HasSentinel = 0x80
  };

  virtual void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   int sendIndex, // -1 if it is not indexed pin, or if index is not explicitly specified
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
    rapidjson::Document &&metadata=rapidjson::Document()
  ){
    uint64_t index=++m_nextEdgeIndex;

    assert(dstDevInst < 0x80000000);
    assert(srcDevInst < 0x80000000);

    uint8_t flags=0;
    
    if(dstDevInst == m_prevDstDevInst){
      flags |= EdgeFlags_RepeatDstDev;
    }else{
      m_prevDstDevInst=dstDevInst;
    }
    auto dstPinIndex=dstPin->getIndex();
    if(dstPinIndex == m_prevDstDevPin){
      flags |= EdgeFlags_RepeatDstPin;
    }else{
      m_prevDstDevPin = dstPinIndex;
    }

    if(srcDevInst == m_prevSrcDevInst){
      flags |= EdgeFlags_RepeatSrcDev;
    }else{
      m_prevSrcDevInst=srcDevInst;
    }
    auto srcPinIndex=srcPin->getIndex();
    if(srcPinIndex == m_prevSrcDevPin){
      flags |= EdgeFlags_RepeatSrcPin;
    }else{
      m_prevSrcDevPin = srcPinIndex;
    }

    if(sendIndex!=-1){
      flags |= EdgeFlags_HasIndex; 
    }

    if(!(properties.empty() || dstPin->getPropertiesSpec()->is_default(properties))){
      flags |= EdgeFlags_HasProperties;
    }
    if(!(state.empty() || dstPin->getStateSpec()->is_default(state))){
      flags |= EdgeFlags_HasState;
    }

    if(1==(index&0xFFFF)){
      flags |= EdgeFlags_HasSentinel;
    }

    auto a=m_sink.tell();
    //fprintf(stderr, "\n    written=%lu\n", m_sink.tell()-a);

    m_sink.write_scalar<uint8_t>(flags);
    //fprintf(stderr, "  flags=1\n");
    //fprintf(stderr, "    written=%lu\n", m_sink.tell()-a);

    if(flags&EdgeFlags_HasSentinel){
      m_sink.write(CONTINUE_EDGES_SENTINEL.size(), &CONTINUE_EDGES_SENTINEL[0]);
      //fprintf(stderr, "  sentinel=16\n");
    }
    
    switch(flags&EdgeFlags_RepeatDstMask){
    case 0:
      {
        uint64_t full = (dstDevInst << m_bitsPerDeviceId) | dstPinIndex;
        m_sink.write( m_bytesPerFullEndpoint , &full ); 
        //fprintf(stderr, "  dstF=%u\n", m_bytesPerFullEndpoint);
      } break;
    case EdgeFlags_RepeatDstDev:
      m_sink.write( m_bytesPerPinOnlyEndpoint , &dstPinIndex );
      //fprintf(stderr, "  dstP=%u\n", m_bytesPerPinOnlyEndpoint);
        break;
    case EdgeFlags_RepeatDstPin:
      m_sink.write( m_bytesPerDevOnlyEndpoint, &dstDevInst ); 
      //fprintf(stderr, "  dstD=%u\n", m_bytesPerDevOnlyEndpoint);
       break;
    default:
      break;
    }
    //fprintf(stderr, "    written=%lu\n", m_sink.tell()-a);

    switch(flags&EdgeFlags_RepeatSrcMask){
    case 0:
      {
        uint64_t full = (srcDevInst << m_bitsPerDeviceId) | srcPinIndex;
        m_sink.write( m_bytesPerFullEndpoint , &full ); 
        //fprintf(stderr, "  srcF=%u\n", m_bytesPerFullEndpoint);
      } break;
    case EdgeFlags_RepeatSrcDev:
      m_sink.write( m_bytesPerPinOnlyEndpoint , &srcPinIndex );
      //fprintf(stderr, "  srcP=%u\n", m_bytesPerPinOnlyEndpoint);
        break;
    case EdgeFlags_RepeatSrcPin:
      m_sink.write( m_bytesPerDevOnlyEndpoint, &srcDevInst );
      //fprintf(stderr, "  srcD=%u\n", m_bytesPerDevOnlyEndpoint);
        break;
    default:
      break;
    }
    //fprintf(stderr, "    written=%lu\n", m_sink.tell()-a);

    if(flags&EdgeFlags_HasIndex){
      auto h=m_sink.write_i64(sendIndex);
      //fprintf(stderr, "  sendIndex=%llu\n", m_sink.tell()-h);
    }

    if(flags&EdgeFlags_HasProperties){
      m_sink.write(properties.payloadSize(), properties.payloadPtr());
      //fprintf(stderr, "  properties=%lu\n", properties.payloadSize());
    }
    //fprintf(stderr, "    written=%lu\n", m_sink.tell()-a);

    if(flags&EdgeFlags_HasState){
      m_sink.write(state.payloadSize(), state.payloadPtr());
      //fprintf(stderr, "  state=%lu\n", state.payloadSize());
    }

    auto b=m_sink.tell();

    //fprintf(stderr, "  flags=%x, bytes=%llu\n", flags, b-a);

    m_sink.maybe_flush();
  }
};

#endif
