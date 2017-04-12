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
    static elements = [
        tInt("nhood"),tInt("dt",1),tFloat("iv"),tFloat("wSelf")
     ];

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
        tInt("ns"), tFloat("na"),
        tBoolean("force")
    ];

    constructor(
        _spec_ : TypedDataSpec,
        public v : number = 0 ,
        public t : number = 0,
        public cs : number = 0,
        public ca : number = 0,
        public ns : number = 0,
        public na : number = 0,
        public force : boolean = false
    ){
        super("cell_state", _spec_);
    };
};

class CellInitInputPort
    extends InputPort
{
    private rts_flag_out:number;

    constructor()
    {
        super("__init__", initEdgeType);
    }

    onBindDevice(parent:DeviceType)
    {
        this.rts_flag_out=parent.getOutput("out").rts_flag;
    }   
    
    onReceive(
        graphPropertiesG : TypedData,
        devicePropertiesG : TypedData,
        deviceStateG : TypedData,
        edgePropertiesG : TypedData,
        edgeStateG : TypedData,
        messageG : TypedData
    ) : number
     {
        let deviceProperties=devicePropertiesG as CellDeviceProperties;
        let deviceState=deviceStateG as CellDeviceState;

        deviceState.v = deviceProperties.iv; 
        deviceState.t = 0;
        deviceState.cs = deviceProperties.nhood; // Force us into the sending ready state
        deviceState.ca = deviceProperties.iv;   // This is the first value
        deviceState.ns = 0;
        deviceState.na = 0;

        return deviceState.cs==deviceProperties.nhood ? this.rts_flag_out : 0;
    }
};

class CellInInputPort
    extends InputPort
{
    private rts_flag_out : number;

    constructor()
    {
        super("in", updateEdgeType);
    }

    onBindDevice(parent:DeviceType)
    {
        this.rts_flag_out=parent.getOutput("out").rts_flag;
    }   
    
    onReceive(
        graphPropertiesG : TypedData,
        devicePropertiesG : TypedData,
        deviceStateG : TypedData,
        edgePropertiesG : TypedData,
        edgeStateG : TypedData,
        messageG : TypedData
    ) : number
    {
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
        return deviceState.cs == deviceProperties.nhood ? this.rts_flag_out : 0;
    }
};

class CellOutOutputPort
    extends OutputPort
{
    private rts_flags_out : number;

    constructor()
    {
        super("out", updateEdgeType);
    }   

    onBindDevice(parent:DeviceType)
    {
        this.rts_flags_out=parent.getOutput("out").rts_flag;
    }
    
    onSend(
        graphPropertiesG : TypedData,
        devicePropertiesG : TypedData,
        deviceStateG : TypedData,
        messageG : TypedData
    ) : [boolean,number]
    {
        let graphProperties=graphPropertiesG as HeatGraphProperties;
        let deviceProperties=devicePropertiesG as CellDeviceProperties;
        let deviceState=deviceStateG as CellDeviceState;
        let message=messageG as UpdateMessage;

        var doSend:boolean=false;
        var rts:number=0;
        
        if(deviceState.t > graphProperties.maxTime){
            // do nothing
        }else{
            if(!deviceState.force){
                deviceState.v=deviceState.ca;
            }
            deviceState.t += deviceProperties.dt;
            deviceState.cs = deviceState.ns;
            deviceState.ca = deviceState.na + deviceProperties.wSelf * deviceState.v;
            deviceState.ns=0;
            deviceState.na=0;

            message.t = deviceState.t;
            message.v=deviceState.v;
            
            rts =  deviceState.cs == deviceProperties.nhood ? this.rts_flags_out : 0;
            doSend=true;
        }
        return [doSend,rts];
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

