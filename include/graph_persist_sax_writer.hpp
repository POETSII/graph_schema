#ifndef graph_persist_sax_writer_hpp
#define graph_persist_sax_writer_hpp

#include <string>

struct sax_writer_options
{
  std::string format;
  bool compress=false;
  bool sanity=true;
};

/* To get an implementation of this, either #include "graph_persist_sax_writer_impl.hpp",
   or link against graph_persist_sax_writer.o
*/
std::shared_ptr<GraphLoadEvents> createSAXWriterOnFile(const std::string &path, const sax_writer_options &options=sax_writer_options{});

#ifndef POETS_GRAPH_SCHEMA_SEPERATE_COMPILATION
#include "graph_persist_sax_writer_impl.hpp"
#endif

#endif