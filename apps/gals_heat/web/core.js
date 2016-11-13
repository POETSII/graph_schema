var __extends = (this && this.__extends) || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
};
var DataClass;
(function (DataClass) {
    DataClass[DataClass["Message"] = 0] = "Message";
    DataClass[DataClass["Properties"] = 1] = "Properties";
    DataClass[DataClass["State"] = 2] = "State";
})(DataClass || (DataClass = {}));
;
var TypeClass;
(function (TypeClass) {
    TypeClass[TypeClass["Graph"] = 0] = "Graph";
    TypeClass[TypeClass["Device"] = 1] = "Device";
    TypeClass[TypeClass["Edge"] = 2] = "Edge";
})(TypeClass || (TypeClass = {}));
;
;
;
var Message = (function () {
    function Message(_type_id_, _source_) {
        this._type_id_ = _type_id_;
        this._source_ = _source_;
        this._data_class_ = DataClass.Message;
        this._type_class_ = TypeClass.Edge;
    }
    return Message;
}());
;
var GraphProperties = (function () {
    function GraphProperties(_type_id_) {
        this._type_id_ = _type_id_;
        this._data_class_ = DataClass.Properties;
        this._type_class_ = TypeClass.Graph;
    }
    return GraphProperties;
}());
;
var DeviceProperties = (function () {
    function DeviceProperties(_type_id_) {
        this._type_id_ = _type_id_;
        this._data_class_ = DataClass.Properties;
        this._type_class_ = TypeClass.Device;
    }
    return DeviceProperties;
}());
;
var EdgeProperties = (function () {
    function EdgeProperties(_type_id_) {
        this._type_id_ = _type_id_;
        this._data_class_ = DataClass.Properties;
        this._type_class_ = TypeClass.Edge;
    }
    return EdgeProperties;
}());
;
var DeviceState = (function () {
    function DeviceState(_type_id_) {
        this._type_id_ = _type_id_;
        this._data_class_ = DataClass.State;
        this._type_class_ = TypeClass.Device;
    }
    return DeviceState;
}());
;
var EdgeState = (function () {
    function EdgeState(_type_id_) {
        this._type_id_ = _type_id_;
        this._data_class_ = DataClass.State;
        this._type_class_ = TypeClass.Edge;
    }
    return EdgeState;
}());
;
;
;
;
var EdgeType = (function () {
    function EdgeType() {
    }
    return EdgeType;
}());
;
;
;
;
;
;
var GraphStepper = (function () {
    function GraphStepper() {
    }
    return GraphStepper;
}());
;
var UpdateEdgeMessage = (function (_super) {
    __extends(UpdateEdgeMessage, _super);
    function UpdateEdgeMessage(source, time, heat) {
        if (time === void 0) { time = 0; }
        if (heat === void 0) { heat = 0.0; }
        _super.call(this, "update", source);
        this.time = time;
        this.heat = heat;
    }
    return UpdateEdgeMessage;
}(Message));
;
var UpdateEdgeType = (function () {
    function UpdateEdgeType() {
        this.messageType = typeof (UpdateEdgeMessage);
        this.propertiesType = typeof (EdgeProperties);
        this.stateType = typeof (EdgeState);
    }
    return UpdateEdgeType;
}());
;
