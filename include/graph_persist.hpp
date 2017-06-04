#ifndef graph_persist_hpp
#define graph_persist_hpp

#include "rapidjson/document.h"

#include <boost/filesystem.hpp>


#include "graph.hpp"

/*!
  Metadata is represented as rapidjson structs, as they
  are pretty good for memory efficiency, and can handle
  untyped bags of stuff. The semantics are always move
  semantics, as this makes it easier to pass through data.
  Most of the time you won't know what the metadata means,
  so you grab hold of it when you are given it, add or read
  anything you know about, then pass it on to the destination.
*/
class GraphLoadEvents
{
public:
  virtual ~GraphLoadEvents()
  {}

  virtual void onGraphType(const GraphTypePtr &graph)
  {}

  virtual void onDeviceType(const DeviceTypePtr &device)
  {}

  virtual void onMessageType(const MessageTypePtr &edge)
  {}

  // Should meta-data be parsed and extracted?
  /*! By default metadata is not parsed, as it is faster
      to skip it if you don't want it.
  */
  virtual bool parseMetaData() const
  { return false; }

  //! Tells the consumer that a new graph is starting
  virtual uint64_t onBeginGraphInstance(
    const GraphTypePtr &graph,
    const std::string &id,
    const TypedDataPtr &properties
  ) {
    throw std::runtime_error("onBeginGraphInstance not implemented.");
  }

  //! Tells the consumer that a new graph is starting
  virtual uint64_t onBeginGraphInstance(
    const GraphTypePtr &graph,
    const std::string &id,
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ) {
    return onBeginGraphInstance(graph, id, properties);
  }

  //! The graph is now complete
  virtual void onEndGraphInstance(uint64_t /*graphToken*/)
  {}

  //! The device instances within the graph instance will follow
  virtual void onBeginDeviceInstances(uint64_t /*graphToken*/)
  {}

  //! There will be no more device instances in the graph.
  virtual void onEndDeviceInstances(uint64_t /*graphToken*/)
  {}

  // Tells the consumer that a new instance is being added
  /*! The return value is a unique identifier that means something
    to the consumer. */
  virtual uint64_t onDeviceInstance
  (
   uint64_t graphInst,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties
  ) {
    throw std::runtime_error("onDeviceInstance not implemented.");
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
   rapidjson::Document &&metadata
  ) {
    return onDeviceInstance(graphInst, dt, id, properties);
  }

    //! The edge instances within the graph instance will follow
  virtual void onBeginEdgeInstances(uint64_t /*graphToken*/)
  {}

  //! There will be no more edge instances in the graph.
  virtual void onEndEdgeInstances(uint64_t /*graphToken*/)
  {}

  //! Tells the consumer that the a new edge is being added
  /*! It is required that both device instances have already been
    added (otherwise the ids would not been known).
  */
  virtual void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr &properties
  ) {
    throw std::runtime_error("onEdgeInstance not implemented.");
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
   const TypedDataPtr &properties,
   rapidjson::Document &&metadata
  ) {
    onEdgeInstance(graphInst, dstDevInst, dstDevType, dstPort, srcDevInst, srcDevType, srcPort, properties);
  }
};

extern "C" void loadGraph(Registry *registry, const boost::filesystem::path &srcPath, xmlpp::Element *elt, GraphLoadEvents *events);

#endif
