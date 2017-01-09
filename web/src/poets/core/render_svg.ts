import * as POETS from "./core"
import * as jq from "jquery"
//import * as svgjs from "svg.js"

import assert = POETS.assert;
import TypedData = POETS.TypedData;
import EdgeType = POETS.EdgeType;
import DeviceType = POETS.DeviceType;
import GraphType = POETS.GraphType;
import GenericTypedDataSpec = POETS.GenericTypedDataSpec;
import InputPort = POETS.InputPort;
import OutputPort = POETS.OutputPort;
import DeviceInstance = POETS.DeviceInstance;
import EdgeInstance = POETS.EdgeInstance;
import GraphInstance = POETS.GraphInstance;


export class RenderSVG
{
    drawing:svgjs.Doc;

    private _devToSvg : { [key:string]:svgjs.Element; } = {};

    constructor(
        public graph:GraphInstance,
        public container:HTMLDivElement
        )
    {
        this.drawing=SVG(container);

        let diameter = 5;

        for(let d of graph.enumDevices()){
            let meta=d.metadata;
            let x=meta["x"];
            let y=meta["y"];

            let n=this.drawing.circle(diameter);
            n.move(x*10+70,y*10+70);
            this._devToSvg[d.id]=n;
        }

        this.drawing.scale(2,2);
    }

};