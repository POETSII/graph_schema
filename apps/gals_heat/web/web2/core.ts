declare function require(name: string) : any;
//require('source-map-support').install();

export function assert(cond : boolean)
{
    if(!cond){
        console.assert(false);
    }
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
            //console.log(`  adding ${i.name}, edgeType=${i.edgeType.id}`);
            this.inputs[i.name]=i;
        }
        for(let o of outputs){
            //console.log(`  adding ${o.name}`);
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
        return this.devicesA;
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
        this.devicesA.push(this.devices[id]);
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
        public readonly port : OutputPort,
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

export function get_event_list_updated_nodes_and_edges(
    events : Event[]
) : [ DeviceInstance[], EdgeInstance[] ]
{
    var devices : {[key:string]:DeviceInstance; } = {};
    var edges : {[key:string]:EdgeInstance; } = {};
    for(let ev of events)
    {
        if(ev instanceof InitEvent){
            devices[ev.device.id]=ev.device;
        }else if(ev instanceof SendEvent){
            devices[ev.device.id]=ev.device;
            for(let e of ev.device.outputs[ev.port.name]){
                edges[e.id]=e;
                devices[e.dstDev.id]=e.dstDev;
            }
        }else if(ev instanceof ReceiveEvent){
            devices[ev.edge.srcDev.id]=ev.edge.srcDev;
            devices[ev.edge.dstDev.id]=ev.edge.dstDev;
            edges[ev.edge.id]=ev.edge;
        }else{
            assert(false);
        }
    }

    var aDevices:DeviceInstance[]=[];
    var aEdges:EdgeInstance[]=[];
    for(var d in devices){
        aDevices.push(devices[d]);
    }
    for(var e in edges){
        aEdges.push(edges[e]);
    }
    return [aDevices,aEdges];
}

export interface Stepper
{
    attach(g:GraphInstance, doInit:boolean) : void;
    detach() : GraphInstance;

    step() : [number,Event[]];
}

export class SingleStepper
    implements Stepper
{
    readyDevs : DeviceInstance[] = [];
    readyEdges : EdgeInstance[] = [];

    history : Event[] = [];

    g : GraphInstance|null = null;

    attach(
        graph : GraphInstance,
        doInit: boolean
    ){
        assert(this.g==null);
        this.g=graph;

        for(let d of this.g.enumDevices()){
            if(doInit && ("__init__" in d.deviceType.inputs)){
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

            this.update_dev(d);
        }
        for(let e of this.g.enumEdges()){
            this.update_edge(e);
        }
    }

    detach() : GraphInstance
    {
        assert(this.g!=null);
        var res=this.g;

        this.history=[];
        this.readyDevs=[];
        this.readyEdges=[];
        this.g=null;

        return res;
    }

    private update_dev(dev:DeviceInstance) : void
    {
        dev.update();
        let ii=this.readyDevs.indexOf(dev);
        if(!dev.is_rts() || dev.blocked()){
            if(ii!=-1){
                this.readyDevs.splice(ii,1);
            }
        }else{
            if(ii==-1){
                this.readyDevs.push(dev);
            }
        }
    }

    private update_edge(edge:EdgeInstance) : void
    {
        edge.update();
        let ii=this.readyEdges.indexOf(edge);
        if(edge.empty()){
            if(ii!=-1){
                this.readyEdges.splice(ii,1);
            }
        }else{
            if(ii==-1){
                this.readyEdges.push(edge);
            }
        }
    }

    step() : [number,Event[]]
    {
        let res:Event[]=[];

        let readyAll=this.readyEdges.length+this.readyDevs.length;

        //console.log(`readyEdges : ${this.readyEdges.length}, readyDevs : ${this.readyDevs.length}`);

        if(readyAll==0)
            return [0,res];

        let sel=Math.floor(Math.random()*readyAll);

        if(sel < this.readyDevs.length){
            let selDev=sel;
            let dev=this.readyDevs[selDev];
            this.readyDevs.splice(selDev,1);

            assert(dev.is_rts());

            let rts=dev.rts;
            let rtsPorts:string[]=[];
            for(let k in rts){
                if(rts[k]){
                    rtsPorts.push(k);
                }
            }

            assert(rtsPorts.length>0);

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
            res.push(new SendEvent(dev, port, preState, preRts, message, !doSend));
            //console.log(` send to ${dev.id} : state'=${JSON.stringify(dev.state)}, rts'=${JSON.stringify(dev.rts)}`);

            if(doSend){
                for(let e of dev.outputs[port.name]){
                    e.queue.push(message);
                    this.update_edge(e);
                }
            }

            this.update_dev(dev);

        }else{   
            let selEdge=sel-this.readyDevs.length;
            let e= this.readyEdges[selEdge];
            this.readyEdges.splice(selEdge, 1);

            let message=e.queue[0];
            e.queue.splice(0,1);
            
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

            //console.log(`   eprops = ${JSON.stringify(e.properties)}`);
            //console.log(`   recv on ${e.dstDev.id} : state'=${JSON.stringify(e.dstDev.state)}, rts'=${JSON.stringify(e.dstDev.rts)}`);

            this.update_dev(e.dstDev);
            this.update_dev(e.srcDev);
        }

        this.history=this.history.concat(res);

        return [res.length,res];
    }
}


export class BatchStepper
    implements Stepper
{
    g : GraphInstance|null = null;

    history : Event[] = [];

    attach(
        graph : GraphInstance,
        doInit: boolean
    ){
        assert(this.g==null);
        this.g=graph;

        for(let d of this.g.enumDevices()){
            if(doInit && ("__init__" in d.deviceType.inputs)){
                let port=d.deviceType.inputs["__init__"];
                let message=port.edgeType.message.create();
                
                port.onReceive(
                    this.g.properties,
                    d.properties,
                    d.state,
                    port.edgeType.properties.create(),
                    port.edgeType.state.create(),
                    message,
                    d.rts
                );
            }
            d.update();
        }
        for(let e of this.g.enumEdges()){
            while(e.queue.length>0){
                var h=e.queue.pop();

                let message=e.queue[0];
                e.queue.splice(0,1);
            
                e.dstPort.onReceive(
                    this.g.properties,
                    e.dstDev.properties,
                    e.dstDev.state,
                    e.properties,
                    e.state,
                    message,
                    e.dstDev.rts
                );
            }
            e.update();
            e.srcDev.update();
            e.dstDev.update();
        }
    }

    detach() : GraphInstance
    {
        assert(this.g!=null);
        var res=this.g;

        this.g=null;

        return res;
    }

    step() : [number,Event[]]
    {
        var count=0;

        for(let dev of this.g.enumDevices()){
            if(!dev.is_rts())
                continue;

            let rts=dev.rts;
            let rtsPorts:string[]=[];
            for(let k in rts){
                if(rts[k]){
                    rtsPorts.push(k);
                }
            }

            assert(rtsPorts.length>0);

            let selPort=Math.floor(Math.random()*rtsPorts.length);
            let port=dev.deviceType.getOutput(rtsPorts[selPort]);

            let message=port.edgeType.message.create();

            let doSend=port.onSend(
                this.g.properties,
                dev.properties,
                dev.state,
                message,
                dev.rts
            );
            ++count;
            if(doSend){
                for(let e of dev.outputs[port.name]){
                
                    e.dstPort.onReceive(
                        this.g.properties,
                        e.dstDev.properties,
                        e.dstDev.state,
                        e.properties,
                        e.state,
                        message,
                        e.dstDev.rts
                    );
                    e.dstDev.update_rts_only();
                    ++count;
                }
            }
            dev.update_rts_only();
        }
        return [count,[]];
    }
}
