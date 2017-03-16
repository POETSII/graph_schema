/// <reference path="heat_types.ts" />
/// <reference path="heat_dirichlet.ts" />
/// <reference path="heat_cell.ts" />

import * as POETS from "../core/core"

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
    new GenericTypedDataSpec(HeatGraphProperties, HeatGraphProperties.elements),
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
    
    var g = new POETS.GraphInstance(heatGraphType, `heat_rect_${width}_${height}`, {maxTime:1000000});

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
        }
    }

    return g;
}

export function makeGridHex(width : number, height : number) : POETS.GraphInstance
{

    if((width%2)==0) width++;
    if((height%2)==0) height++;

    let h=Math.sqrt(1.0/(width*height));
    let alpha=1;

    let dt=h*h / (6*alpha) * 0.5;
    //let dt=0.05;

    assert(h*h/(6*alpha) >= dt);

    let wOther = dt*alpha/(h*h);

    let d = Math.sqrt(3.0) / 4.0;
    
    var g = new POETS.GraphInstance(heatGraphType, `heat_hex_${width}_${height}`, {maxTime:1000000});

    /* We place hexagons at points as follows:

                            |     |
                            +--1--+
                            |     |

        +---+       +---+       +---+         -+----+-
       /     \     /     \     /     \         d    |
      +  0,0  +---+  2,0  +---+  4,0  +       -+-  2*d
       \     /     \     /     \     /              |
        +---+  1,1  +---+  3,1  +---+              -+-
       /     \     /     \     /     \
      +  0,2  +---+  2,2  +---+  4,2  +
       \     /     \     /     \     /
        +---+       +---+       +---+

            +
           /|\
      0.5 / |d\
         /  |  \
        +---+---+
        0.25  0.25

        d=sqrt(3)/4

        So even y co-ordinates have hexagons at even x co-ords,
        and odd y at odd y. Equivalently, hexagons appear when
        the sum of co-ordinates is even.

        The world location is:
            (1+x,(1+y)*d)

        
    */
    
    var makeVoronoiCell = function(x:number,y:number) : number[][]{
        var res = [ [x-0.5,y], [x-0.25,y+d], [x+0.25,y+d], [x+0.5,y], [x+0.25, y-d], [x-0.25,y-d]   ];
        return res;
    };

    var isGap = function(x:number,y:number) : boolean{
        let ox = ((x+0.5)/width-0.5)*2;
        let oy = ((y+0.5)/height-0.5)*2;
        return Math.sqrt(ox*ox+oy*oy) < 0.4;
    };

    var makeNhood = function(x:number,y:number) : number[][]{
        var nhood = [ [x-1,y-1],[x,y+2],[x-1,y+1],[x+1,y+1],[x,y-2],[x+1,y-1] ];
        nhood=nhood.filter( (v) => (v[0]>=0) && (v[0]<width) && (v[1]>=0) && (v[1]<height) );
        nhood=nhood.filter( (v) => !isGap(v[0],v[1] ));
        return nhood;
    };

    for(let x=0; x<width; x++){
        for(let y=0;y<height;y++){
            if( ((x+y)%2)!=0 ){
                continue;
            }
            if(isGap(x,y)){
                continue;
            }

            var nhood = makeNhood(x,y);

            let id=`d_${x}_${y}`;
            let nhoodSize = nhood.length;
            let isDirichlet = (x==0 || x==width-1);
            let voronoi=makeVoronoiCell(x,y);
            let wSelf = (1.0 - nhoodSize*wOther);


            let px=1.0+x;
            let py=(1.0+y)*d;

            console.log(id);

            var dev:DeviceType;
            if(isDirichlet){
                let props={ "bias":0, "amplitude":1.0, "phase":1, "frequency": 70*dt*((x/width)+(y/height)), "neighbours":nhoodSize };
                g.addDevice(id, dirichletDeviceType, props, {x:px,y:py, voronoi:voronoi});
            }else{
                let props={nhood:nhoodSize, wSelf:wSelf, iv:Math.random()*2-1 };
                g.addDevice(id, cellDeviceType, props, {x:px,y:py, voronoi:voronoi});
            }
        }
    }

    for(let x=0; x<width; x++){
        for(let y=0;y<height;y++){
            if( ((x+y)%2)!=0 ){
                continue;
            }
            if(isGap(x,y)){
                continue;
            }

            var nhood = makeNhood(x,y);

            console.log(`${x},${y} -> ${nhood}`);

            let id=`d_${x}_${y}`;
            let nhoodSize = nhood.length;
            let isDirichlet = (x==0 || x==width-1 || y==0 || y==height-1);

            var addEdge = function(dstX:number,dstY:number){
                assert(dstX>=0);
                assert(dstX<width);
                assert(dstY>=0);
                assert(dstY<height);
                let buddy=`${id}:in-d_${dstX}_${dstY}:out`;
                g.addEdge(`d_${dstX}_${dstY}`, "in", id, "out", {w:wOther}, {buddy:buddy});
            };

            for(let [sx,sy] of nhood){
                addEdge(sx,sy);
            }

        }
    }

    return g;
}


