#include "graph_persist_dom_reader.hpp"

#include <libxml++/parsers/domparser.h>

#include <cstdio>

std::shared_ptr<xmlpp::DomParser> parseXML(const std::string &src)
{
    auto p=std::make_shared<xmlpp::DomParser>();
    p->parse_memory(src);
    return p;
}

std::string format(const char *pattern, ...)
{
    int w=INT_MAX;
    std::vector<char> tmp(strlen(pattern)*2);
    
    while(w>=(int)tmp.size()-1){
        va_list va;
        va_start(va, pattern);
        
        w=vsnprintf(&tmp[0], tmp.size()-1, pattern, va);
        va_end(va);
        
        if(w<0){
            throw std::runtime_error("vsnprintf problem.");
        }
        if(w>=(int)tmp.size()-1){           
            tmp.resize(w+1);
            w=INT_MAX;
        }
    }
    
    return std::string(&tmp[0]);
}

void test_scalar(const char *name)
{
    const char *src=R"(<?xml version="1.0"?>
<Scalar xmlns="https://poets-project.org/schemas/virtual-graph-schema-v2" name="wibble" type="%s" />
)";
    auto doc=parseXML(format(src,name));
    
    rapidjson::Document dd;
    
    auto ts=loadTypedDataSpecElement(doc->get_document()->get_root_node());
    auto p=std::dynamic_pointer_cast<TypedDataSpecElementScalar>(ts);
    assert(p);
    assert(p->getName()=="wibble");
    assert(p->getTypeName()==name);
    auto v=ts->getJSONDefault(dd.GetAllocator());
    assert(v.IsNumber());
    assert(v.GetDouble()==0);
    
    
    const char *srcD=R"(<?xml version="1.0"?>
<Scalar xmlns="https://poets-project.org/schemas/virtual-graph-schema-v2" name="wibble" type="%s" default="1" />
)";
    doc=parseXML(format(src,name));
    
    ts=loadTypedDataSpecElement(doc->get_document()->get_root_node());
    p=std::dynamic_pointer_cast<TypedDataSpecElementScalar>(ts);
    assert(p);
    assert(p->getTypeName()==name);
    v=ts->getJSONDefault(dd.GetAllocator());
    assert(v.IsNumber());
    assert(v.GetDouble()==0);
}

void test_array(unsigned length, const char *name)
{
    const char *src=R"(<?xml version="1.0"?>
<Array xmlns="https://poets-project.org/schemas/virtual-graph-schema-v2" name="wibble" length="%u" type="%s" />
)";
    auto doc=parseXML(format(src,length,name));
    
    rapidjson::Document dd;
    
    auto ts=loadTypedDataSpecElement(doc->get_document()->get_root_node());
    auto p=std::dynamic_pointer_cast<TypedDataSpecElementArray>(ts);
    assert(p);
    assert(p->getName()=="wibble");
    assert(p->getElementCount()==length);
    assert(p->getElementType()->isScalar());
}

void test_tuple_empty()
{
    const char *src=R"(<?xml version="1.0"?>
<Tuple xmlns="https://poets-project.org/schemas/virtual-graph-schema-v2" name="wibble" />
)";
    auto doc=parseXML(src);
    
    rapidjson::Document dd;
    
    auto ts=loadTypedDataSpecElement(doc->get_document()->get_root_node());
    auto p=std::dynamic_pointer_cast<TypedDataSpecElementTuple>(ts);
    assert(p);
    assert(p->getName()=="wibble");
    assert(p->size()==0);
}

void test_tuple_single()
{
    const char *src=R"(<?xml version="1.0"?>
<Tuple xmlns="https://poets-project.org/schemas/virtual-graph-schema-v2" name="wibble">
    <Scalar name="x" type="char" />
</Tuple>
)";
    auto doc=parseXML(src);
    
    rapidjson::Document dd;
    
    auto ts=loadTypedDataSpecElement(doc->get_document()->get_root_node());
    auto p=std::dynamic_pointer_cast<TypedDataSpecElementTuple>(ts);
    assert(p);
    assert(p->getName()=="wibble");
    assert(p->size()==1);
    
    std::vector<TypedDataSpecElementPtr> elts(p->begin(), p->end());
    assert(elts[0]->isScalar());
    assert(elts[0]->getName()=="x");
}

void test_tuple_double()
{
    const char *src=R"(<?xml version="1.0"?>
<Tuple xmlns="https://poets-project.org/schemas/virtual-graph-schema-v2" name="wibble">
    <Scalar name="x" type="char" />
    <Array name="y" length="10" type="char" />
</Tuple>
)";
    auto doc=parseXML(src);
    
    rapidjson::Document dd;
    
    auto ts=loadTypedDataSpecElement(doc->get_document()->get_root_node());
    auto p=std::dynamic_pointer_cast<TypedDataSpecElementTuple>(ts);
    assert(p);
    assert(p->getName()=="wibble");
    assert(p->size()==2);
    
    std::vector<TypedDataSpecElementPtr> elts(p->begin(), p->end());
    assert(elts[0]->isScalar());
    assert(elts[0]->getName()=="x");
    assert(elts[1]->isArray());
    assert(elts[1]->getName()=="y");
}

void test_tuple_triple()
{
    const char *src=R"(<?xml version="1.0"?>
<Tuple xmlns="https://poets-project.org/schemas/virtual-graph-schema-v2" name="wibble">
    <Scalar name="x" type="char" />
    <Array name="y" length="11" type="char" />
    <Tuple name="z" />
</Tuple>
)";
    auto doc=parseXML(src);
    
    rapidjson::Document dd;
    
    auto ts=loadTypedDataSpecElement(doc->get_document()->get_root_node());
    auto p=std::dynamic_pointer_cast<TypedDataSpecElementTuple>(ts);
    assert(p);
    assert(p->getName()=="wibble");
    assert(p->size()==3);
    
    std::vector<TypedDataSpecElementPtr> elts(p->begin(), p->end());
    assert(elts[0]->isScalar());
    assert(elts[0]->getName()=="x");
    assert(elts[1]->isArray());
    assert(elts[1]->getName()=="y");
    assert(elts[2]->isTuple());
    assert(elts[2]->getName()=="z");
}



int main()
{
    test_scalar("char");
    test_scalar("float");
    test_scalar("double");
    test_scalar("uint8_t");
    test_scalar("uint16_t");
    
    test_array(0, "char");
    test_array(1, "float");
    test_array(2, "uint64_t");
    
    test_tuple_empty();
    test_tuple_single();
    test_tuple_double();
    test_tuple_triple();
}
