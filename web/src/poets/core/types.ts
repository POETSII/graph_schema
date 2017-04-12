declare function require(name: string) : any;
//require('source-map-support').install();

import {TypedData,TypedDataSpec,EmptyTypedDataSpec} from "./typed_data"


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

    index : number = -1;

    setIndex(_index:number) : void
    {
        assert(this.index==-1);
        this.index = _index;
    }

    abstract onBindDevice(parent:DeviceType) : void;
    
    abstract onReceive(
        graphProperties : TypedData,
        deviceProperties : TypedData,
        deviceState : TypedData,
        edgeProperties : TypedData,
        edgeState : TypedData,
        message : TypedData
    ) : number;
};

export abstract class OutputPort
{
    constructor(
        public readonly name : string,
        public readonly edgeType : EdgeType
    ){}

    index : number = -1;
    rts_flag : number = 0;

    setIndex(_index:number) : void
    {
        assert(this.index==-1);
        this.index = _index;
        this.rts_flag = 1<<_index;
    }

    abstract onBindDevice(parent:DeviceType) : void;
    
    abstract onSend (
        graphProperties : TypedData,
        deviceProperties : TypedData,
        deviceState : TypedData,
        message : TypedData
    ) : [boolean,number]; // Return a pair of doSend and new rts
};

export class DeviceType
{
    inputs : {[key:string]:InputPort} = {};
    inputsByIndex : InputPort[] = [];
    outputs : {[key:string]:OutputPort} = {};
    outputsByIndex : OutputPort[] = [];
    
    constructor(
        public readonly id:string,
        public readonly properties:TypedDataSpec,
        public readonly state:TypedDataSpec,
        inputs : InputPort[] = [],
        outputs : OutputPort[] = [],
        public readonly outputCount : number = outputs.length
    ){
        var numInputs=0;
        for(let i of inputs){
            //console.log(`  adding ${i.name}, edgeType=${i.edgeType.id}`);
            i.setIndex(numInputs);
            this.inputs[i.name]=i;
            this.inputsByIndex.push(i);
            numInputs++;
            
        }
        var numOutputs=0;
        for(let o of outputs){
            //console.log(`  adding ${o.name}`);
            o.setIndex(numOutputs);
            this.outputs[o.name]=o;
            this.outputsByIndex.push(o);
            numOutputs++;
        }

        for(let i of inputs){
            i.onBindDevice(this);
        }
        for(let o of outputs){
            o.onBindDevice(this);
        }
    }
    
    
    getInput(name:string) : InputPort
    {
        if(!this.inputs.hasOwnProperty(name))
            throw new Error(`No input called ${name}`);
        return this.inputs[name];
    }

    getInputByIndex(index:number) : InputPort
    {
        return this.inputsByIndex[index];
    }
    
    getOutput(name:string) : OutputPort
    {
        if(!this.outputs.hasOwnProperty(name))
            throw new Error(`No output called ${name}`);
        return this.outputs[name];
    }

    getOutputByIndex(index:number) : OutputPort
    {
        return this.outputsByIndex[index];
    }
};


export class GraphType
{
    readonly deviceTypes : { [key:string]:DeviceType; } = {};
    readonly edgeTypes : { [key:string]:EdgeType; } = {};

    constructor(
        public readonly id : string,
        public readonly properties : TypedDataSpec,
        _deviceTypes : DeviceType[]
    ){
        for(let d of _deviceTypes){
            this.deviceTypes[d.id]=d;
            for(let eId in d.inputs){
                if( ! (eId in this.edgeTypes ) ){
                    this.edgeTypes[eId] = d.inputs[eId].edgeType;
                }
            }
            for(let eId in d.outputs){
                if( ! (eId in this.edgeTypes ) ){
                    this.edgeTypes[eId] = d.outputs[eId].edgeType;
                }
            }

        }
    }

};
