/// <reference path="heat_types.ts" />

import * as POETS from "../core/core"

import {tFloat,tInt,tBoolean} from "../core/typed_data"

import {HeatGraphProperties} from "./heat_types"
import {initEdgeType,updateEdgeType,UpdateEdgeProperties,UpdateMessage} from "./heat_types"

import assert = POETS.assert;
import TypedData = POETS.TypedData;
import EdgeType = POETS.EdgeType;
import DeviceType = POETS.DeviceType;
import GraphType = POETS.GraphType;
import TypedDataSpec = POETS.TypedDataSpec;
import GenericTypedDataSpec = POETS.GenericTypedDataSpec;
import InputPort = POETS.InputPort;
import OutputPort = POETS.OutputPort;



class CellDeviceProperties
    extends TypedData
{
    static elements = [tInt("nhood"),tInt("dt"),tFloat("iv"),tFloat("wSelf")];

    constructor(
        _spec_ : TypedDataSpec,
        public nhood : number = 0,
        public dt : number = 1,
        public iv : number = 0,
        public wSelf : number = 0
    ){
        super("cell_properties", _spec_);
    };
};

class CellDeviceState
    extends TypedData
{
    static elements = [
        tFloat("v"), tInt("t"),
        tInt("cs"), tFloat("ca"),
        tInt("ns"), tFloat("na")
    ];

    constructor(
        _spec_ : TypedDataSpec,
        public v : number = 0 ,
        public t : number = 0,
        public cs : number = 0,
        public ca : number = 0,
        public ns : number = 0,
        public na : number = 0
    ){
        super("cell_state", _spec_);
    };
};

class CellInitInputPort
    extends InputPort
{
    constructor()
    {
        super("__init__", initEdgeType);
    }   
    
    onReceive(
        graphPropertiesG : TypedData,
        devicePropertiesG : TypedData,
        deviceStateG : TypedData,
        edgePropertiesG : TypedData,
        edgeStateG : TypedData,
        messageG : TypedData,
        rts : { [key:string] : boolean; }
    ) : void
     {
        let deviceProperties=devicePropertiesG as CellDeviceProperties;
        let deviceState=deviceStateG as CellDeviceState;

        deviceState.v = deviceProperties.iv; 
        deviceState.t = 0;
        deviceState.cs = deviceProperties.nhood; // Force us into the sending ready state
        deviceState.ca = deviceProperties.iv;   // This is the first value
        deviceState.ns = 0;
        deviceState.na = 0;

        rts["out"] = deviceState.cs==deviceProperties.nhood;
    }
};

class CellInInputPort
    extends InputPort
{
    constructor()
    {
        super("in", updateEdgeType);
    }
    
    onReceive(
        graphPropertiesG : TypedData,
        devicePropertiesG : TypedData,
        deviceStateG : TypedData,
        edgePropertiesG : TypedData,
        edgeStateG : TypedData,
        messageG : TypedData,
        rts : { [key:string] : boolean; }
    ){
        let deviceProperties=devicePropertiesG as CellDeviceProperties;
        let deviceState=deviceStateG as CellDeviceState;
        let edgeProperties=edgePropertiesG as UpdateEdgeProperties;
        let message=messageG as UpdateMessage;

        //console.log("  w = "+edgeProperties.w);
        
        if(message.t==deviceState.t){
            deviceState.cs++;
            deviceState.ca += edgeProperties.w * message.v;
        }else{
            assert( message.t == deviceState.t+1 );
            deviceState.ns++;
            deviceState.na += edgeProperties.w * message.v;
        }
        rts["out"] = deviceState.cs == deviceProperties.nhood;
    }
};

class CellOutOutputPort
    extends OutputPort
{
    constructor()
    {
        super("out", updateEdgeType);
    }   
    
    onSend(
        graphPropertiesG : TypedData,
        devicePropertiesG : TypedData,
        deviceStateG : TypedData,
        messageG : TypedData,
        rts : { [key:string] : boolean; }
    ) : boolean
    {
        let graphProperties=graphPropertiesG as HeatGraphProperties;
        let deviceProperties=devicePropertiesG as CellDeviceProperties;
        let deviceState=deviceStateG as CellDeviceState;
        let message=messageG as UpdateMessage;
        
        if(deviceState.t > graphProperties.maxTime){
            rts["out"]=false;
            return false;
        }else{
            deviceState.v=deviceState.ca;
            deviceState.t += deviceProperties.dt;
            deviceState.cs = deviceState.ns;
            deviceState.ca = deviceState.na + deviceProperties.wSelf * deviceState.v;
            deviceState.ns=0;
            deviceState.na=0;

            message.t = deviceState.t;
            message.v=deviceState.v;
            
            rts["out"] = deviceState.cs == deviceProperties.nhood;
            return true;
        }
    }
};


export const cellDeviceType = new DeviceType(
    "cell",
    new GenericTypedDataSpec(CellDeviceProperties, CellDeviceProperties.elements),
    new GenericTypedDataSpec(CellDeviceState, CellDeviceState.elements),
    [
        new CellInitInputPort(),
        new CellInInputPort()
    ],
    [
        new CellOutOutputPort()
    ]
);

