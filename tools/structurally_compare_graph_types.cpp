#define RAPIDJSON_HAS_STDSTRING 1
#include "graph.hpp"

#include "xml_pull_parser.hpp"


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

    fprintf(stderr, "Loading reference graph type from %s\n", refSrcFileName.native().c_str());
    GraphTypePtr refGraphType=loadGraphType(refSrcFileName);
    fprintf(stderr, "  found graph type %s\n", refGraphType->getId().c_str());

    GraphTypePtr otherGraphType;
    if(otherSrcFileName.native()==""){
      fprintf(stderr, "Loading graph type %s from provider registry.\n", refGraphType->getId().c_str());
      fprintf(stderr, "  initialising registry.\n");
      RegistryImpl registry;

      fprintf(stderr, "  loading graph type from registry.\n");
      otherGraphType=registry.lookupGraphType(refGraphType->getId());
    }else{
      fprintf(stderr, "Loading other graph type from %s.\n", otherSrcFileName.native().c_str());
      otherGraphType=loadGraphType(otherSrcFileName);
    }

    fprintf(stderr, "Comparing graph types.\n");
    check_graph_types_structurally_similar(refGraphType, otherGraphType, true);
    fprintf(stderr, "Graph typess match structurally.\n");
    return 0;
  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
