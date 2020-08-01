#ifndef xml_pull_parser_hpp
#define xml_pull_parser_hpp

#include "graph_persist.hpp"

void loadGraphPull(Registry *registry, const filepath &srcPath, GraphLoadEvents *events);

void loadGraphTypePull(Registry *registry, const filepath &srcPath, GraphLoadEvents *events);

#ifndef POETS_GRAPH_SCHEMA_SEPERATE_COMPILATION
#include "xml_pull_parser_impl.hpp"
#endif

#endif
