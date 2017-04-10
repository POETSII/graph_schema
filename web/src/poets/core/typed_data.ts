
export class TypedData
{
    constructor(
        public readonly _name_ : string,
        public readonly _spec_ : TypedDataSpec
    ){
    }
    
    [x:string]:any;

    equals(other:TypedData) : boolean
    { return this._spec_.equals(this,other); }
    
    clone() : TypedData
    { return this._spec_.clone(this); }
};

class EmptyTypedData
    implements TypedData
{
    readonly _name_ = "empty";

    constructor(
        public readonly _spec_:TypedDataSpec
    ){}

    clone() : TypedData
    {
        return new EmptyTypedData(this._spec_);
    }

    equals(other:TypedData) : boolean
    {
        if(other instanceof EmptyTypedData ){
            return true;
        }else{
            throw "Attempt to compare incompatible typed data instances";
        }
    }
};


export interface TypedDataType
{
    name : string;

    is_composite : boolean;

    format(data:TypedData) : string;
}

export class ScalarDataType
    implements TypedDataType
{
    is_composite=false;

    constructor(
        public name:string,
        public type:"float"|"int"|"boolean",
        public defaultValue:any = 0
    )
    {}

    format(data:any) : string
    {
        if(this.type=="float"){
            return Number(data).toFixed(3);
        }else if (this.type=="int"){
            return Number(data).toString();
        }else if(this.type=="boolean"){
            return data ? "true" : "false";
        }else{
            throw "Invalid data type";
        }
    }
}

export function tFloat(name:string) : ScalarDataType
{ return new ScalarDataType(name,"float");}

export function tInt(name:string) : ScalarDataType
{ return new ScalarDataType(name,"int");}

export function tBoolean(name:string) : ScalarDataType
{ return new ScalarDataType(name,"boolean");}


export class VectorDataType
    implements TypedDataType
{
    is_composite=true;

    constructor(
        public name:string,
        public elementType:ScalarDataType,
        public elementCount:number
    )
    {}

    format(data:any) : string
    {
        var res="[";
        if(data instanceof Array){
            let et=this.elementType;
            var first=true;
            for(let e of data){
                if(first){
                    first=false;
                }else{
                    res=res+","+et.format(e);
                }
            }
        }else{
            throw "Data is not an array";
        }
        res=res+"]";
        return res;
    }
}

export class TupleDataType
    implements TypedDataType
{
    is_composite = true;

    elements : {[key:string]:TypedDataType} = {};

    constructor(
        public name:string,
        public elementsByIndex:TypedDataType[]
    )
    {
        for(let e of elementsByIndex){
            if(e.name in this.elements){
                throw "Duplicate tuple element name";
            }
            this.elements[e.name]=e;
        }
    }

    format(data:any) : string
    {
        var res="{";
        var first=true;
        for(let et of this.elementsByIndex){
            let v = data[et.name];
            if(first){
                first=false;
            }else{
                res=res+",";
            }
            res=res+et.name+":"+et.format(v);
        }
        res=res+"}";
        return res;
    }
}

export interface TypedDataSpec
    extends TupleDataType
{
    import(data:any) : TypedData;

    create() : TypedData;

    clone(data:TypedData):TypedData;

    import(data:any|null):TypedData;

    equals(a:TypedData,b:TypedData):boolean;
}

export class EmptyTypedDataSpec
    implements TypedDataSpec
{
    is_composite = true;

    elements : {[key:string]:TypedDataSpec} = {};
    elementsByIndex : TypedDataSpec[]= [];

    private _shared : EmptyTypedData = new EmptyTypedData(this);

    constructor(
        public name:string = "empty"
    )
    {}

    clone(x:TypedData) : TypedData
    { return this._shared; }

    equals(a:TypedData,b:TypedData) : boolean
    {
        if( (a instanceof EmptyTypedData) && (b instanceof EmptyTypedData) ){
            return true;
        }else{
            throw "Cannot compare data from different specs.";
        }
    }

    create() : TypedData
    {
        return this._shared;
    }

    import(data:any) : TypedData
    {
        for(let n in data){
            throw "Unexpected property";
        }
        return this._shared;
    }

    format(data:any) : string
    {
        return "{}";
    }
}


function clone_typed_data(orig:TypedData):TypedData
{
    let spec=orig._spec_;

    var res=orig._spec_.create();

    for(let e of spec.elementsByIndex){
        if(e instanceof ScalarDataType){
            res[e.name]=orig[e.name];
        }else{
            throw "NotImplemented";
        }
    }

    return res;
}

function equals_typed_data(a:TypedData, b:TypedData):boolean
{
    let spec=a._spec_;
    if(spec!=b._spec_){
        throw "Typed data comes from different specs";
    }

    for(let e of spec.elementsByIndex){
        if(e instanceof ScalarDataType){
            if(a[e.name] != b[e.name] ){
                return false;
            }
        }else{
            throw "NotImplemented";
        }
    }

    return true;
}

function import_typed_data(spec:TypedDataSpec, data:any) : TypedData
{
    var res=spec.create();

    if(data!=null){
        for(let e of spec.elementsByIndex){
            if(data.hasOwnProperty(e.name)){
                res[e.name] = data[e.name];
            }else if(e instanceof ScalarDataType){
                res[e.name] = e.defaultValue;
            }else{
                throw "NotImplemented";
            }
        }
    }

    return res;
}


export class GenericTypedDataSpec<T extends TypedData>
    implements TypedDataSpec
{
    readonly is_composite = true;

    name : string;

    readonly elements : {[key:string]:TypedDataType} = {};
    

    constructor(
        // https://github.com/Microsoft/TypeScript/issues/2794
        private readonly newT : { new(_spec_:TypedDataSpec,  ...args : any[]) : T; } ,
        public elementsByIndex:TypedDataType[]
    ) {
        this.name = new newT(this)._name_;

        for(let e of elementsByIndex){
            if(e.name in this.elements){
                throw "Duplicate tuple element name";
            }
            this.elements[e.name]=e;
        }
    };
    
    create () : TypedData {
        return new this.newT(this);
    };

    equals(a:TypedData,b:TypedData):boolean
    {
        return equals_typed_data(a,b);
    }

    // This will work for both another typed data instance, or a general object
    import(data : any|null) : TypedData
    {
        return import_typed_data(this,data);
    }

    clone(x:TypedData):TypedData
    { return clone_typed_data(x); }

    format(data:any) : string
    {
        var res="{";
        var first=true;
        for(let et of this.elementsByIndex){
            let v = data[et.name];
            if(first){
                first=false;
            }else{
                res=res+",";
            }
            res=res+et.name+":"+et.format(v);
        }
        res=res+"}";
        return res;
    }
};
/*
class GenericTypedDataSpec<T extends TypedData>
{
    constructor()

    create() : TypedData
    {
        return this._shared;
    }

    import(data:any) : TypedData
    {
        for(let n in data){
            throw "Unexpected property";
        }
        return this._shared;
    }

    format(data:any) : string
    {
        return "{}";
    }
}*/
