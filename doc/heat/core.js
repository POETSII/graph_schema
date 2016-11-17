var __extends = (this && this.__extends) || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
};
define(["require", "exports"], function (require, exports) {
    "use strict";
    //require('source-map-support').install();
    function assert(cond) {
        if (!cond) {
            console.assert(false);
        }
    }
    exports.assert = assert;
    // http://stackoverflow.com/a/3826081
    function get_type(thing) {
        if (thing === null)
            return "[object Null]"; // special case
        return Object.prototype.toString.call(thing);
    }
    exports.get_type = get_type;
    ;
    var EmptyTypedData = (function () {
        function EmptyTypedData() {
            this._name_ = "empty";
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
        EmptyTypedDataSpec.prototype.import = function (data) {
            if (data != null) {
                for (var k in data) {
                    throw new Error("Empty data should havve no properties, but got " + k);
                }
            }
            return EmptyTypedDataSpec.instance;
        };
        EmptyTypedDataSpec.instance = new EmptyTypedData();
        return EmptyTypedDataSpec;
    }());
    exports.EmptyTypedDataSpec = EmptyTypedDataSpec;
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
        // This will work for both another typed data instance, or a general object
        GenericTypedDataSpec.prototype.import = function (data) {
            var r = new this.newT();
            if (data != null) {
                //for(var k of data.getOwnPropertyNames()){
                for (var k in data) {
                    if (!r.hasOwnProperty(k)) {
                        throw new Error("Object " + r._name_ + " does not have property called " + k);
                    }
                    r[k] = data[k];
                }
            }
            return r;
        };
        return GenericTypedDataSpec;
    }());
    exports.GenericTypedDataSpec = GenericTypedDataSpec;
    ;
    var EdgeType = (function () {
        function EdgeType(id, message, properties, state) {
            if (message === void 0) { message = new EmptyTypedDataSpec(); }
            if (properties === void 0) { properties = new EmptyTypedDataSpec(); }
            if (state === void 0) { state = new EmptyTypedDataSpec(); }
            this.id = id;
            this.message = message;
            this.properties = properties;
            this.state = state;
        }
        ;
        return EdgeType;
    }());
    exports.EdgeType = EdgeType;
    ;
    var InputPort = (function () {
        function InputPort(name, edgeType) {
            this.name = name;
            this.edgeType = edgeType;
        }
        return InputPort;
    }());
    exports.InputPort = InputPort;
    ;
    var OutputPort = (function () {
        function OutputPort(name, edgeType) {
            this.name = name;
            this.edgeType = edgeType;
        }
        return OutputPort;
    }());
    exports.OutputPort = OutputPort;
    ;
    var DeviceType = (function () {
        function DeviceType(id, properties, state, inputs, outputs) {
            if (inputs === void 0) { inputs = []; }
            if (outputs === void 0) { outputs = []; }
            this.id = id;
            this.properties = properties;
            this.state = state;
            this.inputs = {};
            this.outputs = {};
            for (var _i = 0, inputs_1 = inputs; _i < inputs_1.length; _i++) {
                var i = inputs_1[_i];
                //console.log(`  adding ${i.name}, edgeType=${i.edgeType.id}`);
                this.inputs[i.name] = i;
            }
            for (var _a = 0, outputs_1 = outputs; _a < outputs_1.length; _a++) {
                var o = outputs_1[_a];
                //console.log(`  adding ${o.name}`);
                this.outputs[o.name] = o;
            }
        }
        DeviceType.prototype.getInput = function (name) {
            if (!this.inputs.hasOwnProperty(name))
                throw new Error("No input called " + name);
            return this.inputs[name];
        };
        DeviceType.prototype.getOutput = function (name) {
            if (!this.outputs.hasOwnProperty(name))
                throw new Error("No output called " + name);
            return this.outputs[name];
        };
        return DeviceType;
    }());
    exports.DeviceType = DeviceType;
    ;
    var GraphType = (function () {
        function GraphType(id, properties, _deviceTypes) {
            this.id = id;
            this.properties = properties;
            this.deviceTypes = {};
            for (var _i = 0, _deviceTypes_1 = _deviceTypes; _i < _deviceTypes_1.length; _i++) {
                var d = _deviceTypes_1[_i];
                this.deviceTypes[d.id] = d;
            }
        }
        return GraphType;
    }());
    exports.GraphType = GraphType;
    ;
    function clone(s) {
        var res = {};
        for (var k in s) {
            res[k] = s[k];
        }
        return res;
    }
    var DeviceInstance = (function () {
        function DeviceInstance(id, deviceType, properties, state, metadata) {
            if (properties === void 0) { properties = deviceType.properties.create(); }
            if (state === void 0) { state = deviceType.state.create(); }
            if (metadata === void 0) { metadata = {}; }
            this.id = id;
            this.deviceType = deviceType;
            this.properties = properties;
            this.state = state;
            this.metadata = metadata;
            this.rts = {};
            this.inputs = {};
            this.outputs = {};
            this._is_blocked = false;
            this._is_rts = false;
            for (var k in deviceType.outputs) {
                this.rts[k] = false;
                this.outputs[k] = [];
            }
            for (var k in deviceType.inputs) {
                this.inputs[k] = [];
            }
        }
        DeviceInstance.prototype.update = function () {
            var _blocked = false;
            var _is_rts = false;
            for (var k in this.rts) {
                var lrts = this.rts[k];
                _is_rts = _is_rts || lrts;
                if (lrts) {
                    for (var _i = 0, _a = this.outputs[k]; _i < _a.length; _i++) {
                        var e = _a[_i];
                        var b = e.full();
                        _blocked = _blocked || b;
                    }
                }
            }
            this._is_blocked = _blocked;
            this._is_rts = _is_rts;
        };
        DeviceInstance.prototype.update_rts_only = function () {
            this._is_rts = false;
            for (var k in this.rts) {
                if (this.rts[k]) {
                    this._is_rts = true;
                    return;
                }
            }
        };
        DeviceInstance.prototype.blocked = function () {
            return this._is_blocked && this._is_rts;
        };
        DeviceInstance.prototype.is_rts = function () {
            return this._is_rts;
        };
        return DeviceInstance;
    }());
    exports.DeviceInstance = DeviceInstance;
    ;
    var EdgeInstance = (function () {
        function EdgeInstance(id, edgeType, srcDev, srcPort, dstDev, dstPort, properties, state, metadata, queue) {
            if (properties === void 0) { properties = edgeType.properties.create(); }
            if (state === void 0) { state = edgeType.state.create(); }
            if (metadata === void 0) { metadata = {}; }
            if (queue === void 0) { queue = []; }
            this.id = id;
            this.edgeType = edgeType;
            this.srcDev = srcDev;
            this.srcPort = srcPort;
            this.dstDev = dstDev;
            this.dstPort = dstPort;
            this.properties = properties;
            this.state = state;
            this.metadata = metadata;
            this.queue = queue;
            this._is_blocked = false;
        }
        ;
        EdgeInstance.prototype.full = function () {
            return this.queue.length > 0;
        };
        EdgeInstance.prototype.empty = function () {
            return this.queue.length == 0;
        };
        EdgeInstance.prototype.update = function () {
        };
        return EdgeInstance;
    }());
    exports.EdgeInstance = EdgeInstance;
    ;
    var GraphInstance = (function () {
        function GraphInstance(graphType, propertiesG) {
            this.graphType = graphType;
            this.propertiesG = propertiesG;
            this.devices = {};
            this.edges = {};
            this.devicesA = [];
            this.properties = graphType.properties.import(propertiesG);
        }
        GraphInstance.prototype.getDevice = function (id) {
            if (!this.devices.hasOwnProperty(id))
                throw new Error("No device called " + id);
            return this.devices[id];
        };
        GraphInstance.prototype.enumDevices = function () {
            return this.devicesA;
        };
        GraphInstance.prototype.enumEdges = function () {
            var res = [];
            for (var k in this.edges) {
                res.push(this.edges[k]);
            }
            return res;
        };
        GraphInstance.prototype.addDevice = function (id, deviceType, propertiesG, metadata) {
            if (propertiesG === void 0) { propertiesG = null; }
            var properties = deviceType.properties.import(propertiesG);
            if (this.devices.hasOwnProperty(id))
                throw new Error("Device already exists.");
            this.devices[id] = new DeviceInstance(id, deviceType, properties, deviceType.state.create(), metadata);
            this.devicesA.push(this.devices[id]);
        };
        GraphInstance.prototype.addEdge = function (dstId, dstPortName, srcId, srcPortName, propertiesG, metadata) {
            if (propertiesG === void 0) { propertiesG = null; }
            if (metadata === void 0) { metadata = {}; }
            var dstDev = this.getDevice(dstId);
            var dstPort = dstDev.deviceType.getInput(dstPortName);
            var srcDev = this.getDevice(srcId);
            var srcPort = srcDev.deviceType.getOutput(srcPortName);
            if (dstPort.edgeType.id != srcPort.edgeType.id)
                throw new Error("Edge types don't match : " + dstId + ":" + dstPortName + " has " + dstPort.edgeType.id + " vs " + srcId + ":" + srcPortName + " has " + srcPort.edgeType.id);
            var edgeType = dstPort.edgeType;
            var properties = edgeType.properties.import(propertiesG);
            var id = dstId + ":" + dstPort + "-" + srcId + ":" + srcPort;
            if (this.edges.hasOwnProperty(id))
                throw new Error("Edge already exists.");
            var edge = new EdgeInstance(id, dstPort.edgeType, srcDev, srcPort, dstDev, dstPort, properties, edgeType.state.create(), metadata);
            this.edges[id] = edge;
            srcDev.outputs[srcPort.name].push(edge);
            dstDev.inputs[dstPort.name].push(edge);
        };
        return GraphInstance;
    }());
    exports.GraphInstance = GraphInstance;
    ;
    var EventType;
    (function (EventType) {
        EventType[EventType["Send"] = 0] = "Send";
        EventType[EventType["Receive"] = 1] = "Receive";
        EventType[EventType["Init"] = 2] = "Init";
    })(EventType || (EventType = {}));
    var Event = (function () {
        function Event() {
            this.applied = true;
        }
        Event.prototype.apply = function () {
            assert(!this.applied);
            this.swap();
            this.applied = true;
        };
        Event.prototype.unapply = function () {
            assert(this.applied);
            this.swap();
            this.applied = false;
        };
        return Event;
    }());
    ;
    var SendEvent = (function (_super) {
        __extends(SendEvent, _super);
        function SendEvent(device, port, state, rts, message, cancelled) {
            _super.call(this);
            this.device = device;
            this.port = port;
            this.state = state;
            this.rts = rts;
            this.message = message;
            this.cancelled = cancelled;
            this.eventType = EventType.Send;
        }
        SendEvent.prototype.swap = function () {
            assert(!this.applied);
            var tmp1 = this.device.state;
            this.device.state = this.state;
            this.state = tmp1;
            var tmp2 = this.device.rts;
            this.device.rts = this.rts;
            this.rts = tmp2;
        };
        return SendEvent;
    }(Event));
    ;
    var InitEvent = (function (_super) {
        __extends(InitEvent, _super);
        function InitEvent(device, state, rts, message) {
            _super.call(this);
            this.device = device;
            this.state = state;
            this.rts = rts;
            this.message = message;
            this.eventType = EventType.Init;
        }
        InitEvent.prototype.swap = function () {
            assert(!this.applied);
            var tmp1 = this.device.state;
            this.device.state = this.state;
            this.state = tmp1;
            var tmp2 = this.device.rts;
            this.device.rts = this.rts;
            this.rts = tmp2;
        };
        return InitEvent;
    }(Event));
    ;
    var ReceiveEvent = (function (_super) {
        __extends(ReceiveEvent, _super);
        function ReceiveEvent(edge, state, rts, message) {
            _super.call(this);
            this.edge = edge;
            this.state = state;
            this.rts = rts;
            this.message = message;
            this.eventType = EventType.Receive;
        }
        ReceiveEvent.prototype.swap = function () {
            assert(!this.applied);
            var tmp1 = this.edge.dstDev.state;
            this.edge.dstDev.state = this.state;
            this.state = tmp1;
            var tmp2 = this.edge.dstDev.rts;
            this.edge.dstDev.rts = this.rts;
            this.rts = tmp2;
        };
        return ReceiveEvent;
    }(Event));
    ;
    function get_event_list_updated_nodes_and_edges(events) {
        var devices = {};
        var edges = {};
        for (var _i = 0, events_1 = events; _i < events_1.length; _i++) {
            var ev = events_1[_i];
            if (ev instanceof InitEvent) {
                devices[ev.device.id] = ev.device;
            }
            else if (ev instanceof SendEvent) {
                devices[ev.device.id] = ev.device;
                for (var _a = 0, _b = ev.device.outputs[ev.port.name]; _a < _b.length; _a++) {
                    var e_1 = _b[_a];
                    edges[e_1.id] = e_1;
                    devices[e_1.dstDev.id] = e_1.dstDev;
                }
            }
            else if (ev instanceof ReceiveEvent) {
                devices[ev.edge.srcDev.id] = ev.edge.srcDev;
                devices[ev.edge.dstDev.id] = ev.edge.dstDev;
                edges[ev.edge.id] = ev.edge;
            }
            else {
                assert(false);
            }
        }
        var aDevices = [];
        var aEdges = [];
        for (var d in devices) {
            aDevices.push(devices[d]);
        }
        for (var e in edges) {
            aEdges.push(edges[e]);
        }
        return [aDevices, aEdges];
    }
    exports.get_event_list_updated_nodes_and_edges = get_event_list_updated_nodes_and_edges;
    var SingleStepper = (function () {
        function SingleStepper() {
            this.readyDevs = [];
            this.readyEdges = [];
            this.history = [];
            this.g = null;
        }
        SingleStepper.prototype.attach = function (graph, doInit) {
            assert(this.g == null);
            this.g = graph;
            for (var _i = 0, _a = this.g.enumDevices(); _i < _a.length; _i++) {
                var d = _a[_i];
                if (doInit && ("__init__" in d.deviceType.inputs)) {
                    var port = d.deviceType.inputs["__init__"];
                    var message = port.edgeType.message.create();
                    var preState = d.deviceType.state.import(d.state);
                    var preRts = clone(d.rts);
                    port.onReceive(this.g.properties, d.properties, d.state, port.edgeType.properties.create(), port.edgeType.state.create(), message, d.rts);
                    this.history.push(new InitEvent(d, preState, preRts, message));
                }
                this.update_dev(d);
            }
            for (var _b = 0, _c = this.g.enumEdges(); _b < _c.length; _b++) {
                var e = _c[_b];
                this.update_edge(e);
            }
        };
        SingleStepper.prototype.detach = function () {
            assert(this.g != null);
            var res = this.g;
            this.history = [];
            this.readyDevs = [];
            this.readyEdges = [];
            this.g = null;
            return res;
        };
        SingleStepper.prototype.update_dev = function (dev) {
            dev.update();
            var ii = this.readyDevs.indexOf(dev);
            if (!dev.is_rts() || dev.blocked()) {
                if (ii != -1) {
                    this.readyDevs.splice(ii, 1);
                }
            }
            else {
                if (ii == -1) {
                    this.readyDevs.push(dev);
                }
            }
        };
        SingleStepper.prototype.update_edge = function (edge) {
            edge.update();
            var ii = this.readyEdges.indexOf(edge);
            if (edge.empty()) {
                if (ii != -1) {
                    this.readyEdges.splice(ii, 1);
                }
            }
            else {
                if (ii == -1) {
                    this.readyEdges.push(edge);
                }
            }
        };
        SingleStepper.prototype.step = function () {
            var res = [];
            var readyAll = this.readyEdges.length + this.readyDevs.length;
            //console.log(`readyEdges : ${this.readyEdges.length}, readyDevs : ${this.readyDevs.length}`);
            if (readyAll == 0)
                return [0, res];
            var sel = Math.floor(Math.random() * readyAll);
            if (sel < this.readyDevs.length) {
                var selDev = sel;
                var dev = this.readyDevs[selDev];
                this.readyDevs.splice(selDev, 1);
                assert(dev.is_rts());
                var rts = dev.rts;
                var rtsPorts = [];
                for (var k in rts) {
                    if (rts[k]) {
                        rtsPorts.push(k);
                    }
                }
                assert(rtsPorts.length > 0);
                var selPort = Math.floor(Math.random() * rtsPorts.length);
                var port = dev.deviceType.getOutput(rtsPorts[selPort]);
                var message = port.edgeType.message.create();
                var preState = dev.deviceType.state.import(dev.state);
                var preRts = clone(dev.rts);
                var doSend = port.onSend(this.g.properties, dev.properties, dev.state, message, dev.rts);
                res.push(new SendEvent(dev, port, preState, preRts, message, !doSend));
                //console.log(` send to ${dev.id} : state'=${JSON.stringify(dev.state)}, rts'=${JSON.stringify(dev.rts)}`);
                if (doSend) {
                    for (var _i = 0, _a = dev.outputs[port.name]; _i < _a.length; _i++) {
                        var e = _a[_i];
                        e.queue.push(message);
                        this.update_edge(e);
                    }
                }
                this.update_dev(dev);
            }
            else {
                var selEdge = sel - this.readyDevs.length;
                var e = this.readyEdges[selEdge];
                this.readyEdges.splice(selEdge, 1);
                var message = e.queue[0];
                e.queue.splice(0, 1);
                var preState = e.dstDev.deviceType.state.import(e.dstDev.state);
                var preRts = clone(e.dstDev.rts);
                e.dstPort.onReceive(this.g.properties, e.dstDev.properties, e.dstDev.state, e.properties, e.state, message, e.dstDev.rts);
                res.push(new ReceiveEvent(e, preState, preRts, message));
                //console.log(`   eprops = ${JSON.stringify(e.properties)}`);
                //console.log(`   recv on ${e.dstDev.id} : state'=${JSON.stringify(e.dstDev.state)}, rts'=${JSON.stringify(e.dstDev.rts)}`);
                this.update_dev(e.dstDev);
                this.update_dev(e.srcDev);
            }
            this.history = this.history.concat(res);
            return [res.length, res];
        };
        return SingleStepper;
    }());
    exports.SingleStepper = SingleStepper;
    var BatchStepper = (function () {
        function BatchStepper() {
            this.g = null;
            this.history = [];
        }
        BatchStepper.prototype.attach = function (graph, doInit) {
            assert(this.g == null);
            this.g = graph;
            for (var _i = 0, _a = this.g.enumDevices(); _i < _a.length; _i++) {
                var d = _a[_i];
                if (doInit && ("__init__" in d.deviceType.inputs)) {
                    var port = d.deviceType.inputs["__init__"];
                    var message = port.edgeType.message.create();
                    port.onReceive(this.g.properties, d.properties, d.state, port.edgeType.properties.create(), port.edgeType.state.create(), message, d.rts);
                }
                d.update();
            }
            for (var _b = 0, _c = this.g.enumEdges(); _b < _c.length; _b++) {
                var e = _c[_b];
                while (e.queue.length > 0) {
                    var h = e.queue.pop();
                    var message = e.queue[0];
                    e.queue.splice(0, 1);
                    e.dstPort.onReceive(this.g.properties, e.dstDev.properties, e.dstDev.state, e.properties, e.state, message, e.dstDev.rts);
                }
                e.update();
                e.srcDev.update();
                e.dstDev.update();
            }
        };
        BatchStepper.prototype.detach = function () {
            assert(this.g != null);
            var res = this.g;
            this.g = null;
            return res;
        };
        BatchStepper.prototype.step = function () {
            var count = 0;
            for (var _i = 0, _a = this.g.enumDevices(); _i < _a.length; _i++) {
                var dev = _a[_i];
                if (!dev.is_rts())
                    continue;
                var rts = dev.rts;
                var rtsPorts = [];
                for (var k in rts) {
                    if (rts[k]) {
                        rtsPorts.push(k);
                    }
                }
                assert(rtsPorts.length > 0);
                var selPort = Math.floor(Math.random() * rtsPorts.length);
                var port = dev.deviceType.getOutput(rtsPorts[selPort]);
                var message = port.edgeType.message.create();
                var doSend = port.onSend(this.g.properties, dev.properties, dev.state, message, dev.rts);
                ++count;
                if (doSend) {
                    for (var _b = 0, _c = dev.outputs[port.name]; _b < _c.length; _b++) {
                        var e = _c[_b];
                        e.dstPort.onReceive(this.g.properties, e.dstDev.properties, e.dstDev.state, e.properties, e.state, message, e.dstDev.rts);
                        e.dstDev.update_rts_only();
                        ++count;
                    }
                }
                dev.update_rts_only();
            }
            return [count, []];
        };
        return BatchStepper;
    }());
    exports.BatchStepper = BatchStepper;
});
//# sourceMappingURL=core.js.map