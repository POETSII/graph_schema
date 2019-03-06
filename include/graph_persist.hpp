#ifndef graph_persist_hpp
#define graph_persist_hpp

#include "rapidjson/document.h"


#include "graph.hpp"

#if 0
// Boost not installed on keystone
//#include <boost/filesystem.hpp>
#else
 #include <unistd.h>
// HACK.
struct filepath
{
  filepath(const std::string &p)
    : path(p)
  {}

  std::string path;

  const char *c_str() const
  { return path.c_str(); }

  const std::string &native() const
  { return path; }

  filepath parent_path() const
  {
    auto pos=path.find_last_of('/');
    if(pos==std::string::npos){
      return *this;
    }
    auto parent=path.substr(0, pos);
    return parent;
  }
};

filepath current_path()
{
  std::shared_ptr<char> p(getcwd(0,0),free);
  return filepath(p.get());
}

filepath absolute(const filepath &relPath, const filepath &basePath=current_path())
{
  if(relPath.path.size()>0 && relPath.path[0]=='/'){
    return relPath;
  }
  return basePath.native()+"/"+relPath.native();
}

bool exists(const filepath &p)
{
  return 0==access(p.path.c_str(), F_OK);
}
#endif

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
    const TypedDataPtr &properties,
    rapidjson::Document &&metadata
  ) =0;

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
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
   rapidjson::Document &&metadata=rapidjson::Document()
  ) =0;

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
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   const TypedDataPtr &properties,
   rapidjson::Document &&metadata=rapidjson::Document()
  ) =0;
};

extern "C" void loadGraph(Registry *registry, const std::string &srcPath, xmlpp::Element *elt, GraphLoadEvents *events);

#endif
