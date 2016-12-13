import * as POETS from "../core/core"

import assert = POETS.assert;
import TypedData = POETS.TypedData;
import EdgeType = POETS.EdgeType;
import DeviceType = POETS.DeviceType;
import GraphType = POETS.GraphType;
import TypedDataSpec = POETS.TypedDataSpec;
import InputPort = POETS.InputPort;
import OutputPort = POETS.OutputPort;
import Registry = POETS.Registry;


class DefaultRegistry
    implements Registry
{
    private _mapping : {[key:string]:GraphType;} = {};

  registerGraphType(graphType:GraphType) : void
  {
      this._mapping[graphType.id]=graphType;
  }

  lookupGraphType(id:string) : GraphType
  {
      return this._mapping[id];
  }
};

