#include "graph.hpp"

#include "xml_pull_parser.hpp"
#include "graph_persist_binary_writer.hpp"

#include <iostream>
#include <fstream>


int main(int argc, char *argv[])
{
  try{
    filepath srcPath(current_path());
    filepath srcFileName("-");

    BinarySink binarySink;
    binarySink.m_file=stdout;

    BinaryPayloadWriter binaryPayloadWriter{binarySink};

    loadGraphPull(nullptr, filepath(std::string{"/dev/stdin"}), &binaryPayloadWriter);

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
