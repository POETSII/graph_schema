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
                console.log(` ${k} = ${r[k]}`);
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


export class DeviceInstance
{
    constructor(
        public readonly id : string,
        public readonly deviceType : DeviceType,
        public readonly  properties : TypedData =deviceType.properties.create(),
        public state : TypedData = deviceType.state.create(),
        public metadata = {}
    ) {};
    
    inputs : { InputPort : [DeviceInstance,OutputPort] };
    outputs : { OutputPort : [DeviceInstance,InputPort] };
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
    )
    {}
};

export class GraphInstance
{
    private devices : {[key:string]:DeviceInstance} = {};
    private edges : {[key:string]:EdgeInstance} = {};
    
    constructor(
        public readonly graphType:GraphType
    ){}
        
    
    getDevice(id : string)
    {
        if(!this.devices.hasOwnProperty(id))
            throw new Error(`No device called ${id}`);
        return this.devices[id];
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
            deviceType.state.create(),
            properties,
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
        
        this.edges[id]=new EdgeInstance(
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
    }
};
