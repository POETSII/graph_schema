#include "graph.hpp"

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

  
  virtual uint64_t onGraphProperties(const GraphTypePtr &graphType,
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
   const TypedDataPtr &properties,
   const TypedDataPtr &state
   ) override {
    std::vector<output> outputs;
    for(auto output : dt->getOutputs()){
      outputs.emplace_back(output->getEdgeType(), {});
    }
    
    uint64_t index=instances.size();
    instances.emplace_back(index, id, deviceType, properties,state, outputs);
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
   const TypedDataPtr properties,
   TypedDataPtr state
  ) override
  {
    auto dst=instances.at(dstDevInst);
    auto src=instances.at(srcDevInst);

    src.outputs.at(OutputPortPtr->getIndex()).edges.emplace_back(dstDevInst, srcPort->getIndex(), properties, state));
  }
};


int main()
{
  xmlpp::DomParser parser;

  parser.parse_stream(std::cin);
  if(!parser){
    exit(1);
  }
}
