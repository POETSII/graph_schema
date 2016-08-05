#include "graph_impl.hpp"

#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <fstream>

class GraphInfo : public GraphLoadEvents
{
public:
  GraphTypePtr graphType;
  
  std::string id;
  TypedDataPtr properties;

  struct edge
  {
    uint64_t dstDevice;
    unsigned dstPort;
    TypedDataPtr properties;
    TypedDataPtr state;
  };
  
  struct output
  {
    EdgeTypePtr edgeType;
    std::vector<edge> edges;
  };
  
  struct instance{
    uint64_t index;
    std::string id;
    DeviceTypePtr deviceType;
    TypedDataPtr properties;
    TypedDataPtr state;

    std::vector<output> outputs;
  };
  
  std::vector<instance> instances;

  virtual void onGraphType(const GraphTypePtr &graph) override 
  {
    fprintf(stderr, "  onGraphType(%s)\n", graph->getId().c_str());
  }
  
  virtual void onDeviceType(const DeviceTypePtr &device) override
  {
    fprintf(stderr, "  onDeviceType(%s)\n", device->getId().c_str());
  }

  virtual void onEdgeType(const EdgeTypePtr &edge) override
  {
    fprintf(stderr, "  onEdgeType(%s)\n", edge->getId().c_str());
  }

  
  virtual uint64_t onGraphInstance(const GraphTypePtr &graphType,
				 const std::string &id,
				 const TypedDataPtr &properties) override
  {
    this->graphType=graphType;
    this->id=id;
    this->properties=properties;
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
   const TypedDataPtr &properties
   ) override {
    fprintf(stderr, "  onDeviceInstance(%s)\n", id.c_str());

    std::vector<output> outputs;
    for(auto o : dt->getOutputs()){
      output to{ o->getEdgeType(), {} };
      outputs.emplace_back(std::move(to));
    }

    TypedDataPtr state;
    
    uint64_t index=instances.size();
    instance inst{ index, id, dt, properties, state, outputs   };
    instances.emplace_back(std::move(inst));
    return index;
  }

  //! Tells the consumer that the a new edge is being added
  /*! It is required that both device instances have already been
    added (otherwise the ids would not been known).
  */
  virtual void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr properties
  ) override
  { 
    auto dst=instances.at(dstDevInst);
    auto src=instances.at(srcDevInst);

    fprintf(stderr, "  onEdgeInstance(%s.%s <- %s.%s)\n",
	    dst.id.c_str(), dstPort->getName().c_str(),
	    src.id.c_str(), srcPort->getName().c_str());

    TypedDataPtr state;
    
    edge e{ dstDevInst, srcPort->getIndex(), properties, state };
    src.outputs.at(srcPort->getIndex()).edges.emplace_back(std::move(e));
  }
};


int main(int argc, char *argv[])
{
  try{
    RegistryImpl registry;
    
    xmlpp::DomParser parser;
    
    std::istream *src=&std::cin;
    std::ifstream srcFile;
    
    if(argc>1){
      fprintf(stderr,"Reading from '%s'\n", argv[1]);
      srcFile.open(argv[1]);
      if(!srcFile.is_open())
	throw std::runtime_error(std::string("Couldn't open '")+argv[1]+"'");
      src=&srcFile;
      
    }

    fprintf(stderr, "Parsing XML\n");
    parser.parse_stream(*src);
    fprintf(stderr, "Parsed XML\n");

    GraphInfo graph;
    
    loadGraph(&registry, parser.get_document()->get_root_node(), &graph);

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
