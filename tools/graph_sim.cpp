#include "simulator_context.hpp"
#include "graph_persist_dom_reader.hpp"

#include <libxml++/parsers/domparser.h>

void usage()
{
    fprintf(stderr, "graph_sim [options] sourceFile?\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  --log-level n\n");
    fprintf(stderr, "  --max-events n : Maximum number of send or receive events.\n");
    fprintf(stderr, "  --log-events destFile\n");
    fprintf(stderr, "  --prob-send probability : The closer to 1.0, the more likely to send. Closer to 0.0 will prefer receive\n");
    fprintf(stderr, "  --accurate-assertions : Capture device state before send/recv in case of assertions.\n");
    fprintf(stderr, "  --message-init n: 0 (default) - Zero initialise all messages, 1 - All messages are randomly inisitalised, 2 - Randomly zero or random inisitalise\n");
    fprintf(stderr, "  --strategy strategy-name : FIFO|Random|LIFO\n");
    exit(1);
}

std::shared_ptr<LogWriter> g_pLog; // for flushing purposes on exit

void close_resources()
{
    if(g_pLog){
        g_pLog->close();
        g_pLog=0;
    }
}

void atexit_close_resources()
{
    close_resources();
}

void onsignal_close_resources (int)
{
    close_resources();
    exit(1);
}

int main(int argc, char *argv[])
{
    std::string srcFilePath="-";

    std::string logSinkName;

    unsigned long maxEvents=ULONG_MAX;

    double probSend=0.5;

    unsigned logLevel=2;

    int messageInit=-1;

    std::string strategyName="Random";

    std::mt19937 urng;
    urng.seed(0);

    int ia=1;
    while(ia < argc){
        if(!strcmp("--help",argv[ia])){
            usage();
        }else if(!strcmp("--log-level",argv[ia])){
            if(ia+1 >= argc){
                fprintf(stderr, "Missing argument to --log-level\n");
                usage();
            }
            logLevel=strtoul(argv[ia+1], 0, 0);
            ia+=2;
        }else if(!strcmp("--max-events",argv[ia])){
            if(ia+1 >= argc){
                fprintf(stderr, "Missing argument to --max-events\n");
                usage();
            }
            maxEvents=strtoul(argv[ia+1], 0, 0);
            ia+=2;
        }else if(!strcmp("--prob-send",argv[ia])){
            if(ia+1 >= argc){
                fprintf(stderr, "Missing argument to --prob-send\n");
                usage();
            }
            probSend=strtod(argv[ia+1], 0);
            ia+=2;
        }else if(!strcmp("--set-seed",argv[ia])){
            urng.seed(atoi(argv[ia+1]));
            ia+=2;    
        }else if(!strcmp("--random-seed",argv[ia])){
            std::mt19937::result_type seeds[16];
            seeds[0]=time(NULL);
            seeds[1]=getpid();
            std::random_device rd;
            std::generate(std::begin(seeds)+2, std::end(seeds), std::ref(rd));
            std::seed_seq seeder(std::begin(seeds), std::end(seeds));
            urng.seed(seeder);
            ia++;
        }else if(!strcmp("--log-events",argv[ia])){
            if(ia+1 >= argc){
                fprintf(stderr, "Missing two arguments to --log-events destination \n");
                usage();
            }
            logSinkName=argv[ia+1];
            ia+=2;
        }else if(!strcmp("--message-init",argv[ia])){
            if(ia+1 >= argc){
                fprintf(stderr, "Missing argument to --message-init\n");
                usage();
            }
            messageInit=strtoul(argv[ia+1], 0, 0);
            ia+=2;
            if (messageInit > 2) {
                fprintf(stderr, "Argument for --message-init is too large. Defaulting to 0\n");
            }
        }else if(!strcmp("--strategy",argv[ia])){
            if(ia+1 >= argc){
                fprintf(stderr, "Missing argument to --strategy\n");
                usage();
            }
            strategyName=argv[ia+1];
            ia+=2;
        }else{
            srcFilePath=argv[ia];
            ia++;
        }
    }

    if(messageInit!=-1){
        fprintf(stderr, "Explicit messageInit not supported yet.\n");
        exit(1);
    }

    RegistryImpl registry;

    xmlpp::DomParser parser;

    filepath srcPath(current_path());

    if(srcFilePath!="-"){
        filepath p(srcFilePath);
        p=absolute(p);
        if(logLevel>1){
            fprintf(stderr,"Parsing XML from '%s' ( = '%s' absolute)\n", srcFilePath.c_str(), p.c_str());
        }
        srcPath=p.parent_path();
        parser.parse_file(p.c_str());
    }else{
        if(logLevel>1){
            fprintf(stderr, "Parsing XML from stdin (this will fail if it is compressed\n");
        }
        parser.parse_stream(std::cin);
    }
    if(logLevel>1){
        fprintf(stderr, "Parsed XML\n");
    }

    atexit(atexit_close_resources);
    signal(SIGABRT, onsignal_close_resources);
    signal(SIGINT, onsignal_close_resources);

    if(!logSinkName.empty()){
        g_pLog.reset(new LogWriterToFile(logSinkName.c_str()));
    }

    std::shared_ptr<SimulationEngine> engine;


    engine = std::make_shared<SimulationEngineFast>(g_pLog);
    engine->setLogLevel(logLevel);

    loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), engine.get());
    if(logLevel>1){
        fprintf(stderr, "Loaded\n");
    }

    std::shared_ptr<BasicStrategy> strategy;
    if(strategyName=="FIFO") {
        strategy = std::make_shared<InOrderQueueStrategy>(engine);
    }else if(strategyName=="Random") {
        strategy = std::make_shared<OutOfOrderStrategy>(engine);
    }else if(strategyName=="LIFO"){
        strategy = std::make_shared<ReverseOrderStrategy>(engine);
    }else{
        fprintf(stderr, "Don't understand strategy '%s'\n", strategyName.c_str());
    }

    strategy->init(urng);
    fprintf(stderr, "Inited\n");

    unsigned long steps=0;
    while(strategy->step()){
        if(steps >= maxEvents){
            fprintf(stderr, "maxEvents exceeded.\n");
            break;
        }
        //fprintf(stderr, "Stepped\n");
        ++steps;
    }
    fprintf(stderr, "Application has gone idle.\n");

    return 0;
}
