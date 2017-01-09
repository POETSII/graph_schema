import {DeviceInstance,TypedDataSpec,TypedDataType,TypedData} from "../core/core"

export class StatePanel
{
    private _table : HTMLTableElement|null=null;
    private _propMapping : [TypedDataType,HTMLSpanElement][] = [];
    private _stateMapping : [TypedDataType,HTMLSpanElement][] = [];
    private _rate : HTMLTextAreaElement = null;
    
    private _currentDevice : DeviceInstance|null = null; 

    constructor(
        public container : HTMLDivElement
    ){
    }

    private makeCell(msg:string) : HTMLTableRowElement
    {
        var res=new HTMLTableRowElement();
        res.innerText=msg;
        return res;
    }

    private addHeaderRow() : void
    {
        var res=document.createElement("tr");
        var p1=document.createElement("th");
        p1.innerText="Name";
        var p2=document.createElement("th");
        p2.innerText="Value";
        res.appendChild(p1);
        res.appendChild(p2);
        this._table.appendChild(res);
    }
    
    private addRateRow() : void
    {
        var res=document.createElement("tr");
        var p1=document.createElement("td");
        p1.innerText="rate";
        var p2=document.createElement("td");
        var p3=document.createElement("textarea");
        p2.appendChild(p3);
        res.appendChild(p1);
        res.appendChild(p2);    
        this._table.appendChild(res);
        this._rate=p3;

        let device=this._currentDevice;

        this._rate.onchange = function()
        {
            let val=p3.value;
            let r=parseFloat(val);
            if(r<0) r=0;
            if(r>1) r=1;

            device.rate=r;
        };
    }

    private addDataRow(type:TypedDataType, isProp:boolean) : void
    {
        var res=document.createElement("tr");
        var p1=document.createElement("td");
        p1.innerText=type.name;
        var p2=document.createElement("td");
        var p3=document.createElement("span");
        p2.appendChild(p3);
        res.appendChild(p1);
        res.appendChild(p2);    
        this._table.appendChild(res);
        if(isProp){
            this._propMapping.push([type,p3]);
        }else{
            this._stateMapping.push([type,p3]);
        }
    }

    detach():void
    {
        this._propMapping=[];
        this._stateMapping=[];
        this._currentDevice=null;
        this._table=null;
        this.container.innerHTML=""; // Delete existing table?
    }

    attach(device:DeviceInstance):void
    {
        this.detach();

        if(device==null)
            return;

        this._currentDevice=device;
        this._table=document.createElement("table");
        this.addHeaderRow();
        this.addRateRow();
        for(let elt of device.properties._spec_.elementsByIndex){
            this.addDataRow(elt,true);
        }
        for(let elt of device.state._spec_.elementsByIndex){
            this.addDataRow(elt,false);
        }
        this.container.appendChild(this._table);

        this._rate.innerText = this._currentDevice.rate.toFixed(3);

        this.update();
    }

    update() : void
    {
        if(!this._currentDevice)
            return;
        for(let [type,span] of this._propMapping){
            let val=this._currentDevice.properties[type.name];
            span.innerText=type.format(val);
        }
        for(let [type,span] of this._stateMapping){
            let val=this._currentDevice.state[type.name];
            span.innerText=type.format(val);
        }
    }

};