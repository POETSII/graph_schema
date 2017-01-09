import {registerGraphType} from "../core/core"
import {heatGraphType} from "./heat"

export function registerAllGraphTypes()
{
    registerGraphType(heatGraphType);
}