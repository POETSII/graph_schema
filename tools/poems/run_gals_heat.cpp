#include "poems.hpp"
#include "gals_heat_type.hpp"

int main()
{
    POEMS instance;

    POEMSBuilder builder(instance);

    int n=1000;

    auto gt=make_gals_heat_graph_type();
    auto dt=gt->getDeviceType("cell");
    auto in=dt->getInput("in");
    auto out=dt->getOutput("out");

    auto gp=gt->getPropertiesSpec()->create();
    ((gals_heat_properties_t*)gp.payloadPtr())->max_t=10;

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

    fprintf(stderr, "Running.\n");
    instance.run(1);
}