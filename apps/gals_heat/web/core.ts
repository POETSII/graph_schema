
enum DataClass
{
  Message=0,
  Properties=1,
  State=2
};

enum TypeClass
{
  Graph=0,
  Device=1,
  Edge=2
};

interface Endpoint
{
    readonly device : string;
    readonly port : string;
};

interface TypedData
{
  readonly _type_id_    : string; // id of the device type, edge type, or graph type it is associated with
  readonly _data_class_ : DataClass;
  readonly _type_class_ : TypeClass;
};


abstract class Message
    implements TypedData
{
    constructor(
        public _type_id_ : string,
        public _source_ : Endpoint
    ){}
    
    readonly _data_class_ = DataClass.Message;
    readonly _type_class_ = TypeClass.Edge;
};

abstract class GraphProperties
    implements TypedData
{
    constructor(
        public _type_id_ : string
    ){}
    
  readonly _data_class_ = DataClass.Properties;
  readonly _type_class_ = TypeClass.Graph;
};

abstract class DeviceProperties
    implements TypedData
{
    constructor(
        public _type_id_ : string
    ){}
    
  readonly _data_class_ = DataClass.Properties;
  readonly _type_class_ = TypeClass.Device;
};

abstract class EdgeProperties
    implements TypedData
{
    constructor(
        public _type_id_ : string
    ){}
    
  readonly _data_class_ = DataClass.Properties;
  readonly _type_class_ = TypeClass.Edge;
};

abstract class DeviceState
    implements TypedData
{
    constructor(
        public _type_id_ : string
    ){}
    
  readonly _data_class_ = DataClass.State;
  readonly _type_class_ = TypeClass.Device;
};

abstract class EdgeState
    implements TypedData
{
    constructor(
        public _type_id_ : string
    ){}
    
  readonly _data_class_ = DataClass.State;
  readonly _type_class_ = TypeClass.Edge;
};


interface GraphInstance
{
    readonly devices : { string : DeviceInstance };
    readonly edges : [ EdgeInstance ];
    
};

interface DeviceInstance
{
    readonly graph      : GraphInstance;
    readonly properties : DeviceProperties;
    state               : DeviceState;
};

interface EdgeInstance
{
    readonly source     : DeviceInstance;
    readonly dest       : DeviceInstance;
    readonly properties : EdgeProperties;
    state               : EdgeState;
    pending             : Message[];
};

abstract class EdgeType
{
    messageType : 
};

interface DeviceType
{
    inputs : { string : string };   //
    outputs : { string : string };
    
    sendHandlers : { string : SendHandler };
    receiveHandlers : { string : SendHandler };
};

interface GraphType
{
    deviceTypes : { string : DeviceType };
};

interface Handler
{
    readonly device_type : string;
    readonly port_name : string; 
    readonly direction : "send" | "receive";
};

interface ReceiveHandler
    extends Handler
{
    readonly direction : "receive";
    
    (
        graphProperties     : GraphProperties,  // in
        deviceProperties    : DeviceProperties, // in
        deviceState         : DeviceState,      // in/out
        edgeProperties      : EdgeProperties,   // in
        edgeState           : EdgeState,        // in/out
        message             : Message,          // in
        readyToSend         : {string: boolean} // in/out
    ) : void;
};

interface SendHandler
    extends Handler
{
    readonly direction : "send";
    
    (
        graphProperties     : GraphProperties,  // in
        deviceProperties    : DeviceProperties, // in
        deviceState         : DeviceState,      // in/out
        message             : Message,          // out
        readyToSend         : {string: boolean} // in/out
    ) : void;
};


class GraphStepper
{
    graph : GraphInstance;
    ready : [DeviceInstance,string][]; // Array of ports and 
    
};


class UpdateEdgeMessage
    extends Message
{
    constructor(
        source : Endpoint,
        public time : number =0,
        public heat : number =0.0
    )
    {
        super("update", source);
    }
};

class UpdateEdgeType
{
    messageType    : any = typeof(UpdateEdgeMessage);
    propertiesType : any = typeof(EdgeProperties);
    stateType      : any = typeof(EdgeState);
};


