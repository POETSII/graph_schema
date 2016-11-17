/// <reference path="core.ts" />
define(["require", "exports", "./core"], function (require, exports, POETS) {
    "use strict";
    var EdgeType = POETS.EdgeType;
    var GenericTypedDataSpec = POETS.GenericTypedDataSpec;
    var UpdateMessage = (function () {
        function UpdateMessage(source_device, source_port, t, v) {
            if (t === void 0) { t = 0; }
            if (v === void 0) { v = 0; }
            this.source_device = source_device;
            this.source_port = source_port;
            this.t = t;
            this.v = v;
            this._name_ = "update_message";
        }
        ;
        return UpdateMessage;
    }());
    exports.UpdateMessage = UpdateMessage;
    ;
    var UpdateEdgeProperties = (function () {
        function UpdateEdgeProperties(w) {
            if (w === void 0) { w = 0; }
            this.w = w;
            this._name_ = "update_properties";
        }
        ;
        return UpdateEdgeProperties;
    }());
    exports.UpdateEdgeProperties = UpdateEdgeProperties;
    exports.initEdgeType = new EdgeType("__init__");
    exports.updateEdgeType = new EdgeType("update", new GenericTypedDataSpec(UpdateMessage), new GenericTypedDataSpec(UpdateEdgeProperties));
    var HeatGraphProperties = (function () {
        function HeatGraphProperties(maxTime) {
            if (maxTime === void 0) { maxTime = 1000000; }
            this.maxTime = maxTime;
            this._name_ = "heat_properties";
        }
        ;
        return HeatGraphProperties;
    }());
    exports.HeatGraphProperties = HeatGraphProperties;
    ;
});
//# sourceMappingURL=heat_types.js.map