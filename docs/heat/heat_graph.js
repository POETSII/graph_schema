/// <reference path="heat_types.ts" />
/// <reference path="heat_dirichlet.ts" />
/// <reference path="heat_cell.ts" />
define(["require", "exports", "./core", "./heat_types", "./heat_dirichlet", "./heat_cell"], function (require, exports, POETS, heat_types_1, heat_dirichlet_1, heat_cell_1) {
    "use strict";
    var assert = POETS.assert;
    var GraphType = POETS.GraphType;
    var GenericTypedDataSpec = POETS.GenericTypedDataSpec;
    exports.heatGraphType = new GraphType("gals_heat", new GenericTypedDataSpec(heat_types_1.HeatGraphProperties), [
        heat_dirichlet_1.dirichletDeviceType,
        heat_cell_1.cellDeviceType
    ]);
    function makeGrid(width, height) {
        var h = Math.sqrt(1.0 / (width * height));
        var alpha = 1;
        var dt = h * h / (4 * alpha) * 0.5;
        //let dt=0.05;
        assert(h * h / (4 * alpha) >= dt);
        var wOther = dt * alpha / (h * h);
        var wSelf = (1.0 - 4 * wOther);
        console.log(" wOther=" + wOther + ", eSelf=" + wSelf);
        var g = new POETS.GraphInstance(exports.heatGraphType, { maxTime: 1000000 });
        for (var y = 0; y < width; y++) {
            var T = y == 0;
            var B = y == height - 1;
            var H = T || B;
            for (var x = 0; x < height; x++) {
                var L = x == 0;
                var R = x == width - 1;
                var V = L || R;
                if (H && V)
                    continue;
                var id = "d_" + x + "_" + y;
                if (x == Math.floor(width / 2) && y == Math.floor(height / 2)) {
                    var props = { "bias": 0, "amplitude": 1.0, "phase": 1.5, "frequency": 100 * dt, "neighbours": 4 };
                    g.addDevice(id, heat_dirichlet_1.dirichletDeviceType, props, { x: x, y: y });
                }
                else if (H || V) {
                    var props = { "bias": 0, "amplitude": 1.0, "phase": 1, "frequency": 70 * dt * ((x / width) + (y / height)), "neighbours": 1 };
                    g.addDevice(id, heat_dirichlet_1.dirichletDeviceType, props, { x: x, y: y });
                }
                else {
                    var props = { nhood: 4, wSelf: wSelf, iv: Math.random() * 2 - 1 };
                    g.addDevice(id, heat_cell_1.cellDeviceType, props, { x: x, y: y });
                }
                console.log(" " + y + " " + x);
            }
        }
        for (var y = 0; y < width; y++) {
            var T = y == 0;
            var B = y == height - 1;
            var H = T || B;
            for (var x = 0; x < height; x++) {
                var L = x == 0;
                var R = x == width - 1;
                var V = L || R;
                if (H && V)
                    continue;
                var id = "d_" + x + "_" + y;
                if (L) {
                    g.addEdge("d_" + (x + 1) + "_" + y, "in", id, "out", { w: wOther });
                }
                else if (R) {
                    g.addEdge("d_" + (x - 1) + "_" + y, "in", id, "out", { w: wOther });
                }
                else if (T) {
                    g.addEdge("d_" + x + "_" + (y + 1), "in", id, "out", { w: wOther });
                }
                else if (B) {
                    g.addEdge("d_" + x + "_" + (y - 1), "in", id, "out", { w: wOther });
                }
                else {
                    g.addEdge("d_" + (x - 1) + "_" + y, "in", id, "out", { w: wOther });
                    g.addEdge("d_" + (x + 1) + "_" + y, "in", id, "out", { w: wOther });
                    g.addEdge("d_" + x + "_" + (y - 1), "in", id, "out", { w: wOther });
                    g.addEdge("d_" + x + "_" + (y + 1), "in", id, "out", { w: wOther });
                }
                console.log(" " + y + " " + x);
            }
        }
        return g;
    }
    exports.makeGrid = makeGrid;
});
//# sourceMappingURL=heat_graph.js.map