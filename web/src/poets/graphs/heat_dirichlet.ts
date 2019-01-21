/// <reference path="heat_types.ts" />

import * as POETS from "../core/core"
import {HeatGraphProperties} from "./heat_types"
import {UpdateMessage,initEdgeType,updateEdgeType} from "./heat_types"

import {tFloat,tInt,tBoolean} from "../core/typed_data"

import assert = POETS.assert;
import TypedData = POETS.TypedData;
import EdgeType = POETS.EdgeType;
import DeviceType = POETS.DeviceType;
import GraphType = POETS.GraphType;
import GenericTypedDataSpec = POETS.GenericTypedDataSpec;
import TypedDataSpec = POETS.TypedDataSpec;
import InputPort = POETS.InputPort;
import OutputPort = POETS.OutputPort;


class DirichletDeviceProperties
    extends TypedData
{
    static elements = [
        tInt("dt",1), tInt("neighbours"),
        tFloat("amplitude",1.0),
        tFloat("phase",0.5),
        tFloat("frequency",1.0),
        tFloat("bias")
    ];

    constructor(
        _spec_:TypedDataSpec,
        public dt : number = 1,
        public neighbours : number = 0,
        public amplitude : number = 1,
        public phase : number = 0.5,
        public frequency : number = 1,
        public bias : number = 0
    ){
        super("dirichlet_properties", _spec_);
    }
};

class DirichletDeviceState
    extends TypedData
{
    static elements = [
        tFloat("v"), tInt("t"),
        tFloat("cs"), tInt("ns")
    ];

    constructor(
        _spec_:TypedDataSpec,
        public v : number =0,
        public t : number =0,
        public cs : number =0,
        public ns : number =0
    ){
        super("dirichlet_state", _spec_);
    };
};

class DirichletInitInputPort
    extends InputPort
{
    constructor()
    {
        super("__init__", initEdgeType);
    }   

    private rts_flag_out : number=0;

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
        let deviceProperties=devicePropertiesG as DirichletDeviceProperties;
        let deviceState=deviceStateG as DirichletDeviceState;
        
        deviceState.t =0;
        deviceState.cs = deviceProperties.neighbours; // Force us into the sending ready state
        deviceState.ns = 0;

        deviceState.v=deviceProperties.bias + deviceProperties.amplitude
            * Math.sin(deviceProperties.phase + deviceProperties.frequency * deviceState.t);

        return deviceState.cs==deviceProperties.neighbours ? this.rts_flag_out : 0;
    }
};

class DirichletInInputPort
    extends InputPort
{
    constructor()
    {
        super("in", updateEdgeType);
    }

    private rts_flag_out : number=0;

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
        let deviceProperties=devicePropertiesG as DirichletDeviceProperties;
        let deviceState=deviceStateG as DirichletDeviceState;
        let message=messageG as UpdateMessage;
        
        if(message.t==deviceState.t){
            deviceState.cs++;
        }else{
            assert( message.t == deviceState.t+deviceProperties.dt );
            deviceState.ns++;
        }
        return deviceState.cs == deviceProperties.neighbours ? this.rts_flag_out : 0;
    }
};

class DirichletOutOutputPort
    extends OutputPort
{
    constructor()
    {
        super("out", updateEdgeType);
    }   

    private rts_flag_out : number=0;

    onBindDevice(parent:DeviceType)
    {
        this.rts_flag_out=parent.getOutput("out").rts_flag;
    }   
    
    onSend(
        graphPropertiesG : TypedData,
        devicePropertiesG : TypedData,
        deviceStateG : TypedData,
        messageG : TypedData
    ) : [boolean,number]
    {
        let graphProperties=graphPropertiesG as HeatGraphProperties;
        let deviceProperties=devicePropertiesG as DirichletDeviceProperties;
        let deviceState=deviceStateG as DirichletDeviceState;
        let message=messageG as UpdateMessage;
        
        var doSend=false;
        var rts=0;

        if(deviceState.t > graphProperties.maxTime){
            // do nothing
        }else{
            deviceState.v=deviceProperties.bias + deviceProperties.amplitude
                * Math.sin(deviceProperties.phase + deviceProperties.frequency * deviceState.t);
            deviceState.t+=deviceProperties.dt;
            deviceState.cs = deviceState.ns;
            deviceState.ns=0;

            message.t = deviceState.t;
            message.v=deviceState.v;
            
            rts= deviceState.cs == deviceProperties.neighbours ? this.rts_flag_out : 0;
            doSend=true;
        }
        return [doSend,rts];
    }
};


export const dirichletDeviceType = new DeviceType(
    "dirichlet_variable",
    new GenericTypedDataSpec(DirichletDeviceProperties, DirichletDeviceProperties.elements),
    new GenericTypedDataSpec(DirichletDeviceState, DirichletDeviceState.elements),
    [
        new DirichletInitInputPort(),
        new DirichletInInputPort()
    ],
    [
        new DirichletOutOutputPort()
    ]
);

