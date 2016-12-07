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

export function makeGrid(width : number, height : number) : POETS.GraphInstance
{

    let h=Math.sqrt(1.0/(width*height));
    let alpha=1;

    let dt=h*h / (4*alpha) * 0.5;
    //let dt=0.05;

    assert(h*h/(4*alpha) >= dt);

    let wOther = dt*alpha/(h*h);
    let wSelf = (1.0 - 4*wOther);
    
    console.log(` wOther=${wOther}, eSelf=${wSelf}`);

    var g = new POETS.GraphInstance(heatGraphType, {maxTime:1000000});

    var makeVoronoiCell = function(x:number,y:number) : number[][]{
        var res = [ [x-0.5,y-0.5], [x-0.5,y+0.5],[x+0.5,y+0.5],[x+0.5,y-0.5] ];
        return res;
    };

    for(var y=0; y<width; y++){
        let T = y==0;
        let B = y==height-1;
        let H = T||B;
        for(var x=0; x<height; x++){
            let L = x==0;
            let R = x==width-1;
            let V = L||R;

            if( H && V )
                continue

            let id=`d_${x}_${y}`;
            let voronoi=makeVoronoiCell(x,y);

            if( x == Math.floor(width/2) && y==Math.floor(height/2)){
                let props={ "bias":0, "amplitude":1.0, "phase":1.5, "frequency": 100*dt, "neighbours":4 };
                g.addDevice(id, dirichletDeviceType, props, {x:x,y:y, voronoi:voronoi});
            } else if( H || V){
                let props={ "bias":0, "amplitude":1.0, "phase":1, "frequency": 70*dt*((x/width)+(y/height)), "neighbours":1 };
                g.addDevice(id, dirichletDeviceType, props, {x:x,y:y});
            }else{
                let props={nhood:4, wSelf:wSelf, iv:Math.random()*2-1 };
                g.addDevice(id, cellDeviceType, props, {x:x,y:y});
            }

            console.log(` ${y} ${x}`);
        }
    }

    for(var y=0; y<width; y++){
        let T = y==0;
        let B = y==height-1;
        let H = T||B;
        for(var x=0; x<height; x++){
            let L = x==0;
            let R = x==width-1;
            let V = L||R;

            if( H && V )
                continue;

            let id=`d_${x}_${y}`;

            var addEdge = function(dstX:number,dstY:number){
                let buddy=`${id}:in-d_${dstX}_${dstY}:out`;
                g.addEdge(`d_${dstX}_${dstY}`, "in", id, "out", {w:wOther}, {buddy:buddy});
            };

            if(L){
                addEdge(x+1, y);
            }else if(R){
                addEdge(x-1,y);
            }else if(T){
                addEdge(x,y+1);
            }else if(B){
                addEdge(x,y-1);
            }else{
                addEdge(x-1,y);
                addEdge(x+1,y);
                addEdge(x,y-1);
                addEdge(x,y+1);
            }

            console.log(` ${y} ${x}`);
        }
    }

    return g;
}


