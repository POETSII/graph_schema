#include "poems.hpp"

#include "graph_persist_dom_reader.hpp"

FILE *g_stats_file;
std::string g_stats_prefix;

void stats_log(const std::string &thing, const std::string &sub_thing, const std::string &value)
{
    if(g_stats_file){
        std::stringstream acc;  
        acc<<g_stats_prefix<<thing<<","<<sub_thing<<","<<value<<"\n";
        fputs(acc.str().c_str(), g_stats_file);
    }
}

std::function<void()> _at_exit_hook;

void at_exit_raw()
{
    _at_exit_hook();
}

int main(int argc, char *argv[])
{    
    set_now_start();

    filepath source_path;
    filepath stats_file_path;
    int nThreads=std::thread::hardware_concurrency();
    int cluster_size=1024;
    int use_metis=1;

    int ai=1;

    auto parse_str_opt=[&](const char *name, std::string &val){
        if(strcmp(argv[ai], name)){
            return false;
        }
        if(ai+1>=argc){
            fprintf(stderr, "Missing argument to %s\n", name);
            exit(1);
        }
        val=argv[ai+1];
        ai+=2;
        return true;
    };
    auto parse_int_opt=[&](const char *name, int &val){
        std::string s;
        if(!parse_str_opt(name, s)){
            return false;
        }
        val=std::atoi(s.c_str());
        return true;
    };
    auto parse_path_opt=[&](const char *name, filepath &val){
        std::string s;
        if(!parse_str_opt(name, s)){
            return false;
        }
        val=filepath(s);
        return true;
    };

    auto usage=[&]()
    {
        fprintf(stderr, R"(
usage : %s [--threads n] [--cluster-size n] [--use-metis 0|1] <source.xml>
--threads : How many threads to use for simulation (default is std::thread::hardware_concurrency)
--cluster-size : Target number of devices per cluster (default is 1024)
--use-metis : Whether to cluster using metis (default is 1)
--stats-file file : A file to log execution information to.
)", argv[0]);
    };

    while(ai<argc){
        if(!strcmp(argv[ai], "--help")){
            usage();
            exit(1);
        }else if(parse_int_opt("--threads", nThreads)) {}
        else if(parse_int_opt("--cluster-size", cluster_size)) {}
        else if(parse_int_opt("--use-metis", use_metis)) {}
        else if(parse_path_opt("--stats-file", stats_file_path)) {}
        else{
            if(source_path.native()!=""){
                fprintf(stderr, "Received more than one source path (mis-spelled option?)\n");
                exit(1);
            }
            source_path=filepath(argv[ai]);
            ai++;
        }
    }


    POEMS instance;

    fprintf(stderr, "nThreads=%u, cluster_size=%u, use_metus=%u, source_path=%s\n",
        nThreads, cluster_size, use_metis, source_path.c_str()
    );

    if(nThreads<=0){
        fprintf(stderr, "Invalid number of threads.\n");
        exit(1);
    }
    if(cluster_size<=0){
        fprintf(stderr, "Invalid cluster size.\n");
        exit(1);
    }

    if(stats_file_path.native()!=""){
        g_stats_file=fopen(stats_file_path.c_str(), "wt");
        if(!g_stats_file){
            fprintf(stderr, "Couldn't open stats file '%s'.\n", stats_file_path.c_str());
            exit(1);
        }
    }

    stats_log("threads_used", "", std::to_string(nThreads));
    stats_log("cluster_size", "", std::to_string(cluster_size));
    stats_log("use_metis", "", std::to_string(use_metis));


    instance.use_metis=use_metis;
    instance.m_cluster_size=cluster_size;

    POEMSBuilder builder(instance);
    builder.stats_log=stats_log;

    filepath srcDir;

    if(source_path.native()=="-"){
        source_path=filepath("/dev/stdin");
        srcDir=current_path();
    }else{
        srcDir=source_path.parent_path();
    }

    xmlpp::DomParser parser;
    parser.parse_file(source_path.c_str());

    loadGraph(nullptr, srcDir, parser.get_document()->get_root_node(), &builder);

    fprintf(stderr, "Running, setup=%f.\n", clock()/(double)CLOCKS_PER_SEC);

    auto beginTime=std::chrono::high_resolution_clock::now();

    _at_exit_hook=[&](){
        auto endTime=std::chrono::high_resolution_clock::now();
        double runTime=std::chrono::duration<double>(endTime-beginTime).count();
        uint64_t messages=instance.total_received_messages_approx();
        fprintf(stderr, "Sim time = %g secs, M messages received = %f, MReceive/sec = %f\n",  runTime, messages/(1000.0*1000.0), messages/runTime/(1000.0*1000.0));
    };
    atexit(at_exit_raw);

    instance.run(nThreads);
}