var __extends = (this && this.__extends) || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
};
function assert(cond) {
    console.assert(cond);
}
;
var EmptyTypedData = (function () {
    function EmptyTypedData() {
    }
    return EmptyTypedData;
}());
;
;
var EmptyTypedDataSpec = (function () {
    function EmptyTypedDataSpec() {
    }
    EmptyTypedDataSpec.prototype.create = function () {
        return EmptyTypedDataSpec.instance;
    };
    EmptyTypedDataSpec.instance = new EmptyTypedData();
    return EmptyTypedDataSpec;
}());
;
var GenericTypedDataSpec = (function () {
    function GenericTypedDataSpec(
        // https://github.com/Microsoft/TypeScript/issues/2794
        newT) {
        this.newT = newT;
    }
    ;
    GenericTypedDataSpec.prototype.create = function () {
        return new this.newT();
    };
    ;
    return GenericTypedDataSpec;
}());
;
var EdgeType = (function () {
    function EdgeType(id, properties, state) {
        if (properties === void 0) { properties = new EmptyTypedDataSpec(); }
        if (state === void 0) { state = new EmptyTypedDataSpec(); }
        this.id = id;
        this.properties = properties;
        this.state = state;
    }
    ;
    return EdgeType;
}());
;
var InputPort = (function () {
    function InputPort(name, edgeType) {
        this.name = name;
        this.edgeType = edgeType;
    }
    return InputPort;
}());
;
var OutputPort = (function () {
    function OutputPort(name, edgeType) {
        this.name = name;
        this.edgeType = edgeType;
    }
    return OutputPort;
}());
;
var DeviceType = (function () {
    function DeviceType(id, properties, state, inputs, outputs) {
        this.id = id;
        this.properties = properties;
        this.state = state;
        this.inputs = {};
        this.outputs = {};
        for (var _i = 0, inputs_1 = inputs; _i < inputs_1.length; _i++) {
            var i = inputs_1[_i];
            console.log("  adding " + i.name + ", edgeType=" + i.edgeType.id);
            this.inputs[i.name] = i;
        }
        for (var _a = 0, outputs_1 = outputs; _a < outputs_1.length; _a++) {
            var o = outputs_1[_a];
            console.log("  adding " + o.name);
            this.outputs[o.name] = o;
        }
    }
    DeviceType.prototype.getInput = function (name) {
        return this.inputs[name];
    };
    DeviceType.prototype.getOutput = function (name) {
        return this.outputs[name];
    };
    return DeviceType;
}());
;
var UpdateMessage = (function () {
    function UpdateMessage(source) {
        this.source = source;
        this._type_ = "update";
    }
    ;
    return UpdateMessage;
}());
;
var initEdgeType = new EdgeType("__init__");
var updateEdgeType = new EdgeType("update", new GenericTypedDataSpec(UpdateMessage));
var DirichletDeviceProperties = (function () {
    function DirichletDeviceProperties() {
    }
    return DirichletDeviceProperties;
}());
;
var DirichletDeviceState = (function () {
    function DirichletDeviceState() {
    }
    return DirichletDeviceState;
}());
;
var DirichletInitInputPort = (function (_super) {
    __extends(DirichletInitInputPort, _super);
    function DirichletInitInputPort() {
        _super.call(this, "__init__", initEdgeType);
    }
    DirichletInitInputPort.prototype.onReceive = function (graphPropertiesG, devicePropertiesG, deviceStateG, edgePropertiesG, edgeStateG, messageG, rts) {
        var deviceProperties = devicePropertiesG;
        var deviceState = deviceStateG;
        deviceState.t = 0;
        deviceState.cs = deviceProperties.neighbours; // Force us into the sending ready state
        deviceState.ns = 0;
        deviceState.v = deviceProperties.bias + deviceProperties.amplitude
            * Math.sin(deviceProperties.phase + deviceProperties.frequency * deviceState.t);
        rts["out"] = deviceState.cs == deviceProperties.neighbours;
    };
    return DirichletInitInputPort;
}(InputPort));
;
var DirichletInInputPort = (function (_super) {
    __extends(DirichletInInputPort, _super);
    function DirichletInInputPort() {
        _super.call(this, "in", updateEdgeType);
    }
    DirichletInInputPort.prototype.onReceive = function (graphPropertiesG, devicePropertiesG, deviceStateG, edgePropertiesG, edgeStateG, messageG, rts) {
        var deviceProperties = devicePropertiesG;
        var deviceState = deviceStateG;
        var message = messageG;
        if (message.t == deviceState.t) {
            deviceState.cs++;
        }
        else {
            assert(message.t == deviceState.t + 1);
            deviceState.ns++;
        }
        rts["out"] = deviceState.cs == deviceProperties.neighbours;
    };
    return DirichletInInputPort;
}(InputPort));
;
var DirichletOutOutputPort = (function (_super) {
    __extends(DirichletOutOutputPort, _super);
    function DirichletOutOutputPort() {
        _super.call(this, "out", updateEdgeType);
    }
    DirichletOutOutputPort.prototype.onSend = function (graphPropertiesG, devicePropertiesG, deviceStateG, messageG, rts) {
        var graphProperties = graphPropertiesG;
        var deviceProperties = devicePropertiesG;
        var deviceState = deviceStateG;
        var message = messageG;
        if (deviceState.t > graphProperties.maxTime) {
            rts["out"] = false;
            return false;
        }
        else {
            deviceState.v = deviceProperties.bias + deviceProperties.amplitude
                * Math.sin(deviceProperties.phase + deviceProperties.frequency * deviceState.t);
            deviceState.t++;
            deviceState.cs = deviceState.ns;
            deviceState.ns = 0;
            message.t = deviceState.t + 1;
            message.v = deviceState.v;
            rts["out"] = deviceState.cs == deviceProperties.neighbours;
            return true;
        }
    };
    return DirichletOutOutputPort;
}(OutputPort));
;
var dirichletDeviceType = new DeviceType("dirichlet", new GenericTypedDataSpec(DirichletDeviceProperties), new GenericTypedDataSpec(DirichletDeviceState), [
    new DirichletInitInputPort(),
    new DirichletInInputPort()
], [
    new DirichletOutOutputPort()
]);
var HeatGraphProperties = (function () {
    function HeatGraphProperties() {
    }
    return HeatGraphProperties;
}());
;
var GraphType = (function () {
    function GraphType() {
    }
    return GraphType;
}());
;
var heatGraphType = {
    "id": "gals_heat",
    "properties": new GenericTypedDataSpec(HeatGraphProperties),
    "deviceTypes": [
        dirichletDeviceType
    ]
};
var DeviceInstance = (function () {
    function DeviceInstance() {
    }
    return DeviceInstance;
}());
;
var EdgeInstance = (function () {
    function EdgeInstance(id, edgeType, srcDev, srcPort, dstDev, dstPort, properties, state) {
        this.id = id;
        this.edgeType = edgeType;
        this.srcDev = srcDev;
        this.srcPort = srcPort;
        this.dstDev = dstDev;
        this.dstPort = dstPort;
        this.properties = properties;
        this.state = state;
    }
    return EdgeInstance;
}());
;
var GraphInstance = (function () {
    function GraphInstance(graphType) {
        this.graphType = graphType;
        this.devices = {};
        this.edges = {};
    }
    GraphInstance.prototype.getDevice = function (id) {
        return this.devices[id];
    };
    GraphInstance.prototype.addDevice = function (id, deviceType, properties) {
        if (properties === void 0) { properties = null; }
        properties = properties || deviceType.properties.create();
        if (this.devices.hasOwnProperty(id))
            throw new Error("Device already exists.");
        this.devices[id] = {
            id: id,
            deviceType: deviceType,
            state: deviceType.state.create(),
            properties: properties
        };
    };
    GraphInstance.prototype.addEdge = function (dstId, dstPortName, srcId, srcPortName, properties) {
        if (properties === void 0) { properties = null; }
        var dstDev = this.getDevice(dstId);
        var dstPort = dstDev.deviceType.getInput(dstPortName);
        var srcDev = this.getDevice(srcId);
        var srcPort = srcDev.deviceType.getOutput(srcPortName);
        if (dstPort.edgeType.id != srcPort.edgeType.id)
            throw new Error("Edge types don't match : " + dstId + ":" + dstPortName + " has " + dstPort.edgeType.id + " vs " + srcId + ":" + srcPortName + " has " + srcPort.edgeType.id);
        var edgeType = dstPort.edgeType;
        properties = properties || edgeType.properties.create();
        var id = dstId + ":" + dstPort + "-" + srcId + ":" + srcPort;
        if (this.edges.hasOwnProperty(id))
            throw new Error("Edge already exists.");
        this.edges[id] = new EdgeInstance(id, dstPort.edgeType, srcDev, srcPort, dstDev, dstPort, properties, edgeType.state.create());
    };
    return GraphInstance;
}());
;
var g = new GraphInstance(heatGraphType);
g.addDevice("d0", dirichletDeviceType);
g.addDevice("d1", dirichletDeviceType);
g.addEdge("d0", "in", "d1", "out");
