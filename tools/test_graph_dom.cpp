#include "graph_dom.hpp"

#include "xml_pull_parser.hpp"
#include "graph_persist_sax_writer.hpp"
#include "graph_provider_helpers.hpp"

#include <iostream>
#include <fstream>

int main(int argc, char *argv[])
{
  try{
    RegistryImpl registry;

    filepath src("/dev/stdin");
    filepath dst("/dev/stdout");

    if(argc>1){
      src=filepath(argv[1]);
    }
    if(argc>2){
      dst=filepath(argv[2]);
    }

    GraphDOMBuilder builder;
    loadGraphPull(nullptr, src, &builder);

    auto sink=createSAXWriterOnFile(dst.c_str());

    save(builder.g, sink.get() );

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
