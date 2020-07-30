#ifndef graph_persist_sax_writer_hpp
#define graph_persist_sax_writer_hpp

#include "graph_persist_sax_writer_v3.hpp"

struct sax_writer_options
{
  bool compress=false;
  bool sanity=true;
};

std::shared_ptr<GraphLoadEvents> createSAXWriterOnFile(const std::string &path, const sax_writer_options &options=sax_writer_options{})
{
  return createSAXWriterV3OnFile(path, options);
}



#endif
