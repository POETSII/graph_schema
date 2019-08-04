#include "gals_heat_type.hpp"

#include "poems.hpp"


int main(int argc, char *argv[])
{
    int n=1000;
    int max_t=10;
    int nThreads=std::thread::hardware_concurrency();
    int cluster_size=1024;
    bool use_metis=true;

    POEMS instance;

    if(argc>1){
        n=atoi(argv[1]);
    }
    if(argc>2){
        max_t=atoi(argv[2]);
    }
    if(argc>3){
        nThreads=atoi(argv[3]);
    }
    if(argc>4){
        cluster_size=atoi(argv[4]);
    }
    if(argc>5){
        if(!strcmp(argv[5],"random")){
            use_metis=false;
        }else if(!strcmp(argv[5],"metis")){
            use_metis=true;
        }else{
            throw std::runtime_error("Unknown partition method.");
        }
    }

    fprintf(stderr, "n=%u, max_t=%u, nThreads=%u, cluster_size=%u, use_metus=%u\n",
        n, max_t, nThreads, cluster_size, use_metis
    );

    instance.m_cluster_size=cluster_size;

    POEMSBuilder builder(instance);

    auto gt=make_gals_heat_graph_type();
    auto dt=gt->getDeviceType("cell");
    auto in=dt->getInput("in");
    auto out=dt->getOutput("out");

    auto gp=gt->getPropertiesSpec()->create();
    ((gals_heat_properties_t*)gp.payloadPtr())->max_t=max_t;

    auto gid=builder.onBeginGraphInstance(
        gt, "wibble", gp, rapidjson::Document()
    );

    std::vector<uint64_t> devices;

    builder.onBeginDeviceInstances(gid);

    for(unsigned i=0;i<n*n;i++){
        int y=i/n, x=i%n;
        bool edgey= y==0 or y==n-1;
        bool edgex= x==0 or x==n-1;
        int degree=4-edgex-edgey;

        std::string id="c_"+std::to_string(x)+"_"+std::to_string(y);
        auto p=dt->getPropertiesSpec()->create();
        ((cell_properties_t*)p.payloadPtr())->degree=degree;
        ((cell_properties_t*)p.payloadPtr())->fixed=edgex or edgey ;
        ((cell_properties_t*)p.payloadPtr())->wSelf=0.25;

        auto s=dt->getStateSpec()->create();

        devices.push_back(
            builder.onDeviceInstance(
                gid, dt, id, p, s
            )
        );
    }

    builder.onEndDeviceInstances(gid);
    
    builder.onBeginEdgeInstances(gid);

    for(unsigned i=0; i<n*n; i++){
        int y=i/n, x=i%n;
        bool edgey= y==0 or y==n-1;
        bool edgex= x==0 or x==n-1;
        int degree=4-edgex-edgey;

        auto p=dt->getInput("in")->getPropertiesSpec()->create();
        ((cell_in_properties_t*)p.payloadPtr())->w=0.75 / degree;

        TypedDataPtr s;

        if(x!=0){
            builder.onEdgeInstance(
                gid, i, dt, in, y*n+x-1, dt, out, -1, p, s
            );
        }
        if(y!=0){
            builder.onEdgeInstance(
                gid, i, dt, in, (y-1)*n+x, dt, out, -1, p, s
            );
        }
        if(x!=n-1){
            builder.onEdgeInstance(
                gid, i, dt, in, y*n+x+1, dt, out, -1, p, s
            );
        }
        if(y!=n-1){
            builder.onEdgeInstance(
                gid, i, dt, in, (y+1)*n+x, dt, out, -1, p, s
            );
        }
    }

    builder.onEndEdgeInstances(gid);

    builder.onEndGraphInstance(gid);

    fprintf(stderr, "Running, setup=%f.\n", clock()/(double)CLOCKS_PER_SEC);
    instance.run(nThreads);
}