#ifndef graph_persist_sax_writer_impl_hpp
#define graph_persist_sax_writer_impl_hpp


#include "graph_persist_sax_writer_v3.hpp"
#include "graph_persist_sax_writer_v4.hpp"
#include "graph_persist_sax_writer_base85.hpp"

std::shared_ptr<GraphLoadEvents> createSAXWriterOnFile(const std::string &path, const sax_writer_options &options)
{
  if(options.format=="v3"){
    return createSAXWriterV3OnFile(path, options);
  }else if(options.format=="v4" || options.format.empty()){
    return createSAXWriterV4OnFile(path, options);
  }else if(options.format=="base85"){
    return createSAXWriterBase85OnFile(path, options);
  }else{
    throw std::runtime_error("Didnt understand format for SAXWriter : "+options.format);
  }
}

#endif
