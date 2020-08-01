#ifndef graph_persist_dom_reader_hpp
#define graph_persist_dom_reader_hpp

#include "graph_persist.hpp"

/* Implementation comes from graph_persist_dom_reader_impl.hpp, or linking graph_persist_dom_reader_impl.o */

// Helper function to load graph type from node
GraphTypePtr loadGraphType(const filepath &srcPath, xmlpp::Element *root, GraphLoadEvents *events=nullptr);

// Helper function to load graph type from path
GraphTypePtr loadGraphType(const filepath &srcPath, GraphLoadEvents *events=nullptr);

void loadGraph(Registry *registry, const filepath &srcPath, xmlpp::Element *parent, GraphLoadEvents *events);

#ifndef POETS_GRAPH_SCHEMA_SEPERATE_COMPILATION
#include "graph_persist_dom_reader_impl.hpp"
#endif

#endif
