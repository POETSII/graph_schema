#ifndef graph_persist_hpp
#define graph_persist_hpp

#include "graph.hpp"

class GraphLoadEvents
{
public:
  virtual ~GraphLoadEvents()
  {}

  virtual void onGraphType(const GraphTypePtr &graph)
  {}

  virtual void onDeviceType(const DeviceTypePtr &device)
  {}

  virtual void onEdgeType(const EdgeTypePtr &edge)
  {}

  //! Tells the consumer that a new graph is starting
  virtual void onBeginGraphInstance(const GraphTypePtr &graph, const std::string &id, const TypedDataPtr &properties) =0;

  //! The graph is now complete
  virtual void onEndGraphInstance()
  {}

  //! The device instances within the graph instance will follow
  virtual void onBeginDeviceInstances()
  {}

  //! There will be no more device instances in the graph.
  virtual void onEndDeviceInstances()
  {}

  // Tells the consumer that a new instance is being added
  /*! The return value is a unique identifier that means something
    to the consumer. */
  virtual uint64_t onDeviceInstance
  (
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const double *nativeLocation //! If null then no location, otherwise it will match graphType->getNativeDimension()
  ) =0;

    //! The edge instances within the graph instance will follow
  virtual void onBeginEdgeInstances()
  {}

  //! There will be no more edge instances in the graph.
  virtual void onEndEdgeInstances()
  {}

  //! Tells the consumer that the a new edge is being added
  /*! It is required that both device instances have already been
    added (otherwise the ids would not been known).
  */
  virtual void onEdgeInstance
  (
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr &properties
  ) =0;
};

extern "C" void loadGraph(Registry *registry, xmlpp::Element *elt, GraphLoadEvents *events);

#endif
