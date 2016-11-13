declare function require(name: string) : any;
//require('source-map-support').install();

export function assert(cond : boolean)
{
    console.assert(cond);
}

// http://stackoverflow.com/a/3826081
export function get_type(thing : any){
    if(thing===null)return "[object Null]"; // special case
    return Object.prototype.toString.call(thing);
}

export interface TypedData
{
    readonly _name_ : string;

    [x:string]:any;
};

class EmptyTypedData
    implements TypedData
{
    readonly _name_ = "empty";

};


export interface TypedDataSpec
{
    create : { () : TypedData; };
    import(data : any|null) : TypedData;

};

export class EmptyTypedDataSpec
    implements TypedDataSpec
{
    private static instance : TypedData = new EmptyTypedData();
    
    create () : TypedData {
        return EmptyTypedDataSpec.instance;
    }
    import(data : any|null) : TypedData
    {
        if(data!=null){
            for(let k in data){
                throw new Error(`Empty data should havve no properties, but got ${k}`);
            }
        }
        return EmptyTypedDataSpec.instance;
    }
};


export class GenericTypedDataSpec<T extends TypedData>
    implements TypedDataSpec
{
    constructor(
        // https://github.com/Microsoft/TypeScript/issues/2794
        private readonly newT : { new(...args : any[]) : T; } 
    ) {
    };
    
    create () : TypedData {
        return new this.newT();
    };

    // This will work for both another typed data instance, or a general object
    import(data : any|null) : TypedData
    {
        var r = new this.newT();
        if(data !=null){
            //for(var k of data.getOwnPropertyNames()){
            for(let k in data){
                if(!r.hasOwnProperty(k)){
                    throw new Error(`Object ${r._name_} does not have property called ${k}`);
                }
                
                r[k]=data[k];
            }
        }
        return r;
    }
};

export class EdgeType
{
    constructor(
        public readonly id : string,
        public readonly message : TypedDataSpec = new EmptyTypedDataSpec(),
        public readonly properties : TypedDataSpec = new EmptyTypedDataSpec(),
        public state : TypedDataSpec = new EmptyTypedDataSpec()
    ) {};
};

export abstract class InputPort
{
    constructor(
        public readonly name : string,
        public readonly edgeType : EdgeType
    ){}
    
    abstract onReceive(
        graphProperties : TypedData,
        deviceProperties : TypedData,
        deviceState : TypedData,
        edgeProperties : TypedData,
        edgeState : TypedData,
        message : TypedData,
        rts : { [key:string] : boolean ;}
    ) : void;
};

export abstract class OutputPort
{
    constructor(
        public readonly name : string,
        public readonly edgeType : EdgeType
    ){}
    
    abstract onSend (
        graphProperties : TypedData,
        deviceProperties : TypedData,
        deviceState : TypedData,
        message : TypedData,
        rts : { [key:string] : boolean; }
    ) : boolean;
};

export class DeviceType
{
    inputs : {[key:string]:InputPort} = {};
    outputs : {[key:string]:OutputPort} = {};
    
    constructor(
        public readonly id:string,
        public readonly properties:TypedDataSpec,
        public readonly state:TypedDataSpec,
        inputs : InputPort[] = [],
        outputs : OutputPort[] = []
    ){
        for(let i of inputs){
            console.log(`  adding ${i.name}, edgeType=${i.edgeType.id}`);
            this.inputs[i.name]=i;
        }
        for(let o of outputs){
            console.log(`  adding ${o.name}`);
            this.outputs[o.name]=o;
        }
    }
    
    
    getInput(name:string) : InputPort
    {
        if(!this.inputs.hasOwnProperty(name))
            throw new Error(`No input called ${name}`);
        return this.inputs[name];
    }
    
    getOutput(name:string) : OutputPort
    {
        if(!this.outputs.hasOwnProperty(name))
            throw new Error(`No output called ${name}`);
        return this.outputs[name];
    }
};


export class GraphType
{
    readonly deviceTypes : { [key:string]:DeviceType; } = {};

    constructor(
        public readonly id : string,
        public readonly properties : TypedDataSpec,
        _deviceTypes : DeviceType[]
    ){
        for(let d of _deviceTypes){
            this.deviceTypes[d.id]=d;
        }
    }

};

type ReadySet = {[key:string]:boolean;};

function clone(s : ReadySet) : ReadySet
{
    let res : ReadySet ={};
    for(let k in s){
        res[k]=s[k];
    }
    return res;
}

export class DeviceInstance
{
    constructor(
        public readonly id : string,
        public readonly deviceType : DeviceType,
        public readonly  properties : TypedData =deviceType.properties.create(),
        public state : TypedData = deviceType.state.create(),
        public metadata = {}
    )
    {
        for(let k in deviceType.outputs){
            this.rts[k]=false;
            this.outputs[k]=[];
        }
        for(let k in deviceType.inputs){
            this.inputs[k]=[];
        }
    }

    rts : ReadySet = {};
    inputs : { [key:string] : EdgeInstance[]; } = {};
    outputs : { [key:string] : EdgeInstance[]; } = {};

    blocked() : boolean
    {
        for( let k in this.rts){
            if(this.rts[k])
                return false;
        }
        return true;
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
        public metadata = {}
    ){};
};

export class GraphInstance
{
    private devices : {[key:string]:DeviceInstance} = {};
    private edges : {[key:string]:EdgeInstance} = {};

    public readonly properties : TypedData;
    
    constructor(
        public readonly graphType:GraphType,
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
        var res:DeviceInstance[]=[];
        for(let k in this.devices){
            res.push(this.devices[k]);
        }
        return res;
    }

    enumEdges() : EdgeInstance[]
    {
        var res:EdgeInstance[]=[];
        for(let k in this.edges){
            res.push(this.edges[k]);
        }
        return res;
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
        this.devices[id]=new DeviceInstance(
            id,
            deviceType,
            properties,
            deviceType.state.create(),
            metadata
        );
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
        
        let id=dstId+":"+dstPort+"-"+srcId+":"+srcPort;
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
            edgeType.state.create(),
            metadata
        );
        this.edges[id]=edge;
        srcDev.outputs[srcPort.name].push(edge);
        dstDev.inputs[dstPort.name].push(edge);
    }
};

enum EventType
{
    Send,
    Receive,
    Init
}

abstract class Event
{
    eventType : EventType;
    applied : boolean = true;

    abstract swap() : void;

    apply() : void
    {
        assert(!this.applied);
        this.swap();
        this.applied=true;
    }

    unapply() : void
    {
        assert(this.applied);
        this.swap();
        this.applied=false;
    }
};

class SendEvent
    extends Event
{
    readonly eventType = EventType.Send;

    constructor(
        public readonly device : DeviceInstance,
        public state : TypedData,
        public rts : ReadySet,
        public readonly message : TypedData,
        public readonly cancelled : boolean
    ){
        super();
    }

    swap() : void
    {
        assert(!this.applied);
        var tmp1=this.device.state;
        this.device.state=this.state;
        this.state=tmp1;
        var tmp2=this.device.rts;
        this.device.rts=this.rts;
        this.rts=tmp2;
    }
};

class InitEvent
    extends Event
{
    readonly eventType = EventType.Init;

    constructor(
        public readonly device : DeviceInstance,
        public state : TypedData,
        public rts : ReadySet,
        public readonly message : TypedData
    ){
        super();
    }

    swap() : void
    {
        assert(!this.applied);
        var tmp1=this.device.state;
        this.device.state=this.state;
        this.state=tmp1;
        var tmp2=this.device.rts;
        this.device.rts=this.rts;
        this.rts=tmp2;
    }   
 
};

class ReceiveEvent
    extends Event
{
    readonly eventType = EventType.Receive;

    constructor(
        public readonly edge : EdgeInstance,
        public state : TypedData,
        public rts : ReadySet,
        public readonly message : TypedData
    ){
        super();
    }

    swap() : void
    {
        assert(!this.applied);
        var tmp1=this.edge.dstDev.state;
        this.edge.dstDev.state=this.state;
        this.state=tmp1;
        var tmp2=this.edge.dstDev.rts;
        this.edge.dstDev.rts=this.rts;
        this.rts=tmp2;
    }   
 
};

export interface Stepper
{
    step() : Event[]
}

export class SingleStepper
    implements Stepper
{
    ready : DeviceInstance[] = [];

    history : Event[] = [];

    constructor(
        public g : GraphInstance
    ){
        for(let d of g.enumDevices()){
            if("__init__" in d.deviceType.inputs){
                let port=d.deviceType.inputs["__init__"];
                let message=port.edgeType.message.create();
                
                let preState=d.deviceType.state.import(d.state);
                let preRts=clone(d.rts);
                port.onReceive(
                    this.g.properties,
                    d.properties,
                    d.state,
                    port.edgeType.properties.create(),
                    port.edgeType.state.create(),
                    message,
                    d.rts
                );
                this.history.push(new InitEvent(d, preState, preRts, message));
            }

            if(!d.blocked()){
                this.ready.push(d);
            }
        }
    }

    step() : Event[]
    {
        let res:Event[]=[];

        if(this.ready.length==0)
            return res;

        let selDev=Math.floor(Math.random()*this.ready.length);
        let dev=this.ready[selDev];
        this.ready.splice(selDev,1);

        let rts=dev.rts;
        let rtsPorts:string[]=[];
        for(let k in rts){
            if(rts[k]){
                rtsPorts.push(k);
            }
        }

        let selPort=Math.floor(Math.random()*rtsPorts.length);
        let port=dev.deviceType.getOutput(rtsPorts[selPort]);

        let message=port.edgeType.message.create();

        let preState=dev.deviceType.state.import(dev.state)
        let preRts=clone(dev.rts);

        let doSend=port.onSend(
            this.g.properties,
            dev.properties,
            dev.state,
            message,
            dev.rts
        );
        res.push(new SendEvent(dev, preState, preRts, message, !doSend));
        console.log(` send to ${dev.id} : state'=${JSON.stringify(dev.state)}, rts'=${JSON.stringify(dev.rts)}`);

        if(doSend){
            for(let e of dev.outputs[port.name]){
                let preState=e.dstDev.deviceType.state.import(e.dstDev.state);
                let preRts=clone(e.dstDev.rts);
                e.dstPort.onReceive(
                    this.g.properties,
                    e.dstDev.properties,
                    e.dstDev.state,
                    e.properties,
                    e.state,
                    message,
                    e.dstDev.rts
                );
                res.push(new ReceiveEvent(e, preState, preRts, message));

                console.log(` recv on ${e.dstDev.id} : state'=${JSON.stringify(e.dstDev.state)}, rts'=${JSON.stringify(e.dstDev.rts)}`);

                let ii=this.ready.indexOf(e.dstDev);
                if(e.dstDev.blocked()){
                    if(ii!=-1){
                        this.ready.splice(ii,1);
                    }
                }else{
                    if(ii==-1){
                        this.ready.push(e.dstDev);
                    }
                }
            }
        }

        if(!dev.blocked()){
            this.ready.push(dev);
        }

        this.history=this.history.concat(res);

        return res;
    }
}
