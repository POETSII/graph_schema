#ifndef graph_persist_hpp
#define graph_persist_hpp

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"


#include "graph_core.hpp"

#include <mutex>

#if 0
// Boost not installed on keystone
//#include <boost/filesystem.hpp>
#else
 #include <unistd.h>
// HACK.
struct filepath
{
  filepath()
    : path("")
  {}

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

inline filepath current_path()
{
  std::shared_ptr<char> p(getcwd(0,0),free);
  return filepath(p.get());
}

inline filepath absolute(const filepath &relPath, const filepath &basePath=current_path())
{
  if(relPath.path.size()>0 && relPath.path[0]=='/'){
    return relPath;
  }
  return basePath.native()+"/"+relPath.native();
}

inline bool exists(const filepath &p)
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
private:
  uint64_t m_defaultIdCounter=0;
public:
  virtual ~GraphLoadEvents()
  {}

  virtual void onGraphType(const GraphTypePtr &graph)
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
  )
  { return m_defaultIdCounter++; }

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
    to the consumer.
    */
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
   int sendIndex, // -1 if it is not indexed pin, or if index is not explicitly specified
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
    rapidjson::Document &&metadata=rapidjson::Document()
  ) =0;
};

void loadGraph(Registry *registry, const filepath &srcPath, xmlpp::Element *elt, GraphLoadEvents *events);

#endif
