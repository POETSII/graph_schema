#include "graph.hpp"

#include "xml_pull_parser.hpp"

#include <iostream>
#include <fstream>

class GraphInfo : public GraphLoadEvents
{
public:
  GraphTypePtr graphType;

  std::string id;
  TypedDataPtr properties;
  rapidjson::Document metadata;

  bool quiet=false;

  struct edge
  {
    uint64_t dstDevice;
    unsigned dstPin;
    TypedDataPtr properties;
    rapidjson::Document metadata;
  };

  struct output
  {
    MessageTypePtr messageType;
    std::vector<edge> edges;
  };

  struct instance{
    uint64_t index;
    std::string id;
    DeviceTypePtr deviceType;
    TypedDataPtr properties;
    rapidjson::Document metadata;

    std::vector<output> outputs;
  };

  std::vector<instance> instances;

  virtual bool parseMetaData() const override
  { return true; }

  virtual void onGraphType(const GraphTypePtr &graph) override
  {
    fprintf(stderr, "  onGraphType(%s)\n", graph->getId().c_str());
  }

  virtual void onDeviceType(const DeviceTypePtr &device) override
  {
    fprintf(stderr, "  onDeviceType(%s)\n", device->getId().c_str());
  }

  virtual void onMessageType(const MessageTypePtr &message) override
  {
    fprintf(stderr, "  onMessageType(%s)\n", message->getId().c_str());
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

  // Tells the consumer that a new instance is being added
  /*! The return value is a unique identifier that means something
    to the consumer. */
  virtual uint64_t onDeviceInstance
  (
   uint64_t graphId,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
   rapidjson::Document &&metadata
   ) override {
     if(!quiet){
      fprintf(stderr, "  onDeviceInstance(%s)\n", id.c_str());
     }

    std::vector<output> outputs;
    for(auto &o : dt->getOutputs()){
      output to{ o->getMessageType(), {} };
      outputs.push_back(std::move(to));
    }

    uint64_t index=instances.size();
    instance inst;
    inst.index=index;
    inst.id=id;
    inst.deviceType=dt;
    inst.properties=properties;
    inst.metadata=std::move(metadata);
    inst.outputs=std::move(outputs);
    instances.push_back(std::move(inst));
    return index;
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
   int sendIndex, // -1 if it is not indexed pin, or if index is not explicitly specified
   const TypedDataPtr &properties,
  rapidjson::Document &&metadata
  ) override
  {
    auto &dst=instances.at(dstDevInst);
    auto &src=instances.at(srcDevInst);

  if(!quiet){
    fprintf(stderr, "  onEdgeInstance(%s.%s <- %s.%s)\n",
	    dst.id.c_str(), dstPin->getName().c_str(),
	    src.id.c_str(), srcPin->getName().c_str());
  }

    edge e{ dstDevInst, srcPin->getIndex(), properties, rapidjson::Document() };
    src.outputs.at(srcPin->getIndex()).edges.emplace_back(std::move(e));
  }
};


int main(int argc, char *argv[])
{
  try{
    fprintf(stderr, "Initialising registry.\n");
    RegistryImpl registry;
    fprintf(stderr, "Parsing.\n");

    xmlpp::DomParser parser;

    std::istream *src=&std::cin;
    std::ifstream srcFile;
    filepath srcPath(current_path());
    filepath srcFilePath=srcPath;

    filepath p(argv[1]);
    p=absolute(p);
    fprintf(stderr,"Reading from '%s' ( = '%s' absolute)\n", argv[1], p.c_str());
    src=&srcFile;
    srcPath=p.parent_path();

    GraphInfo graph;
    if(argc>2){
      graph.quiet=atoi(argv[2]);
    }

    //loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), &graph);
    loadGraphPull(&registry, p, &graph);

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
