#ifndef graph_persist_hash_topology_hpp
#define graph_persist_hash_topology_hpp

#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "graph_persist.hpp"

#include "xml_pull_parser.hpp"

class GraphPersistHashTopology
    : public GraphLoadEvents
{
public:
  using hash_acc_t = unsigned __int128;

private:
  static const hash_acc_t P=(hash_acc_t(0x0000000001000000ull)<<64)+0x000000000000013Bull;
  static const hash_acc_t O=(hash_acc_t(0x6c62272e07bb0142ull)<<64)+0x62b821756295c58dull;

  hash_acc_t hash_binary(hash_acc_t acc, size_t n, const void *p)
  {
    const char *begin=(const char *)p;
    const char *end=begin+n;

    while(begin<end){
      ptrdiff_t nn=std::min(ptrdiff_t(8), end-begin);
      uint64_t v=0;
      memcpy(&v, begin, nn);
      auto hi=acc>>64;
      acc=(acc ^ v) * P + hi;
      begin+=nn;
    }

    return acc;
  }

  hash_acc_t hash_uint64(hash_acc_t acc, uint64_t value)
  {
    auto hi=acc>>64;
    return (acc ^ value) * P + (acc>>64);
  }

  hash_acc_t hash_string(hash_acc_t acc, const std::string &id)
  {
    return hash_binary(acc, id.size(), id.data());
  }

  hash_acc_t hash_typed_data(hash_acc_t acc, const TypedDataSpecPtr &spec, TypedDataPtr data)
  {
    if(!data){
      data=spec->create();
    }
    if(data){
      return hash_binary(acc, data.payloadSize(), data.payloadPtr());
    }else{
      return acc;
    }
  }

  GraphTypePtr m_graph_type;
  std::string m_graph_instance_id;
  hash_acc_t m_header_hash = O;
  hash_acc_t m_devices_full_hash = O;
  hash_acc_t m_devices_ids_hash = O;
  hash_acc_t m_edges_full_hash = O;
  hash_acc_t m_edges_connection_hash = O;

  unsigned m_device_count=0;
  unsigned m_edge_count=0;

public:
  hash_acc_t get_hash() const
  { return m_header_hash*19937 + 31 * m_devices_full_hash+ m_edges_full_hash; }


  virtual uint64_t onBeginGraphInstance(
    const GraphTypePtr &graph,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ){
    m_graph_type=graph;
    
    m_header_hash=hash_string(m_header_hash, graph->getId());
    m_header_hash=hash_typed_data(m_header_hash, graph->getPropertiesSpec(), properties);

    return 0;
  }

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
    hash_acc_t acc=O;
    acc=hash_string(acc, dt->getId());
    acc=hash_string(acc,id);
    m_devices_ids_hash += acc;

    acc=hash_typed_data(acc, dt->getPropertiesSpec(), properties);
    acc=hash_typed_data(acc, dt->getStateSpec(), state);

    m_devices_full_hash += acc;

    return m_device_count++;
  }

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
    hash_acc_t acc=O;
    acc=hash_uint64(acc, dstDevInst);
    acc=hash_uint64(acc, dstPin->getIndex());
    acc=hash_uint64(acc, srcDevInst);
    acc=hash_uint64(acc, srcPin->getIndex());
    acc=hash_uint64(acc, (uint64_t)(int64_t)sendIndex);
    m_edges_connection_hash += acc;

    acc=hash_typed_data(acc, dstPin->getPropertiesSpec(), properties);
    acc=hash_typed_data(acc, dstPin->getStateSpec(), state);

    m_edges_full_hash += acc;

    m_edge_count++;
  }

  template<class T>
  void report_diff(std::stringstream &acc, const T &a, const T &b, const char *name) const
  {
    if(a==b){
      acc<<"  same : "<<name<<" hash\n";
    }else{
      acc<<"  DIFF : "<<name<<" hash\n";
    }
  }

  template<class T>
  void report_diff_val(std::stringstream &acc, const T &a, const T &b, const char *name) const
  {
    if(a==b){
      acc<<"  same : "<<name<<" = "<<a<<"\n";
    }else{
      acc<<"  DIFF : "<<name<<" = "<<a<<" vs "<<b<<"\n";
    }
  }


  std::string report(const GraphPersistHashTopology &ho){
    std::stringstream acc;
    report_diff_val(acc, m_device_count, ho.m_device_count, "device_count");
    report_diff(acc, m_devices_ids_hash, ho.m_devices_ids_hash, "device_ids_hash");
    report_diff(acc, m_devices_full_hash, ho.m_devices_full_hash, "device_full_hash");
    report_diff_val(acc, m_edge_count, ho.m_edge_count, "edge_count");
    report_diff(acc, m_edges_connection_hash, ho.m_edges_connection_hash, "edges_connection_hash");
    report_diff(acc, m_edges_full_hash, ho.m_edges_full_hash, "edges_full_hash");
    return acc.str();
  }
};

bool check_graph_instances_topologically_similar(
    std::string path1,
    std::string path2,
    bool throw_on_mismatch
){
  GraphPersistHashTopology h1;
  GraphPersistHashTopology h2;

  loadGraphPull(nullptr, path1, &h1);
  loadGraphPull(nullptr, path2, &h2);
  if(h1.get_hash() != h2.get_hash()){
    if(throw_on_mismatch){
      throw std::runtime_error("Graphs are not topologically similar\n"+h1.report(h2));
    }
    return false;
  }
  return true;
}

#endif
