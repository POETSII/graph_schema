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

    std::string srcFileName;

    int partitions=2;
    int steps=1000000;
    double propSteps=1.0;
    bool showClusters=false;
    int imbalanceWeight=1;
    int imbalanceExponent=2;
    int crossingWeight=4;
    std::string method="anneal";

    int ai=1;
    while(ai < argc){
      if(!strcmp(argv[ai], "--partitions")){
        if(ai+1>=argc){
          fprintf(stderr, "Not enough arguments for --partitions.");
          exit(1);
        }
        partitions = atoi(argv[ai+1]);
        ai+=2;
      }else if(!strcmp(argv[ai], "--steps")){
        if(ai+1>=argc){
          fprintf(stderr, "Not enough arguments for --steps.");
          exit(1);
        }
        steps = atoi(argv[ai+1]);
        ai+=2;
      }else if(!strcmp(argv[ai], "--prop-steps")){
        if(ai+1>=argc){
          fprintf(stderr, "Not enough arguments for --prop-steps.");
          exit(1);
        }
        propSteps = strtod(argv[ai+1], nullptr);
        ai+=2;
      }else if(!strcmp(argv[ai], "--show-clusters")){
        showClusters=true;
        ai+=1;
      }else if(!strcmp(argv[ai], "--imbalance-weight")){
        if(ai+1>=argc){
          fprintf(stderr, "Not enough arguments for --imbalance-weight.");
          exit(1);
        }
        imbalanceWeight = atoi(argv[ai+1]);
        ai+=2;
      }else if(!strcmp(argv[ai], "--imbalance-exponent")){
        if(ai+1>=argc){
          fprintf(stderr, "Not enough arguments for --imbalance-exponent.");
          exit(1);
        }
        imbalanceExponent = atoi(argv[ai+1]);
        ai+=2;
      }else if(!strcmp(argv[ai], "--crossing-weight")){
        if(ai+1>=argc){
          fprintf(stderr, "Not enough arguments for --crossing-weight.");
          exit(1);
        }
        crossingWeight = atoi(argv[ai+1]);
        ai+=2;
      }else{
        if(!srcFileName.empty()){
          fprintf(stderr, "Two input filenames.\n");
          exit(1);
        }
        srcFileName=argv[ai];
        ai++;
      }
    }




    std::istream *src=&std::cin;
    std::ifstream srcFile;

    if(!srcFileName.empty()){
      fprintf(stderr,"Reading from '%s'\n", srcFileName.c_str());
      srcFile.open(srcFileName);
      if(!srcFile.is_open())
        throw std::runtime_error(std::string("Couldn't open '")+srcFileName+"'");
      src=&srcFile;
    }



    fprintf(stderr, "Parsing XML\n");
    parser.parse_stream(*src);
    fprintf(stderr, "Parsed XML\n");

    Partitioner graph(partitions);
    graph.setImbalanceWeight(imbalanceWeight);
    graph.setImbalanceExponent(imbalanceExponent);
    graph.setCrossingWeight(crossingWeight);

    std::cerr<<"building partitioner\n";
    loadGraph(&registry, parser.get_document()->get_root_node(), &graph);

    if(method=="greedy"){
      std::cerr<<"Greedy:\n";
      graph.greedy(steps, propSteps);
    }else if(method=="anneal"){
      std::cerr<<"Anneal:\n";
      graph.anneal(steps, propSteps);
    }else{
      throw std::runtime_error("Unknonwn method.");
    }


    graph.dump_dot(showClusters);

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
