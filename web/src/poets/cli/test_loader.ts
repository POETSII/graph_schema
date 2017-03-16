/// <reference path="../../../node_modules/@types/node/index.d.ts" />
/// <reference path="../../../node_modules/@types/xmldom/index.d.ts" />
/// <reference path="../../../node_modules/@types/jquery/index.d.ts" />

require('source-map-support').install();


var xmldom=require("xmldom");

import {loadGraphFromString} from "../core/core"
import {registerAllGraphTypes} from "../graphs/all"

registerAllGraphTypes();

// OMFG...
// http://stackoverflow.com/a/13411244
var content = '';
process.stdin.resume();
process.stdin.on('data', function(buf:Buffer) { content += buf.toString(); });
process.stdin.on('end', function() {

    var graph=loadGraphFromString(content);    
    console.log(graph);
});

