#!/usr/bin/env python3

import re
import sys
from typing import *
from collections import OrderedDict

from graph.core import TypedDataSpec, TupleTypedDataSpec, ScalarTypedDataSpec, ArrayTypedDataSpec

literal_tokens={
    r'{' : "LCURLY",
    r'}' : "RCURLY",
    r'[' : "LSQUARE",
    r']' : "RSQUARE",
    r';' : "SEMICOLON",
    'int8_t' : "TYPE",
    'uint8_t' : "TYPE",
    'int16_t' : "TYPE",
    'uint16_t' : "TYPE",
    'int32_t' : "TYPE",
    'uint32_t' : "TYPE",
    'int64_t' : "TYPE",
    'uint64_t' : "TYPE",
    'float' : "TYPE",
    'double' : "TYPE",
    'struct' : "STRUCT"
}

def escape_re(x:str) -> str:
    for i in ";[]{}+.*/":
        x=x.replace(i, "\\"+i)
    return x


re_CPPCOMMENT=re.compile('//.*') # chomp to end of line
re_KEYWORDS=re.compile("|".join([escape_re(k) for k in literal_tokens.keys()]))
re_ID=re.compile("[_a-zA-Z][_a-zA-Z0-9]*")
re_INT=re.compile("0x[0-9a-fA-F]+|[1-9][0-9]*")

print(re_KEYWORDS)

class Token(NamedTuple):
    type : str
    value : any
    line : int
    col : int

def to_token_stream(input:str):
    pos=0
    col=1
    line=0
    while pos<len(input):
        ch=input[pos]
        if ch=='\n':
            pos+=1
            col=1
            line+=1
            continue

        if ch.isspace():
            pos+=1
            col+=1
            continue

        m=re_CPPCOMMENT.match(input, pos)
        if m:
            val=m.group(0)
            pos+=len(val)
            col+=len(val)
            continue

        m=re_KEYWORDS.match(input, pos)
        if m:
            val=m.group(0)
            yield Token( literal_tokens[val], val, line, col )
            pos+=len(val)
            col+=len(val)
            continue

        m=re_ID.match(input, pos)
        if m:
            val=m.group(0)
            yield Token("ID", val, line, col )
            pos += len(val)
            col+=len(val)
            continue

        m=re_INT.match(input, pos)
        if m:
            val=m.group(0)
            yield Token( "INT", int(val), line, col )
            pos += len(val)
            col+=len(val)
            continue

        raise RuntimeError("Couldn't lex C structure string starting from '%s'", input[pos:pos+16])

class Def:
    def __init__(self):
        self.id=None
        self.dimensions=[]

    def _is_compatible_base(self, value) -> bool:
        raise NotImplementedError()

    def _is_compatible_dims(self, value, dim:int) -> bool:
        #sys.stderr.write(f"_is_compatible_dims({value},{dim})\n")
        if dim >= len(self.dimensions):
            return self._is_compatible_base(value)
        else:
            if len(value)!=self.dimensions[dim]:
                return False
            for v in value:
                if not self._is_compatible_dims(v, dim+1):
                    return False
            return True

    def is_compatible_init(self, value) -> bool:
        return self._is_compatible_dims(value, 0)

class StructDef(Def):
    def __init__(self):
        super().__init__()
        self.members=OrderedDict()

    def print(self, dst, indent):
        dst.write(f"{indent}struct{{\n")
        for m in self.members.values():
            m.print(dst, indent+"  ")
        dst.write(f"{indent}}} {self.id}")
        for i in self.dimensions:
            dst.write(f"[{i}]")
        dst.write(";\n")

    
    def _is_compatible_base(self, value) -> bool:
        #sys.stderr.write(f"is_compatible_struct({value})\n")
        if len(value)!=len(self.members):
            return False
        for (m,v) in zip(self.members.values(),value):
            if not m.is_compatible_init(v):
                return False
        return True

class ScalarDef(Def):
    def __init__(self):
        super().__init__()
        self.type=None

    def print(self, dst, indent):
        dst.write(f"{indent}{self.type} {self.id}")
        for i in self.dimensions:
            dst.write(f"[{i}]")
        dst.write(";\n")

    def _is_compatible_base(self, value) -> bool:
        #sys.stderr.write(f"is_compatible_scalar({value})\n")
        return isinstance(value,(int,float))


class TokenSrc:
    def __init__(self, source:str):
        self.tokens=list(to_token_stream(source))
        sys.stderr.write(f"{self.tokens}\n")
        self.pos=0
        self._last=None

    def end_of_stream(self) -> bool:
        return self.pos==len(self.tokens)

    def consume(self, type:str):
        if self.pos >= len(self.tokens):
            raise RuntimeError(f"Expecting token of type {type}, but found end of input.")

        tok=self.tokens[self.pos]
        if tok.type!=type:
            raise RuntimeError(f"Expecting token of type {type} at line {tok.line} col {tok.col} of struct, but got type {tok.type}.")
        self._last=tok

        self.pos+=1

    def try_consume(self, type:str) -> bool:
        if self.pos >= len(self.tokens):
            return False
        
        if self.tokens[self.pos].type!=type:
            return False
        
        self.consume(type)
        return True

    def error(self, msg:str):
        tok=self.tokens[self.pos]
        raise RuntimeError(f"Error {msg} at line {tok.line} col {tok.col} of struct. tokens=[{self.tokens[self.pos:] }]")

    @property
    def last(self):
        assert self._last
        return self._last


def parse_struct_member(src:TokenSrc):
    if src.try_consume("STRUCT"):
        res=StructDef()
        src.consume("LCURLY")
        while not src.try_consume("RCURLY"):
            member=parse_struct_member(src)
            assert member.id not in res.members
            res.members[member.id]=member
        
        src.consume("ID")
        res.id=src.last.value
    elif src.try_consume("TYPE"):
        res=ScalarDef()
        res.type=src.last.value
        src.consume("ID")
        res.id=src.last.value
    else:
        src.error("Expected struct member to start with 'struct' or a type name.")

    while src.try_consume("LSQUARE"):
        src.consume("INT")
        res.dimensions.append(src.last.value)
        src.consume("RSQUARE")

    src.consume("SEMICOLON")
    
    return res

def parse_struct_def_string(input):
    res=StructDef()
    res.id="_outer_"
    if input is not None and input!="":
        try:
            src=TokenSrc(input)
            while not src.end_of_stream():
                m=parse_struct_member(src)
                assert m not in res.members
                res.members[m.id]=m
        except:
            sys.stderr.write(f"Input struct = '{input}'")
            raise
    return res

re_DELIMITER=re.compile("[{,}]", re.RegexFlag.MULTILINE)

def parse_struct_init_string_impl(input:str, pos:int=0) -> Tuple[List[any],int]:
    res=[]

    n=len(input)

    while pos<n and input[pos].isspace():
        pos+=1

    if input[pos]!='{':
        raise RuntimeError(f"Missing opening brace")
    lcurly=pos
    pos+=1

    while pos < len(input):
        m=re_DELIMITER.search(input, pos)
        if not m:
            raise RuntimeError(f"Couldn't parse initialiser at pos {pos}, next chars={input[pos:]} ")
        end=m.start()

        delim=input[end]
        if delim=='{':
            (val,pos)=parse_struct_init_string_impl(input, end)
            res.append(val)
        else:
            assert delim==',' or delim=='}'
            valstr=input[pos:end].strip()
            if valstr!="":
                try:
                    val=int(valstr)
                except ValueError:
                    try:
                        val=float(valstr)
                    except ValueError:
                        raise RuntimeError(f"Couldn't parse {valstr} as an int or a float")
                res.append(val)
            pos=end+1
            if delim=='}':
                return (res,pos)

    raise RuntimeError("Missing terminating brace for intialiser")

def parse_struct_init_string(input:str) -> List[any]:
    try:
        (res,pos)=parse_struct_init_string_impl(input, 0)
    except:
        sys.stderr.write(f"Input struct = '{input}'")
        raise
    assert input[pos:]=="", f"string remaining = '{input[pos:]}'"
    return res

def convert_def_to_typed_data_spec(d:Def) -> TypedDataSpec:
    inner_name = "_" if len(d.dimensions)>0 else d.id
    if isinstance(d,ScalarDef):
        res=ScalarTypedDataSpec(inner_name, d.type)
    elif isinstance(d,StructDef):
        elements=[convert_def_to_typed_data_spec(c) for c in d.members.values()]
        res=TupleTypedDataSpec(inner_name, elements)    
    for (i,length) in enumerate(reversed(d.dimensions)):
        outer_name = d.id if i==len(d.dimensions)-1 else "_"
        res=ArrayTypedDataSpec(outer_name, length, res)
    return res

def convert_typed_data_spec_to_def(d:TypedDataSpec) -> Def:
    if isinstance(d,ScalarTypedDataSpec):
        res=ScalarDef()
        res.id=d.name
        res.type=d.type
    elif isinstance(d,TupleTypedDataSpec):
        res=StructDef()
        res.id=d.name
        for e in d.elements_by_index:
            te=convert_typed_data_spec_to_def(e)
            res.members[te.id]=te
    elif isinstance(d,ArrayTypedDataSpec):
        res=convert_typed_data_spec_to_def(d.type)
        res.dimensions.insert(0, d.length)
        res.id=d.name
    else:
        assert False
    return res

if __name__=="__main__":
    r=parse_struct_def_string("""int32_t x;
    """)
    r.print(sys.stdout, "")

    i1=parse_struct_init_string("{0}")
    print(i1)
    assert r.is_compatible_init(i1)
    assert not r.is_compatible_init(parse_struct_init_string('{}'))
    assert not r.is_compatible_init(parse_struct_init_string('{0,0}'))

    ts=convert_def_to_typed_data_spec(r)
    print(ts)
    print()

    r=parse_struct_def_string("""
    int32_t x;
    float y;
    """)
    r.print(sys.stdout, "")
    assert r.is_compatible_init(parse_struct_init_string('{0,0}'))
    assert not r.is_compatible_init(parse_struct_init_string('{0}'))

    ts=convert_def_to_typed_data_spec(r)
    print(ts)
    dr=convert_typed_data_spec_to_def(ts)
    dr.print(sys.stdout,"")
    print()


    r=parse_struct_def_string("""
    struct {
        int32_t x;
        float y;
    } z;
    """)
    r.print(sys.stdout, "")

    ts=convert_def_to_typed_data_spec(r)
    print(ts)
    dr=convert_typed_data_spec_to_def(ts)
    dr.print(sys.stdout,"")
    print()


    r=parse_struct_def_string("""
    int8_t x[4];
    """)
    r.print(sys.stdout, "")
    assert not r.is_compatible_init(parse_struct_init_string('{0,0, 4.0}'))
    i1=parse_struct_init_string('{{0,0, 4.0, 1.0}}')
    print(i1)
    assert r.is_compatible_init(i1)

    ts=convert_def_to_typed_data_spec(r)
    print(ts)
    dr=convert_typed_data_spec_to_def(ts)
    dr.print(sys.stdout,"")
    print()


    r=parse_struct_def_string("""
    float _[4][10];
    """)
    r.print(sys.stdout, "")

    r=parse_struct_def_string("""
    struct { int8_t x; } sds[4][3];
    """)
    r.print(sys.stdout, "")
    i1=parse_struct_init_string('{ { {{0},{1},{2}}, {{0},{1},{2}}, {{0},{1},{2}}, {{0},{1},{2}} } }')
    print(i1)
    assert r.is_compatible_init(i1)

    ts=convert_def_to_typed_data_spec(r)
    print(ts)
    dr=convert_typed_data_spec_to_def(ts)
    dr.print(sys.stdout,"")
    print()

    r=parse_struct_def_string("""float b; uint8_t f; uint16_t g[3];""")
    r.print(sys.stdout, "")

