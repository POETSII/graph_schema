/// <reference path="../core/core.ts" />

import * as POETS from "../core/core" 

import {tFloat,tInt,tBoolean} from "../core/core"

import assert = POETS.assert;
import TypedData = POETS.TypedData;
import TypedDataType = POETS.TypedDataType;
import EdgeType = POETS.EdgeType;
import DeviceType = POETS.DeviceType;
import GraphType = POETS.GraphType;
import TypedDataSpec = POETS.TypedDataSpec;
import GenericTypedDataSpec = POETS.GenericTypedDataSpec;
import InputPort = POETS.InputPort;
import OutputPort = POETS.OutputPort;

export class UpdateMessage
    extends TypedData
{
    static elements : TypedDataType[] = [
        tInt("t"),
        tFloat("v")
    ];

    constructor(
        _spec_ : TypedDataSpec,
        public t : number = 0,
        public v : number = 0
    ) {
        super("update_message", _spec_);
    };   
};


export class UpdateEdgeProperties
    extends TypedData
{    
    static elements : TypedDataType[] = [
        tFloat("w")
    ];

    constructor(
        _spec_:TypedDataSpec,
        public w : number = 0
    ){
        super("update_properties", _spec_);
    };
}


export const initEdgeType = new EdgeType("__init__");

export const updateEdgeType = new EdgeType(
    "update",
    new GenericTypedDataSpec(UpdateMessage, UpdateMessage.elements),
    new GenericTypedDataSpec(UpdateEdgeProperties, UpdateEdgeProperties.elements)
);


export class HeatGraphProperties
    extends TypedData
{
    static elements : TypedDataType[] = [
        tInt("maxTime")
    ];

    constructor(
        _spec_:TypedDataSpec,
        public maxTime : number = 1000000
    ) {
        super("heat_properties", _spec_)
    };
};

