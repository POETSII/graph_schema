/// <reference path="heat_types.ts" />
var __extends = (this && this.__extends) || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
};
define(["require", "exports", "./core", "./heat_types"], function (require, exports, POETS, heat_types_1) {
    "use strict";
    var assert = POETS.assert;
    var DeviceType = POETS.DeviceType;
    var GenericTypedDataSpec = POETS.GenericTypedDataSpec;
    var InputPort = POETS.InputPort;
    var OutputPort = POETS.OutputPort;
    var DirichletDeviceProperties = (function () {
        function DirichletDeviceProperties(neighbours, amplitude, phase, frequency, bias) {
            if (neighbours === void 0) { neighbours = 0; }
            if (amplitude === void 0) { amplitude = 1; }
            if (phase === void 0) { phase = 0.5; }
            if (frequency === void 0) { frequency = 1; }
            if (bias === void 0) { bias = 0; }
            this.neighbours = neighbours;
            this.amplitude = amplitude;
            this.phase = phase;
            this.frequency = frequency;
            this.bias = bias;
            this._name_ = "dirichlet_properties";
        }
        return DirichletDeviceProperties;
    }());
    ;
    var DirichletDeviceState = (function () {
        function DirichletDeviceState(v, t, cs, ns) {
            if (v === void 0) { v = 0; }
            if (t === void 0) { t = 0; }
            if (cs === void 0) { cs = 0; }
            if (ns === void 0) { ns = 0; }
            this.v = v;
            this.t = t;
            this.cs = cs;
            this.ns = ns;
            this._name_ = "dirichlet_state";
        }
        ;
        return DirichletDeviceState;
    }());
    ;
    var DirichletInitInputPort = (function (_super) {
        __extends(DirichletInitInputPort, _super);
        function DirichletInitInputPort() {
            _super.call(this, "__init__", heat_types_1.initEdgeType);
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
            _super.call(this, "in", heat_types_1.updateEdgeType);
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
            _super.call(this, "out", heat_types_1.updateEdgeType);
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
                message.t = deviceState.t;
                message.v = deviceState.v;
                rts["out"] = deviceState.cs == deviceProperties.neighbours;
                return true;
            }
        };
        return DirichletOutOutputPort;
    }(OutputPort));
    ;
    exports.dirichletDeviceType = new DeviceType("dirichlet", new GenericTypedDataSpec(DirichletDeviceProperties), new GenericTypedDataSpec(DirichletDeviceState), [
        new DirichletInitInputPort(),
        new DirichletInInputPort()
    ], [
        new DirichletOutOutputPort()
    ]);
});
//# sourceMappingURL=heat_dirichlet.js.map