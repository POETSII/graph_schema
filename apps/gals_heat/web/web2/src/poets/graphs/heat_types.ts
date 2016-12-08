/// <reference path="../core/core.ts" />

import * as POETS from "../core/core" 

import assert = POETS.assert;
import TypedData = POETS.TypedData;
import EdgeType = POETS.EdgeType;
import DeviceType = POETS.DeviceType;
import GraphType = POETS.GraphType;
import GenericTypedDataSpec = POETS.GenericTypedDataSpec;
import InputPort = POETS.InputPort;
import OutputPort = POETS.OutputPort;

export class UpdateMessage
    implements TypedData
{
    constructor(
        public readonly source_device : string,
        public readonly source_port : string,
        public t : number = 0,
        public v : number = 0
    ) {};

    readonly _name_ ="update_message";
    
    
};


export class UpdateEdgeProperties
{    
    readonly _name_ ="update_properties";

    constructor(
        public w : number = 0
    ){};
}


export const initEdgeType = new EdgeType("__init__");

export const updateEdgeType = new EdgeType("update", new GenericTypedDataSpec(UpdateMessage), new GenericTypedDataSpec(UpdateEdgeProperties));


export class HeatGraphProperties
    implements TypedData
{
    readonly _name_ ="heat_properties";

    constructor(
        public maxTime : number = 1000000
    ) {};
};

