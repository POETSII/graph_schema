import {TypedData} from "./typed_data"
import {assert,DeviceType,EdgeType,GraphType,OutputPort,InputPort} from "./types"
import {ReadySet,DeviceInstance,EdgeInstance,GraphInstance} from "./instances"

function clone(s : ReadySet) : ReadySet
{
    let res : ReadySet ={};
    for(let k in s){
        res[k]=s[k];
    }
    return res;
}

enum EventType
{
    Send,
    Receive,
    Init,
    Skip
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

class SkipEvent
    extends Event
{
    readonly eventType = EventType.Skip;

    constructor(
        public readonly device : DeviceInstance
    ){
        super();
    }

    swap() : void
    {
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

            if(dev.rate != 1.0 && dev.rate < Math.random()){
                res.push(new SkipEvent(dev));
                return [1,res ];
            }

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

        let devByIndex=this.g.enumDevices();
        let offset=0 | Math.floor(Math.random() * devByIndex.length);
        for(let i=0; i<devByIndex.length; i++){
            let dev=devByIndex[ (i+offset)%devByIndex.length ];

            if(!dev.is_rts())
                continue;

            if(dev.rate!=1.0 && dev.rate < Math.random()){
                ++count;
                continue;
            }

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
                //let outgoing=dev.outputs[port.name];
                let outgoing=dev.outputsByIndex[port.index];
                for(let e of outgoing){
                
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
