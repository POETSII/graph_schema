#include "simulator_context.hpp"
#include "graph_persist_dom_reader.hpp"

int main(int argc, char *argv[])
{

    RegistryImpl registry;

    filepath srcPath(argv[1]);

    xmlpp::DomParser parser;
    parser.parse_file(srcPath.c_str());

    auto engine=std::make_shared<SimulationEngineFast>();

    loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), engine.get());
}
