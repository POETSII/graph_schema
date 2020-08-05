#ifndef graph_persist_hash_topology_hpp
#define graph_persist_hash_topology_hpp

#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "graph_persist.hpp"

bool check_graph_types_topologically_similar(
    std::string path1,
    std::string path2,
    bool throw_on_mismatch
);

#endif
