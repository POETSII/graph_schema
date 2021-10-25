#include "graph.hpp"
#include "graph_persist_sax_writer_impl.hpp"

#include "rapidjson/document.h"

#include "pressure_sync.graph.hpp"

#include <string>
#include <cstdlib>
#include <random>

int main(int argc, char *argv[])
{
  RegistryImpl registry;

  if(argc<2){
    fprintf(stderr, "usage : create_pressure_sync_instance dstFile [n]\n");
    fprintf(stderr, "  Note that dstFile can't be - at the moment.\n");
    exit(1);
  }

  auto writer=createSAXWriterOnFile(argv[1]);
  
  GraphTypePtr graphType=registry.lookupGraphType("pressure_sync");
  auto cellType=graphType->getDeviceType("cell");
  auto cellInPin = cellType->getInput("in");
  auto cellOutPin = cellType->getOutput("out");

  writer->onGraphType(graphType);

  int D=10;
  int T=100;

  auto graphProperties=DataPtr<pressure_sync_properties_t>::create_zero_filled();

  std::string graph_name="pressure_sync_"+std::to_string(D);
  uint64_t gId=writer->onBeginGraphInstance(graphType, graph_name, graphProperties, {});

  writer->onBeginDeviceInstances(gId);

  std::mt19937_64 rng;

  int random_lsbs=0;
  while( (D*D*D << random_lsbs) < 0x80000000){
    random_lsbs++;
  }

  std::vector<std::vector<std::vector<uint64_t>>> devs;
  char name_temp[64];
  devs.resize(D);
  for (int x = 0; x < D; x++){
    devs[x].resize(D);
    for (int y = 0; y < D; y++){
      devs[x][y].resize(D);
      for (int z = 0; z < D; z++){
        sprintf(name_temp, "d_%02x_%02x_%02x", x, y, z);
        
        auto cellState=DataPtr<pressure_sync_cell_state_t>::create_zero_filled();
        cellState->numSteps=T;
        auto cellProps=DataPtr<pressure_sync_cell_properties_t>::create_zero_filled();
        cellProps->seed = ((x + y * D + z * D * D) << random_lsbs) | (rng()& ((1<<random_lsbs)-1));
        if(cellProps->seed==0){
          cellProps->seed++;
        }
        cellProps->isRoot = x==0 && y==0 && z==0;
        devs[x][y][z]=writer->onDeviceInstance(gId,
          cellType, name_temp, cellProps, cellState, {});
      }
    }
  }

  writer->onEndDeviceInstances(gId);

  writer->onBeginEdgeInstances(gId);

  std::vector<DataPtr<cell_in_properties_t>> labels;
  for(int i=0; i<26; i++){
    labels.push_back(DataPtr<cell_in_properties_t>::create_zero_filled());
    labels[i]->dir=i;
  }

  for (int x = 0; x < D; x++){
    for (int y = 0; y < D; y++){
      for (int z = 0; z < D; z++) {
        int label = 0;
        for (int i = -1; i < 2; i++){
          for (int j = -1; j < 2; j++){
            for (int k = -1; k < 2; k++) {
              if (! (i == 0 && j == 0 && k == 0)) {
                int xd = (x+i) < 0 ? (D-1) : ((x+i) >= D ? 0 : (x+i));
                int yd = (y+j) < 0 ? (D-1) : ((y+j) >= D ? 0 : (y+j));
                int zd = (z+k) < 0 ? (D-1) : ((z+k) >= D ? 0 : (z+k));
                //graph.addLabelledEdge(label, devs[x][y][z], 0, devs[xd][yd][zd]);
                auto from=devs[x][y][z];
                auto to=devs[xd][yd][zd];
                writer->onEdgeInstance(gId,
                  to, cellType, cellInPin,
                  from, cellType, cellOutPin,
                  -1, labels[label], {}, {}
                );
                label++;
              }
            }
          }
        }
      }
    }
  }

  writer->onEndEdgeInstances(gId);

  writer->onEndGraphInstance(gId);

  return 0;
}
