#define RAPIDJSON_HAS_STDSTRING 1
#include "graph.hpp"

#include "xml_pull_parser.hpp"

#include <iostream>
#include <fstream>
#include <unordered_map>

#include <rapidjson/istreamwrapper.h>
#include <fstream>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/skewness.hpp>
#include <boost/accumulators/statistics/kurtosis.hpp>

class GraphInfo : public GraphLoadEvents
{
public:
  GraphTypePtr graphType;

  std::string id;
  TypedDataPtr properties;
  rapidjson::Document metadata;

  uint64_t total_device_count=0;
  uint64_t total_edge_count=0;
  std::unordered_map<std::string,unsigned> device_type_to_instance_count;
  std::unordered_map<std::string,unsigned> message_type_to_edge_count;
  std::unordered_map<std::string,unsigned> endpoints_type_to_edge_count;
  std::vector<std::vector<unsigned> > incoming_edge_count;
  std::vector<std::vector<unsigned> > outgoing_edge_count;


  virtual bool parseMetaData() const override
  { return true; }

  virtual void onGraphType(const GraphTypePtr &graph) override
  {
  }

  virtual void onDeviceType(const DeviceTypePtr &device) override
  {
    device_type_to_instance_count[device->getId()]=0;
  }

  virtual void onMessageType(const MessageTypePtr &message) override
  {
    message_type_to_edge_count[message->getId()]=0;
  }


  virtual uint64_t onBeginGraphInstance(const GraphTypePtr &graphType,
				 const std::string &id,
				 const TypedDataPtr &properties,
                 rapidjson::Document &&metadata
  ) override
  {
    this->graphType=graphType;
    this->id=id;
    this->properties=properties;
    this->metadata=std::move(metadata);
    return 0;
  }

  virtual uint64_t onDeviceInstance
  (
   uint64_t graphId,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
   rapidjson::Document &&metadata
   ) override {
     incoming_edge_count.push_back(std::vector<unsigned>( dt->getInputCount(), 0 ));
     outgoing_edge_count.push_back(std::vector<unsigned>( dt->getOutputCount(), 0 ));
     device_type_to_instance_count[dt->getId()]++;
      return total_device_count++;
  }

  //! Tells the consumer that the a new edge is being added
  /*! It is required that both device instances have already been
    added (otherwise the ids would not been known).
  */
  virtual void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   int sendIndex,
   const TypedDataPtr &properties,
  const TypedDataPtr &state,
  rapidjson::Document &&metadata
  ) override
  {
    total_edge_count++;
    incoming_edge_count.at(dstDevInst).at(dstPin->getIndex())++;
    outgoing_edge_count.at(srcDevInst).at(srcPin->getIndex())++;
    message_type_to_edge_count[dstPin->getMessageType()->getId()]++;

    // This could be made much cheaper
    std::stringstream tmp;
    tmp<<"<"<<dstDevType->getId()<<">:"<<dstPin->getName()<<"-<"<<srcDevType->getId()<<">:"<<srcPin->getName();
    endpoints_type_to_edge_count[tmp.str()]++;
  }
};


int main(int argc, char *argv[])
{
  try{
    fprintf(stderr, "Initialising registry.\n");
    RegistryImpl registry;
    fprintf(stderr, "Parsing.\n");

    filepath srcPath(current_path());
    filepath srcFileName("-");

    filepath metaPath;

    if(argc>1){
      srcFileName=std::string(argv[1]);
      if(srcFileName.native()!="-"){
        srcFileName=absolute(srcFileName);
        fprintf(stderr,"Reading from '%s' ( = '%s' absolute)\n", argv[1], srcFileName.c_str());
        srcPath=srcFileName.parent_path();
      }
    }

    if(argc>2){
      metaPath=filepath(argv[2]);
    }

    GraphInfo graph;

    fprintf(stderr, "Streaming XML\n");
    if(srcFileName.native()=="-"){
      srcFileName=filepath("/dev/stdin");
    }
    loadGraphPull(&registry, srcFileName, &graph);

    fprintf(stderr, "Parsed XML\n");

    rapidjson::Document info;

    if(metaPath.native()!=""){
      std::ifstream ifs(metaPath.native());
      rapidjson::IStreamWrapper isw(ifs);
      info.ParseStream(isw);
      if(!info.IsObject()){
        std::cerr<<"Existing meta-data must be a JSON object.";
        exit(1);
      }
    }else{
      info.SetObject();
    }

    info.AddMember("graph_instance_id", graph.id, info.GetAllocator());
    info.AddMember("graph_type_id", graph.graphType->getId(), info.GetAllocator());
    info.AddMember("total_devices", uint64_t(graph.total_device_count), info.GetAllocator());
    info.AddMember("total_edges", uint64_t(graph.total_edge_count), info.GetAllocator());

    rapidjson::Value device_counts;
    device_counts.SetObject();
    for(auto &kv : graph.device_type_to_instance_count){
      device_counts.AddMember(rapidjson::Value(kv.first,info.GetAllocator()), rapidjson::Value(kv.second), info.GetAllocator());
    } 
    info.AddMember("device_instance_counts_by_device_type", device_counts, info.GetAllocator());

    rapidjson::Value message_counts;
    message_counts.SetObject();
    for(auto &kv : graph.message_type_to_edge_count){
      message_counts.AddMember(rapidjson::Value(kv.first,info.GetAllocator()), rapidjson::Value(kv.second), info.GetAllocator());
    } 
    info.AddMember("edge_instance_counts_by_message_type", message_counts, info.GetAllocator());

    rapidjson::Value edge_counts;
    edge_counts.SetObject();
    for(auto &kv : graph.endpoints_type_to_edge_count){
      edge_counts.AddMember(rapidjson::Value(kv.first,info.GetAllocator()), rapidjson::Value(kv.second), info.GetAllocator());
    } 
    info.AddMember("edge_instance_counts_by_endpoint_pair", edge_counts, info.GetAllocator());

    using namespace boost::accumulators;

    accumulator_set<double, stats<tag::mean, tag::median, tag::variance, tag::min, tag::max, tag::skewness, tag::kurtosis > > in_acc;
    for(auto &d : graph.incoming_edge_count){
      for(auto &p : d){
        in_acc(p);
      }
    }
    rapidjson::Value in_stats;
    in_stats.SetObject();
    in_stats.AddMember("min", min(in_acc), info.GetAllocator());
    in_stats.AddMember("max", max(in_acc), info.GetAllocator());
    in_stats.AddMember("mean", mean(in_acc), info.GetAllocator());
    in_stats.AddMember("median", median(in_acc), info.GetAllocator());
    in_stats.AddMember("stddev", std::sqrt(variance(in_acc)), info.GetAllocator());
    // Boost procuces nan's sometimes...
    //in_stats.AddMember("skewness", skewness(in_acc), info.GetAllocator());
    //in_stats.AddMember("kurtosis", kurtosis(in_acc), info.GetAllocator());
    info.AddMember("incoming_degree", in_stats, info.GetAllocator());

    accumulator_set<double, stats<tag::mean, tag::median, tag::variance, tag::min, tag::max, tag::skewness, tag::kurtosis > > out_acc;
    for(auto &d : graph.outgoing_edge_count){
      for(auto &p : d){
        out_acc(p);
      }
    }
    rapidjson::Value out_stats;
    out_stats.SetObject();
    out_stats.AddMember("min", min(out_acc), info.GetAllocator());
    out_stats.AddMember("max", max(out_acc), info.GetAllocator());
    out_stats.AddMember("mean", mean(out_acc), info.GetAllocator());
    out_stats.AddMember("median", median(out_acc), info.GetAllocator());
    out_stats.AddMember("stddev", std::sqrt(variance(out_acc)), info.GetAllocator());
    //out_stats.AddMember("skewness", skewness(out_acc), info.GetAllocator());
    //out_stats.AddMember("kurtosis", kurtosis(out_acc), info.GetAllocator());
    info.AddMember("outgoing_degree", out_stats, info.GetAllocator());


    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    info.Accept(writer);

    std::cout<<buffer.GetString()<<"\n";
    std::cout.flush();
    

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
