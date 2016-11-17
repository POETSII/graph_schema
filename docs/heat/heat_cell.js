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
    var CellDeviceProperties = (function () {
        function CellDeviceProperties(nhood, iv, wSelf) {
            if (nhood === void 0) { nhood = 0; }
            if (iv === void 0) { iv = 0; }
            if (wSelf === void 0) { wSelf = 0; }
            this.nhood = nhood;
            this.iv = iv;
            this.wSelf = wSelf;
            this._name_ = "cell_properties";
        }
        ;
        return CellDeviceProperties;
    }());
    ;
    var CellDeviceState = (function () {
        function CellDeviceState(v, t, cs, ca, ns, na) {
            if (v === void 0) { v = 0; }
            if (t === void 0) { t = 0; }
            if (cs === void 0) { cs = 0; }
            if (ca === void 0) { ca = 0; }
            if (ns === void 0) { ns = 0; }
            if (na === void 0) { na = 0; }
            this.v = v;
            this.t = t;
            this.cs = cs;
            this.ca = ca;
            this.ns = ns;
            this.na = na;
            this._name_ = "cell_state";
        }
        ;
        return CellDeviceState;
    }());
    ;
    var CellInitInputPort = (function (_super) {
        __extends(CellInitInputPort, _super);
        function CellInitInputPort() {
            _super.call(this, "__init__", heat_types_1.initEdgeType);
        }
        CellInitInputPort.prototype.onReceive = function (graphPropertiesG, devicePropertiesG, deviceStateG, edgePropertiesG, edgeStateG, messageG, rts) {
            var deviceProperties = devicePropertiesG;
            var deviceState = deviceStateG;
            deviceState.v = deviceProperties.iv;
            deviceState.t = 0;
            deviceState.cs = deviceProperties.nhood; // Force us into the sending ready state
            deviceState.ca = deviceProperties.iv; // This is the first value
            deviceState.ns = 0;
            deviceState.na = 0;
            rts["out"] = deviceState.cs == deviceProperties.nhood;
        };
        return CellInitInputPort;
    }(InputPort));
    ;
    var CellInInputPort = (function (_super) {
        __extends(CellInInputPort, _super);
        function CellInInputPort() {
            _super.call(this, "in", heat_types_1.updateEdgeType);
        }
        CellInInputPort.prototype.onReceive = function (graphPropertiesG, devicePropertiesG, deviceStateG, edgePropertiesG, edgeStateG, messageG, rts) {
            var deviceProperties = devicePropertiesG;
            var deviceState = deviceStateG;
            var edgeProperties = edgePropertiesG;
            var message = messageG;
            //console.log("  w = "+edgeProperties.w);
            if (message.t == deviceState.t) {
                deviceState.cs++;
                deviceState.ca += edgeProperties.w * message.v;
            }
            else {
                assert(message.t == deviceState.t + 1);
                deviceState.ns++;
                deviceState.na += edgeProperties.w * message.v;
            }
            rts["out"] = deviceState.cs == deviceProperties.nhood;
        };
        return CellInInputPort;
    }(InputPort));
    ;
    var CellOutOutputPort = (function (_super) {
        __extends(CellOutOutputPort, _super);
        function CellOutOutputPort() {
            _super.call(this, "out", heat_types_1.updateEdgeType);
        }
        CellOutOutputPort.prototype.onSend = function (graphPropertiesG, devicePropertiesG, deviceStateG, messageG, rts) {
            var graphProperties = graphPropertiesG;
            var deviceProperties = devicePropertiesG;
            var deviceState = deviceStateG;
            var message = messageG;
            if (deviceState.t > graphProperties.maxTime) {
                rts["out"] = false;
                return false;
            }
            else {
                deviceState.v = deviceState.ca;
                deviceState.t++;
                deviceState.cs = deviceState.ns;
                deviceState.ca = deviceState.na + deviceProperties.wSelf * deviceState.v;
                deviceState.ns = 0;
                deviceState.na = 0;
                message.t = deviceState.t;
                message.v = deviceState.v;
                rts["out"] = deviceState.cs == deviceProperties.nhood;
                return true;
            }
        };
        return CellOutOutputPort;
    }(OutputPort));
    ;
    exports.cellDeviceType = new DeviceType("cell", new GenericTypedDataSpec(CellDeviceProperties), new GenericTypedDataSpec(CellDeviceState), [
        new CellInitInputPort(),
        new CellInInputPort()
    ], [
        new CellOutOutputPort()
    ]);
});
//# sourceMappingURL=heat_cell.js.map