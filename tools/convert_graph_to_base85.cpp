#include "graph.hpp"

#include "xml_pull_parser.hpp"
#include "graph_persist_sax_writer_base85.hpp"

#include <iostream>
#include <fstream>


int main(int argc, char *argv[])
{
  try{
    filepath srcFileName("/dev/stdin");
    filepath dstFileName("/dev/stdout");

    if(argc>1){
      srcFileName=std::string(argv[1]);
    }
    if(argc>2){
      dstFileName=std::string(argv[2]);
    }

    auto writer=createSAXWriterBase85OnFile(dstFileName.c_str());

    loadGraphPull(nullptr, srcFileName, writer.get());

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
