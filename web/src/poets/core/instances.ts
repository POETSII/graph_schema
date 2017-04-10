import {TypedData} from "./typed_data"
import {assert,DeviceType,EdgeType,GraphType,OutputPort,InputPort} from "./types"

export type ReadySet = {[key:string]:boolean;};


export class DeviceInstance
{
    constructor(
        public readonly id : string,
        public readonly deviceType : DeviceType,
        public readonly  properties : TypedData =deviceType.properties.create(),
        public state : TypedData = deviceType.state.create(),
        public metadata : {[key:string]:any;} = {}
    )
    {
        for(var i=0;i<deviceType.outputsByIndex.length; i++){
            let k=deviceType.outputsByIndex[i];
            this.rts[k.name]=false;
            this.outputs[k.name]=[];
            this.outputsByIndex.push([]);
        }
        for(var i=0;i<deviceType.inputsByIndex.length; i++){
            let k=deviceType.inputsByIndex[i];
            this.inputs[k.name]=[];
            this.inputsByIndex.push([]);
        }
    }

    rts : ReadySet = {};
    inputs : { [key:string] : EdgeInstance[]; } = {};
    outputs : { [key:string] : EdgeInstance[]; } = {};

    // These are faster to access than by name
    outputsByIndex : EdgeInstance[][] = [];
    inputsByIndex : EdgeInstance[][] = [];

    // Controls the rate for this device. If rate==1.0, then
    // it runs at full rate, if rate==0.0, then nothing happens.
    // The exact semantics are determined by the simulator
    rate : number = 1.0;

    private _is_blocked : boolean = false;
    private _is_rts : boolean = false;

    update() : void
    {
        var _blocked=false;
        var _is_rts=false;
        for( let k in this.rts){
            var lrts=this.rts[k];
            _is_rts = _is_rts || lrts;
            if(lrts){
                for( let e of  this.outputs[k]){
                    let b = e.full();
                    _blocked=_blocked || b;
                }
            }
        }
        this._is_blocked=_blocked;
        this._is_rts=_is_rts;
    }

    update_rts_only() : void
    {
        this._is_rts=false;
        for( let k in this.rts){
            if(this.rts[k]){
                this._is_rts=true;
                return;
            }
        }
    }

    blocked() : boolean
    {
        return this._is_blocked && this._is_rts;
    }

    is_rts() : boolean
    {
        return this._is_rts;
    }
    
};

export class EdgeInstance
{
    constructor(
        public id : string,
        public edgeType : EdgeType,
        public srcDev : DeviceInstance,
        public srcPort : OutputPort,
        public dstDev : DeviceInstance,
        public dstPort : InputPort,
        public properties : TypedData = edgeType.properties.create(),
        public state : TypedData = edgeType.state.create(),
        public metadata = {},
        public queue : TypedData[] = []
    ){};

    private _is_blocked=false;

    full() : boolean
    {
        return this.queue.length>0;
    }

    empty() : boolean
    {
        return this.queue.length==0;
    }

    update() : void
    {
    }
};


export class GraphInstance
{
    private devices : {[key:string]:DeviceInstance} = {};
    private edges : {[key:string]:EdgeInstance} = {};

    private devicesA : DeviceInstance[] = [];
    private edgesA : EdgeInstance[] = [];


    public readonly properties : TypedData;
    
    constructor(
        public readonly graphType:GraphType,
        public id:string,
        public readonly propertiesG: any | null
    ){
        this.properties=graphType.properties.import(propertiesG);
    }
        
    
    getDevice(id : string)
    {
        if(!this.devices.hasOwnProperty(id))
            throw new Error(`No device called ${id}`);
        return this.devices[id];
    }

    enumDevices() : DeviceInstance[]
    {
        return this.devicesA;
    }

    enumEdges() : EdgeInstance[]
    {
        return this.edgesA;
    }
    
    addDevice(
        id:string,
        deviceType:DeviceType,
        propertiesG : any|null = null,
        metadata : {}
    ){
        let properties=deviceType.properties.import(propertiesG);
        
        if(this.devices.hasOwnProperty(id))
            throw new Error("Device already exists.");
        let dev=new DeviceInstance(
            id,
            deviceType,
            properties,
            deviceType.state.create(),
            metadata
        );
        this.devices[id]=dev;
        this.devicesA.push(dev);
        return dev;
    }

    addEdge(
        dstId:string, dstPortName:string,
        srcId:string, srcPortName:string,
        propertiesG : any|null = null,
        metadata  = {}
    ){
        let dstDev=this.getDevice(dstId);
        let dstPort=dstDev.deviceType.getInput(dstPortName);
        let srcDev=this.getDevice(srcId);
        let srcPort=srcDev.deviceType.getOutput(srcPortName);
        
        if(dstPort.edgeType.id!=srcPort.edgeType.id)
            throw new Error(`Edge types don't match : ${dstId}:${dstPortName} has ${dstPort.edgeType.id} vs ${srcId}:${srcPortName} has ${srcPort.edgeType.id}`);
        
        let edgeType=dstPort.edgeType;
        
        let properties = edgeType.properties.import(propertiesG);

        this.addEdgeRaw(dstDev, dstPort, srcDev, srcPort, properties, metadata);
    }


    addEdgeRaw(
        dstDev:DeviceInstance, dstPort:InputPort,
        srcDev:DeviceInstance, srcPort:OutputPort,
        properties : TypedData,
        metadata  = {}
    ){
        let id=`${dstDev.id}:${dstPort.name}-${srcDev.id}:${srcPort.name}`;
        if(this.edges.hasOwnProperty(id))
            throw new Error("Edge already exists.");
        
        let edge=new EdgeInstance(
            id,
            dstPort.edgeType,
            srcDev,
            srcPort,
            dstDev,
            dstPort,
            properties,
            dstPort.edgeType.state.create(),
            metadata
        );
        this.edges[id]=edge;
        this.edgesA.push(edge);
        srcDev.outputs[srcPort.name].push(edge);
        srcDev.outputsByIndex[srcPort.index].push(edge);
        dstDev.inputs[dstPort.name].push(edge);
        dstDev.inputsByIndex[dstPort.index].push(edge);
    }
};

