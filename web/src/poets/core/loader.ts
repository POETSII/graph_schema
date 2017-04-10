/// <reference path="../../../node_modules/@types/xmldom/index.d.ts" />

import {TypedData,TypedDataSpec} from "./typed_data"
import {assert,DeviceType,EdgeType,GraphType,OutputPort,InputPort} from "./types"
import {ReadySet,DeviceInstance,EdgeInstance,GraphInstance} from "./instances"

export interface GraphBuildEvents
{
    //! The device instances within the graph instance will follow
    onBeginDeviceInstances(graphToken:Object) : void;

    //! There will be no more device instances in the graph.
    onEndDeviceInstances(graphToken:Object) : void;

    // Tells the consumer that a new instance is being added
    /*! The return value is a unique token that means something
         to the consumer. */
    onDeviceInstance
    (
        graphToken:Object,
        dt:DeviceType,
        id:string,
        properties:TypedData,
        metadata:Object
    ) : Object;

    //! The edge instances within the graph instance will follow
    onBeginEdgeInstances(graphToken:Object) : void;

    //! There will be no more edge instances in the graph.
    onEndEdgeInstances(graphToken:Object) : void;

    //! Tells the consumer that the a new edge is being added
    /*! It is required that both device instances have already been
        added (otherwise the ids would not been known).
    */
    onEdgeInstance
    (
        graphToken:Object,
        dstDevToken:Object, dstDevType:DeviceType, dstPort:InputPort,
        srcDevToken:Object, srcDevType:DeviceType, srcPort:OutputPort,
        properties:TypedData,
        metadata:Object
    ) : void;

}


export interface GraphLoadEvents
    extends GraphBuildEvents
{
    onDeviceType(deviceType:DeviceType) : void;

    onEdgeType(edgeType:EdgeType) : void;

    onGraphType(graphType:GraphType) : void;

    //! Indicates the graph is starting. Returns a token representing the graph
    onBeginGraphInstance(graphType:GraphType, id:string, properties:TypedData) : Object;

    //! The graph is now complete
    onEndGraphInstance(graphToken:Object) : void;
}

export interface Registry
{
  registerGraphType(graphType:GraphType) : void;
  lookupGraphType(id:string) : GraphType;
};

function find_single(elt:Element, tag:string, ns:string) : Element|null
{
    var src=elt.getElementsByTagNameNS(ns, tag);
    if(src.length>1){
        throw "No graph element";
    }
    if(src.length==0){
        return null;
    }
    for(let e in src){
        return src[e];
    }
}

function get_attribute_required(elt:Element, name:string ) : string
{
    if(!elt.hasAttribute(name)){
        throw "Missing attribute "+name;
    }
    return elt.getAttribute(name);
}

function get_attribute_optional(elt:Element, name:string ) : string|null
{
    if(!elt.hasAttribute(name)){
        return null;
    }
    return elt.getAttribute(name);
}

function import_json(spec:TypedDataSpec,  value:string|null) : TypedData
{
    if(!value){
        return spec.create();
    }

    var json=JSON.parse("{"+value+"}");
    return spec.import(json);
}

function split_path(path:string) : [string,string,string,string]
{
    let endpoints=path.split("-");
    if(endpoints.length!=2)
        throw "Path did not split into two components";
    let [dstDevId,dstPortName]=endpoints[0].split(":");
    let [srcDevId,srcPortName]=endpoints[1].split(":");
    return [dstDevId,dstPortName,srcDevId,srcPortName];
}

export function loadGraphXmlToEvents(registry:Registry, parent:Element, events:GraphLoadEvents) : void
{
    var ns="http://TODO.org/POETS/virtual-graph-schema-v1";

    let eGraph= find_single(parent, "GraphInstance", ns);
    if(!eGraph)
        throw "No graph element.";

    let graphId=get_attribute_required(eGraph, "id");
    let graphTypeId=get_attribute_required(eGraph, "graphTypeId");

    let graphType=lookupGraphType(graphTypeId,registry);

    for(let et in graphType.edgeTypes){
        events.onEdgeType( graphType.edgeTypes[et]);
    }
    for(let dt in graphType.deviceTypes){
        events.onDeviceType(graphType.deviceTypes[dt]);
     }
    events.onGraphType(graphType);

    var graphProperties:TypedData;
    let eProperties=find_single(eGraph, "Properties", ns);
    if(eProperties){
        graphProperties=import_json(graphType.properties, eProperties.textContent);
    }else{
        graphProperties=graphType.properties.create();
    }

    var gId=events.onBeginGraphInstance(graphType, graphId, graphProperties);

    var devices : {[id:string]:[Object,DeviceType];} = {};

    let eDeviceInstances=find_single(eGraph, "DeviceInstances", ns);
    if(!eDeviceInstances)
        throw "No DeviceInstances element";

    events.onBeginDeviceInstances(gId);
  
    var devIs=eDeviceInstances.getElementsByTagNameNS(ns, "DevI");
    for(var i=0; i<devIs.length; i++){
        var eDevice:Element=devIs[i];

        let id=get_attribute_required(eDevice, "id");
        let deviceTypeId=get_attribute_required(eDevice, "type");
    
        var dt=graphType.deviceTypes[deviceTypeId];
        if(!dt){
            throw `Couldn't find a device type called ${deviceTypeId}`;
        }

        var deviceProperties:TypedData;
        let eProperties=find_single(eDevice, "P", ns);
        if(eProperties){
            deviceProperties=import_json(dt.properties, eProperties.textContent);
        }else{
            deviceProperties=dt.properties.create();
        }
        
        var metadata:{ [id:string]:any; } = {};
        let eMetadata=find_single(eDevice, "M", ns);
        if(eMetadata){
            var value=eMetadata.textContent;
            metadata=JSON.parse("{"+value+"}");
        }

        console.log(`  Adding device ${id}, metadata=${metadata}`);
        var dId=events.onDeviceInstance(gId, dt, id, deviceProperties, metadata);

        devices[id]=[dId, dt];
  }

  events.onEndDeviceInstances(gId);

  events.onBeginEdgeInstances(gId);

  var eEdgeInstances= find_single(eGraph, "EdgeInstances", ns);
  if(!eEdgeInstances)
    throw "No EdgeInstances element";

  var edgeIs=eEdgeInstances.getElementsByTagNameNS(ns, "EdgeI");   
  for(var i=0; i<edgeIs.length; i++){
    var eEdge=edgeIs[i];
    
    var srcDeviceId:string, srcPortName:string, dstDeviceId:string, dstPortName:string;
    let path:string=get_attribute_optional(eEdge, "path");
    if(path){
      [dstDeviceId, dstPortName, srcDeviceId, srcPortName]=split_path(path);
    }else{
      srcDeviceId=get_attribute_required(eEdge, "srcDeviceId");
      srcPortName=get_attribute_required(eEdge, "srcPortName");
      dstDeviceId=get_attribute_required(eEdge, "dstDeviceId");
      dstPortName=get_attribute_required(eEdge, "dstPortName");
    }

    let srcDevice=devices[srcDeviceId];
    let dstDevice=devices[dstDeviceId];
    let srcPort=srcDevice[1].getOutput(srcPortName);
    let dstPort=dstDevice[1].getInput(dstPortName);

    if( srcPort.edgeType != dstPort.edgeType )
      throw "Edge type mismatch on ports.";

    let et=srcPort.edgeType;

    var edgeProperties : TypedData|null = null;
    eProperties=find_single(eEdge, "P", ns);
    if(eProperties){
      edgeProperties=import_json(et.properties, eProperties.textContent);
    }else{
      edgeProperties=et.properties.create();
    }

    events.onEdgeInstance(gId,
			   dstDevice[0], dstDevice[1], dstPort,
			   srcDevice[0], srcDevice[1], srcPort,
			   edgeProperties,
               metadata
               );
  }

  events.onBeginEdgeInstances(gId);

  events.onEndGraphInstance(gId);
}


export class GraphBuilder
    implements GraphLoadEvents
{
    g:GraphInstance|null = null;

    onDeviceType(deviceType:DeviceType) : void
    {
        // noop
    }

    onEdgeType(edgeType:EdgeType) : void
    {
        // noop
    }

    onGraphType(graphType:GraphType) : void
    {
        // noop
    }

    //! Indicates the graph is starting. Returns a token representing the graph
    onBeginGraphInstance(graphType:GraphType, id:string, properties:TypedData) : Object
    {
        assert(this.g==null);
        this.g=new GraphInstance(graphType, id, properties);
        return this.g;
    }

    //! The graph is now complete
    onEndGraphInstance(graphToken:Object) : void
    {
        assert(this.g==graphToken);
    }

    //! The device instances within the graph instance will follow
    onBeginDeviceInstances(graphToken:Object) : void
    {
        // noop
    }

    //! There will be no more device instances in the graph.
    onEndDeviceInstances(graphToken:Object) : void
    {
        // noop
    }

    // Tells the consumer that a new instance is being added
    /*! The return value is a unique token that means something
         to the consumer. */
    onDeviceInstance
    (
        graphToken:Object,
        dt:DeviceType,
        id:string,
        properties:TypedData,
        metadata:Object
    ) : Object
    {
        assert(graphToken==this.g);
        return this.g.addDevice(id, dt, properties, metadata);
    }

    //! The edge instances within the graph instance will follow
    onBeginEdgeInstances(graphToken:Object) : void
    {
        // noop
    }

    //! There will be no more edge instances in the graph.
    onEndEdgeInstances(graphToken:Object) : void
    {
        // noop
    }

    //! Tells the consumer that the a new edge is being added
    /*! It is required that both device instances have already been
        added (otherwise the ids would not been known).
    */
    onEdgeInstance
    (
        graphToken:Object,
        dstDevToken:Object, dstDevType:DeviceType, dstPort:InputPort,
        srcDevToken:Object, srcDevType:DeviceType, srcPort:OutputPort,
        properties:TypedData,
        metadata:Object
    ) : void
    {
        assert(graphToken==this.g);
        this.g.addEdgeRaw(dstDevToken as DeviceInstance, dstPort, srcDevToken as DeviceInstance, srcPort, properties, metadata);
    }
}

export function loadGraphFromUrl(url:string,registry:Registry|null=null) : GraphInstance
{
//    var jQuery=require('jquery');

    // TODO : This should be asynchronous with a cancel GUI callback
    // http://stackoverflow.com/a/2592780
    var content:Element;

    jQuery.ajax({
        url: url,
        success: function(data:any) {
            content = data as Element;
        },
        error : function( jqXHR:any, textStatus:string, errorThrown:string ){
            throw `Couldn't load data from ${url} : ${errorThrown}`;
        },
        async:false
    });



    return loadGraphFromXml(content, registry);
}

// http://stackoverflow.com/a/31090240
var isNodeCheck=new Function("try {return this===global;}catch(e){return false;}");
var isNode=isNodeCheck();

export function loadGraphFromString(data:string, registry:Registry|null=null) : GraphInstance
{
    var parser:any;

    if(isNode){
        var xmldom=require("xmldom");

        parser=new xmldom.DOMParser();
    }else{
        parser=new DOMParser();
    }

    console.log("Parsing xml");
    let doc=parser.parseFromString(data, "application/xml");
    console.log("Looking for graphs");
    let graphs=doc.getElementsByTagName("Graphs");
    for(let elt of graphs){
        let builder=new GraphBuilder();

        console.log("Begin loadGraph")
        loadGraphXmlToEvents(registry, elt, builder); 
        console.log("End loadGraph");
        return builder.g;
    }

    throw "No graph instance found in text";
}

export function loadGraphFromXml(data:Element, registry:Registry|null=null) : GraphInstance
{
    let graphs=data.getElementsByTagName("Graphs");
    for(let i=0; i<graphs.length; i++){
        let elt=graphs[i];

        let builder=new GraphBuilder();

        console.log("Begin loadGraph")
        loadGraphXmlToEvents(registry, elt, builder); 
        console.log("End loadGraph");
        return builder.g;
    }

    throw "No graph instance found in text";
}


export class DefaultRegistry
    implements Registry
{
    private _mapping : {[key:string]:GraphType;} = {};

    constructor()
    {
    }

    registerGraphType(graphType:GraphType) : void
    {
          this._mapping[graphType.id]=graphType;
    }

    lookupGraphType(id:string) : GraphType
    {
          return this._mapping[id];
     }
};

var defaultRegistry = new DefaultRegistry();


export function registerGraphType(graphType:GraphType) : void
{
    return defaultRegistry.registerGraphType(graphType);
}

export function lookupGraphType(id:string, registry:Registry|null = null) : GraphType
{
    if(!registry)
        registry=defaultRegistry;
    let res=registry.lookupGraphType(id);
    if(!res)
        throw `lookupGraphType(${id}) - No known graph type with that id.`;
    return res;
}
