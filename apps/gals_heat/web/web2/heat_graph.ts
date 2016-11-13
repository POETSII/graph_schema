/// <reference path="heat_types.ts" />
/// <reference path="heat_dirichlet.ts" />
/// <reference path="heat_cell.ts" />

import * as POETS from "./core"

import {HeatGraphProperties} from "./heat_types"
import {UpdateMessage,initEdgeType,updateEdgeType} from "./heat_types"
import {dirichletDeviceType} from "./heat_dirichlet"
import {cellDeviceType} from "./heat_cell"

import assert = POETS.assert;
import TypedData = POETS.TypedData;
import EdgeType = POETS.EdgeType;
import DeviceType = POETS.DeviceType;
import GraphType = POETS.GraphType;
import GenericTypedDataSpec = POETS.GenericTypedDataSpec;
import InputPort = POETS.InputPort;
import OutputPort = POETS.OutputPort;

export const heatGraphType = new GraphType(
    "gals_heat",
    new GenericTypedDataSpec(HeatGraphProperties),
    [
        dirichletDeviceType,
        cellDeviceType
    ]
);

export function makeGrid(w : number, h : number) : POETS.GraphInstance
{
    var g = new POETS.GraphInstance(heatGraphType);

    for(var y=0; y<w; y++){
        let T = y==0;
        let B = y==h-1;
        let H = T||B;
        for(var x=0; x<h; x++){
            let L = x==0;
            let R = x==w-1;
            let V = L||R;

            if( H && V )
                continue

            let id=`d_${x}_${y}`;

            if( H || V){
                g.addDevice(id, dirichletDeviceType, {}, {x:x,y:y});
            }else{
                g.addDevice(id, cellDeviceType, {nhood:1, wSelf:0.25}, {x:x,y:y});
            }

            console.log(` ${y} ${x}`);
        }
    }

    for(var y=0; y<w; y++){
        let T = y==0;
        let B = y==h-1;
        let H = T||B;
        for(var x=0; x<h; x++){
            let L = x==0;
            let R = x==w-1;
            let V = L||R;

            if( H && V )
                continue;

            let id=`d_${x}_${y}`;

            if(L){
                g.addEdge(`d_${x+1}_${y}`, "in", id, "out");
            }else if(R){
                g.addEdge(`d_${x-1}_${y}`, "in", id, "out");
            }else if(T){    
                g.addEdge(`d_${x}_${y+1}`, "in", id, "out");
            }else if(B){
                g.addEdge(`d_${x}_${y-1}`, "in", id, "out");
            }else{
                g.addEdge(`d_${x-1}_${y}`, "in", id, "out", {w:0.125});
                g.addEdge(`d_${x+1}_${y}`, "in", id, "out", {w:0.125});
                g.addEdge(`d_${x}_${y-1}`, "in", id, "out", {w:0.125});
                g.addEdge(`d_${x}_${y+1}`, "in", id, "out", {w:0.125});
            }

            console.log(` ${y} ${x}`);
        }
    }

    return g;
}
