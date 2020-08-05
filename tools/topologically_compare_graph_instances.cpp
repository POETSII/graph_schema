#define RAPIDJSON_HAS_STDSTRING 1
#include "graph.hpp"

#include "graph_persist_hash_topology_impl.hpp"


int main(int argc, char *argv[])
{
  try{
    filepath refSrcFileName("-");
    filepath otherSrcFileName("");

    if(argc>1){
      refSrcFileName=std::string(argv[1]);
    }

    if(argc>2){
      otherSrcFileName=std::string(argv[2]);
    }

    check_graph_instances_topologically_similar(refSrcFileName.c_str(), otherSrcFileName.c_str(), true);

    fprintf(stderr, "Graph typess match topologically.\n");
    return 0;
  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
