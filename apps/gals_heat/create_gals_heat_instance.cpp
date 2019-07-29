#include "graph.hpp"
#include "graph_persist_sax_writer.hpp"

#include "rapidjson/document.h"

#include "gals_heat.graph.hpp"

#include <string>
#include <cstdlib>

int main(int argc, char *argv[])
{
  RegistryImpl registry;

  if(argc<2){
    fprintf(stderr, "usage : create gals_heat_instance dstFile [n]\n");
    fprintf(stderr, "  Note that dstFile can't be - at the moment.\n");
    exit(1);
  }

    auto writer=createSAXWriterOnFile(argv[1]);
  
  GraphTypePtr graphType=registry.lookupGraphType("gals_heat");

  unsigned exportDeltaMask=1023;

  unsigned n=16;
  if(argc>2){
    n=strtol(argv[2],0,0);
  }

  assert(n>=3);

  double h=1.0/n;
  double alpha=1;
    
  double dt=h*h / (4*alpha) * 0.5;
  assert(h*h/(4*alpha) >= dt);

  double weightOther = dt*alpha/(h*h);
  double weightSelf = (1.0 - 4*weightOther);

  auto cellType=graphType->getDeviceType("cell");
  auto dirichletType=graphType->getDeviceType("dirichlet_variable");
  
  std::string instName="heat_"+std::to_string(n)+"_"+std::to_string(n);

  DataPtr<gals_heat_properties_t> graphProperties(new gals_heat_properties_t());
  graphProperties->maxTime=n;
  graphProperties->exportDeltaMask=exportDeltaMask;

  auto xyToIndex=[=](unsigned x, unsigned y)
    { return x*n+y; };
  std::vector<std::pair<uint64_t,bool> > nodes(n*n);// Used to hold mapping to writers view of node ids, plus true=dirichlet, false=cell

  writer->onGraphType(graphType);

  auto gInst=writer->onBeginGraphInstance(graphType, instName, graphProperties, rapidjson::Document());

  writer->onBeginDeviceInstances(gInst);

  for(unsigned x=0;x<n;x++){
    fprintf(stderr," Devices : Row %d of %d\n", x, n);
    for(unsigned y=0;y<n;y++){
      double nativeLocation[2]={(double)x,(double)y};
      bool edgeX = (x==0) || (x==n-1);
      bool edgeY = (y==0) || (y==n-1);

      if(edgeX && edgeY)
	      continue;

      rapidjson::Document meta;
      rapidjson::Value loc(rapidjson::kArrayType);
      loc.PushBack(x, meta.GetAllocator());
      loc.PushBack(y, meta.GetAllocator());
      meta.AddMember("loc", loc, meta.GetAllocator());
      
      if(x==n/2 && y==n/2){
        auto props=make_data_ptr<dirichlet_variable_properties_t>();
        props->bias=0;
        props->amplitude=1;
        props->phase=1.5;
        props->frequency=100*dt;
        props->neighbours=4;
        std::string id="v_"+std::to_string(x)+"_"+std::to_string(y);
        uint64_t index=writer->onDeviceInstance(gInst, dirichletType, id, props, std::move(meta));
        nodes[xyToIndex(x,y)]=std::make_pair(index,true);
        //fprintf(stderr, "  (%d,%d) -> %llu\n", x,y, index);
      }else if(edgeX != edgeY){
        auto props=make_data_ptr<dirichlet_variable_properties_t>();
        props->bias=0;
        props->amplitude=1;
        props->phase=1;
        props->frequency=70*dt*(x/(double)n + y/(double)n);
        props->neighbours=1;
        std::string id="v_"+std::to_string(x)+"_"+std::to_string(y);
        uint64_t index=writer->onDeviceInstance(gInst, dirichletType, id, props, std::move(meta));
        nodes[xyToIndex(x,y)]=std::make_pair(index,true);
        //fprintf(stderr, "  (%d,%d) -> %llu\n", x,y, index);
      }else{
        auto props=make_data_ptr<cell_properties_t>();
        props->iv=drand48()*2-1;
        std::string id="c_"+std::to_string(x)+"_"+std::to_string(y);
        uint64_t index=writer->onDeviceInstance(gInst, cellType, id, props, std::move(meta));
        nodes[xyToIndex(x,y)]=std::make_pair(index,true);
        //fprintf(stderr, "  (%d,%d) -> %llu\n", x,y, index);
      }
    }
  }

  writer->onEndDeviceInstances(gInst);

  auto dirichletIn=dirichletType->getInput("in");
  auto cellIn=cellType->getInput("in");
  auto dirichletOut=dirichletType->getOutput("out");
  auto cellOut=cellType->getOutput("out");
  
  auto add_channel=[&](unsigned x,unsigned y ,unsigned dx,unsigned dy)
    {
      auto dst=nodes[ xyToIndex(x,y) ];
      auto src=nodes[ xyToIndex( (x+dx+n)%n, (y+dy+n)%n ) ];

      //      fprintf(stderr,"(%d,%d):%u <- (%d,%d):%u\n", x,y, dst.first, (x+dx+n)%n, (y+dy+n)%n, src.first);

      writer->onEdgeInstance
      (
        gInst,
        dst.first, dst.second ? dirichletType : cellType, dst.second ? dirichletIn : cellIn,
        src.first, src.second ? dirichletType : cellType, src.second ? dirichletOut : cellOut,
        TypedDataPtr()
       );
    };
  
  writer->onBeginEdgeInstances(gInst);
  
  for(unsigned x=0; x<n; x++){
    fprintf(stderr, " Edges : Row %d of %d\n", x,n);
    for(unsigned y=0; y<n; y++){
      bool edgeX = (x==0) || (x==n-1);
      bool edgeY = (y==0) || (y==n-1);
      if(edgeX && edgeY)
	continue;

      if(y!=0 && !edgeX)
	add_channel(x,y, 0, -1);
      if(x!=n-1 && ! edgeY)
	add_channel(x,y, +1, 0);
      if(y!=n-1 && ! edgeX)
	add_channel(x,y, 0, +1);
      if(x!=0 && ! edgeY)
	add_channel(x,y, -1, 0);
    }
  }

  writer->onEndEdgeInstances(gInst);

  writer->onEndGraphInstance(gInst);

  return 0;
}
