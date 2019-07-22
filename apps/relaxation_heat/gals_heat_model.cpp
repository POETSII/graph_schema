#include "gals_heat_model.hpp"

int main()
{
    auto cellType=std::make_shared<class GALSHeatDeviceType>();

    world_state_t ws;

    const unsigned W=3;
    const unsigned H=3;

    device_id_t devices[W][H];

    for(unsigned x=0; x<W; x++){
        for(unsigned y=0; y<H; y++){
            uint8_t n=(x!=0) + (y!=0) + (x!=W-1) + (y!=H-1);
            devices[x][y]=ws.addDevice(cellType, {n});
        }
    }

    for(unsigned x=0; x<W; x++){
        for(unsigned y=0; y<H; y++){
            if(x!=0)    ws.addEdge({devices[x-1][y],0},{devices[x][y],0} );
            if(y!=0)    ws.addEdge({devices[x][y-1],0},{devices[x][y],0} );
            if(x!=W-1)    ws.addEdge({devices[x+1][y],0},{devices[x][y],0} );
            if(y!=H-1)    ws.addEdge({devices[x][y+1],0},{devices[x][y],0} );
        }
    }

    
    for(auto &d : ws.devices){
        fprintf(stderr, "%llu, %llu\n", d.baseHash.hash, d.currHash.hash);
    }

    run(ws);

    fprintf(stderr, "|terminal| = %u\n", ws.terminal.size());
}
