
function assert(cond)
{
    console.assert(cond);
}

interface TypedData
{
    [x: string]: any ;
};

class EmptyTypedData
    implements TypedData
{};


interface TypedDataSpec
{
    create : { () : TypedData; };
};

class EmptyTypedDataSpec
    implements TypedDataSpec
{
    private static instance : TypedData = new EmptyTypedData();
    
    create () : TypedData {
        return EmptyTypedDataSpec.instance;
    }
};


class GenericTypedDataSpec<T extends TypedData>
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
};

class EdgeType
{
    constructor(
        public readonly id : string,
        public readonly properties : TypedDataSpec = new EmptyTypedDataSpec(),
        public readonly state : TypedDataSpec = new EmptyTypedDataSpec()
    ) {};
};

abstract class InputPort
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
        rts : { string : boolean }
    );
};

abstract class OutputPort
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
        rts : { string : boolean }
    ) : boolean;
};

class DeviceType
{
    inputs : {[key:string]:InputPort} = {};
    outputs : {[key:string]:OutputPort} = {};
    
    constructor(
        public readonly id:string,
        public readonly properties:TypedDataSpec,
        public readonly state:TypedDataSpec,
        inputs : InputPort[],
        outputs : OutputPort[]
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
        return this.inputs[name];
    }
    
    getOutput(name:string) : OutputPort
    {
        return this.outputs[name];
    }
};

class UpdateMessage
    implements TypedData
{
    constructor(
        readonly source : string
    ) {};
    
    readonly _type_ : string = "update";
    
    t : number;
    v : number;
};


const initEdgeType = new EdgeType("__init__");

const updateEdgeType = new EdgeType("update", new GenericTypedDataSpec(UpdateMessage));




class DirichletDeviceProperties
    implements TypedData
{
    neighbours : number;
    amplitude : number;
    phase : number;
    frequency : number;
    bias : number;
};

class DirichletDeviceState
    implements TypedData
{
    v : number;
    t : number;
    cs : number;
    ns : number;
};

class DirichletInitInputPort
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
        rts : { string : boolean }
    ){
        let deviceProperties=devicePropertiesG as DirichletDeviceProperties;
        let deviceState=deviceStateG as DirichletDeviceState;
        
        deviceState.t =0;
        deviceState.cs = deviceProperties.neighbours; // Force us into the sending ready state
        deviceState.ns = 0;

        deviceState.v=deviceProperties.bias + deviceProperties.amplitude
            * Math.sin(deviceProperties.phase + deviceProperties.frequency * deviceState.t);

        rts["out"] = deviceState.cs==deviceProperties.neighbours;
    }
};

class DirichletInInputPort
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
        rts : { string : boolean }
    ){
        let deviceProperties=devicePropertiesG as DirichletDeviceProperties;
        let deviceState=deviceStateG as DirichletDeviceState;
        let message=messageG as UpdateMessage;
        
        if(message.t==deviceState.t){
            deviceState.cs++;
        }else{
            assert( message.t == deviceState.t+1 );
            deviceState.ns++;
        }
        rts["out"] = deviceState.cs == deviceProperties.neighbours;
    }
};

class DirichletOutOutputPort
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
        rts : { string : boolean }
    ) : boolean
    {
        let graphProperties=graphPropertiesG as HeatGraphProperties;
        let deviceProperties=devicePropertiesG as DirichletDeviceProperties;
        let deviceState=deviceStateG as DirichletDeviceState;
        let message=messageG as UpdateMessage;
        
        if(deviceState.t > graphProperties.maxTime){
            rts["out"]=false;
            return false;
        }else{
            deviceState.v=deviceProperties.bias + deviceProperties.amplitude
                * Math.sin(deviceProperties.phase + deviceProperties.frequency * deviceState.t);
            deviceState.t++;
            deviceState.cs = deviceState.ns;
            deviceState.ns=0;

            message.t = deviceState.t+1;
            message.v=deviceState.v;
            
            rts["out"] = deviceState.cs == deviceProperties.neighbours;
            return true;
        }
    }
};


const dirichletDeviceType = new DeviceType(
    "dirichlet",
    new GenericTypedDataSpec(DirichletDeviceProperties),
    new GenericTypedDataSpec(DirichletDeviceState),
    [
        new DirichletInitInputPort(),
        new DirichletInInputPort()
    ],
    [
        new DirichletOutOutputPort()
    ]
);


class HeatGraphProperties
    implements TypedData
{
    maxTime : number;
};

class GraphType
{
    id : string;
    properties : TypedDataSpec;
    deviceTypes : DeviceType[];
};

const heatGraphType = <GraphType>{
    "id" : "gals_heat",
    "properties" : new GenericTypedDataSpec(HeatGraphProperties),
    "deviceTypes" : [
        dirichletDeviceType
    ]
};


class DeviceInstance
{
    id : string;
    deviceType : DeviceType;
    state : TypedDataSpec;
    properties : TypedDataSpec;
    inputs : { InputPort : [DeviceInstance,OutputPort] };
    outputs : { OutputPort : [DeviceInstance,InputPort] };
};

class EdgeInstance
{
    constructor(
        public id : string,
        public edgeType : EdgeType,
        public srcDev : DeviceInstance,
        public srcPort : OutputPort,
        public dstDev : DeviceInstance,
        public dstPort : InputPort,
        public properties : TypedData,
        public state : TypedData
    )
    {}
};

class GraphInstance
{
    private devices : {[key:string]:DeviceInstance} = {};
    private edges : {[key:string]:EdgeInstance} = {};
    
    constructor(
        public readonly graphType:GraphType
    ){}
        
    
    getDevice(id : string)
    {
        return this.devices[id];
    }
    
    addDevice(
        id:string,
        deviceType:DeviceType,
        properties : TypedData|null = null
    ){
        properties = properties || deviceType.properties.create();
        
        if(this.devices.hasOwnProperty(id))
            throw new Error("Device already exists.");
        this.devices[id]=<DeviceInstance>{
            id:id,
            deviceType:deviceType,
            state:deviceType.state.create(),
            properties:properties
        };
    }
    
    addEdge(
        dstId:string, dstPortName:string,
        srcId:string, srcPortName:string,
        properties : TypedData|null = null
    ){
        let dstDev=this.getDevice(dstId);
        let dstPort=dstDev.deviceType.getInput(dstPortName);
        let srcDev=this.getDevice(srcId);
        let srcPort=srcDev.deviceType.getOutput(srcPortName);
        
        if(dstPort.edgeType.id!=srcPort.edgeType.id)
            throw new Error(`Edge types don't match : ${dstId}:${dstPortName} has ${dstPort.edgeType.id} vs ${srcId}:${srcPortName} has ${srcPort.edgeType.id}`);
        
        let edgeType=dstPort.edgeType;
        
        properties = properties || edgeType.properties.create();
        
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
            edgeType.state.create()
        );
    }
};

var g = new GraphInstance(heatGraphType);
g.addDevice("d0", dirichletDeviceType);
g.addDevice("d1", dirichletDeviceType);
g.addEdge("d0", "in", "d1", "out");
