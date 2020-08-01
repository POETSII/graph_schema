#ifndef graph_persist_dom_reader_impl_hpp
#define graph_persist_dom_reader_impl_hpp

#include "graph_persist_dom_reader_v3.hpp"
#include "graph_persist_dom_reader_v4.hpp"

//#include <boost/filesystem.hpp>
#include <libxml++/parsers/domparser.h>

// Helper function to load graph type from node
GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *root, GraphLoadEvents *events)
{
  if(root->get_namespace_uri()=="https://poets-project.org/schemas/virtual-graph-schema-v3"){
    return xml_v3::loadGraphType(srcPath, root, events);
  }else if(root->get_namespace_uri()=="https://poets-project.org/schemas/virtual-graph-schema-v4"){
    return xml_v4::loadGraphType(srcPath, root, events);
  }else{
    throw std::runtime_error("Unknown namespace on Graphs element : "+root->get_namespace_uri());
  }
}

// Helper function to load graph type from path
GraphTypePtr loadGraphType(const filepath &srcPath, GraphLoadEvents *events)
{
  auto parser=std::make_shared<xmlpp::DomParser>(srcPath.native());
  if(!*parser){
    throw std::runtime_error("Couldn't parse XML at '"+srcPath.native()+"'");
  }

  xmlpp::Element *root=parser->get_document()->get_root_node();
  if(root->get_name()!="Graphs"){
    throw std::runtime_error("Expected document root to be a Graphs element, but got "+root->get_name());
  }

  return loadGraphType(srcPath, root, events);
}

void loadGraph(Registry *registry, const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events)
{
  if(parent->get_namespace_uri()=="https://poets-project.org/schemas/virtual-graph-schema-v3"){
    xml_v3::loadGraph(registry, srcPath, parent, events);
  }else if(parent->get_namespace_uri()=="https://poets-project.org/schemas/virtual-graph-schema-v4"){
    xml_v4::loadGraph(registry, srcPath, parent, events);
  }else{
    throw std::runtime_error("Unknown namespace URI on document root element.");
  }

}


#endif
