#include "graph.hpp"
#include "partitioner.hpp"

#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <fstream>



int main(int argc, char *argv[])
{
  try{
    RegistryImpl registry;

    xmlpp::DomParser parser;

    std::istream *src=&std::cin;
    std::ifstream srcFile;

    if(argc>1){
      fprintf(stderr,"Reading from '%s'\n", argv[1]);
      srcFile.open(argv[1]);
      if(!srcFile.is_open())
	throw std::runtime_error(std::string("Couldn't open '")+argv[1]+"'");
      src=&srcFile;

    }

    fprintf(stderr, "Parsing XML\n");
    parser.parse_stream(*src);
    fprintf(stderr, "Parsed XML\n");

    Partitioner graph(8);

    std::cerr<<"building partitioner\n";
    loadGraph(&registry, parser.get_document()->get_root_node(), &graph);

    //std::cerr<<"Greedy:\n";
    //graph.greedy(1000000);

    std::cerr<<"Anneal:\n";
    graph.anneal(100000000);
    //graph.annealk(10000000,2);

    graph.dump_dot();

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
