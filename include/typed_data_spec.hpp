#ifndef typed_data_spec_hpp
#define typed_data_spec_hpp

#include <memory>
#include <cassert>
#include <climits>

#include <stdexcept>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <iostream>

#include "rapidjson/document.h"

class TypedDataSpecElement
{
private:
    std::string m_name;
    const char *m_internedName;

    bool m_defaultSet;
    std::vector<char> m_binaryDefault;
    rapidjson::Document m_jsonDefault;

    // Allow us to use names in StringRef
    // Only keys should go in here, as we don't want to bloat too much
    static const char *intern(const std::string &s)
    {
        static std::set<std::string> strings;
        strings.insert(s);
        return strings.find(s)->c_str();
    }
protected:
    TypedDataSpecElement(const std::string &name)
        : m_name(name)
        , m_internedName(intern(name))
        , m_defaultSet(false)
    {
        static_assert( CHAR_BIT==8, "This assumes that we can use char for type punning, and that char is a byte...");
    }

    void setBinaryDefault(const std::vector<char> &binaryDefault)
    {
        //std::cerr<<"binaryDefault.size() = "<<binaryDefault.size()<<"\n";

        assert(!m_defaultSet);
        m_defaultSet=true;

        m_binaryDefault=binaryDefault;

        auto def=binaryToJSON(&m_binaryDefault[0], m_binaryDefault.size(), m_jsonDefault.GetAllocator());
        def.Swap(m_jsonDefault);

    }
public:
  virtual ~TypedDataSpecElement()
  {}

  const std::string &getName() const
  { return m_name; }


    //! Get a constant string that can be used within JSON without allocation
    rapidjson::Value getNameValue() const
    { return rapidjson::Value(rapidjson::StringRef(m_internedName)); }

  size_t getPayloadSize() const
  { return m_binaryDefault.size(); }

  void createBinaryDefault(char *pBinary, unsigned cbBinary) const
  {
    if(cbBinary!=m_binaryDefault.size()){

        std::stringstream tmp;
        tmp<<"createBinaryDefault - incorrect binary size of "<<cbBinary<<", expected "<<m_binaryDefault.size();
        throw std::runtime_error(tmp.str());
    }
    std::memcpy(pBinary, &m_binaryDefault[0], m_binaryDefault.size());
  }

  void createBinaryRandom(char *pBinary, unsigned cbBinary) const
  {//Generates random numbers for every byte of the payload, and add this to the message.
    if (cbBinary!=m_binaryDefault.size()){
        std::stringstream tmp;
        tmp<<"createBinaryDefault - incorrect binary size of "<<cbBinary<<", expected "<<m_binaryDefault.size();
        throw std::runtime_error(tmp.str());
    }
    std::vector<char> m_binaryRandom;
    m_binaryRandom.resize(m_binaryDefault.size());
    for (unsigned i = 0; i < m_binaryDefault.size(); i++)
    {
        m_binaryRandom[i] = rand() % 256;
    }
    std::memcpy(pBinary, &m_binaryRandom[0], m_binaryDefault.size());
  }

  //! Returns a fully populated JSON value, including all members (include zeros)
  rapidjson::Value getJSONDefault(rapidjson::Document::AllocatorType &alloc) const
  {
    return rapidjson::Value(m_jsonDefault, alloc); // create a copy
  }

  virtual rapidjson::Value binaryToJSON(const char *pBinary, unsigned cbBinary, rapidjson::Document::AllocatorType& alloc) const=0;

    virtual void JSONToBinary(const rapidjson::Value &value, char *pBinary, unsigned cbBinary, bool alreadyDefaulted=false) const=0;

  virtual bool isTuple() const
  { return false; }

  virtual bool isScalar() const
  { return false; }

  virtual bool isArray() const
  { return false; }

  virtual bool isUnion() const
  { return false; }
};
typedef std::shared_ptr<TypedDataSpecElement> TypedDataSpecElementPtr;

class TypedDataSpecElementScalar
  : public TypedDataSpecElement
{
public:
    enum ScalarType
    {
        ScalarType_width_8   = 0x0,
        ScalarType_width_16  = 0x1,
        ScalarType_width_32  = 0x2,
        ScalarType_width_64  = 0x3,
        ScalarType_width_mask  = 0xF,

        ScalarType_kind_unsigned = 0x10,
        ScalarType_kind_signed  = 0x20,
        ScalarType_kind_float   = 0x30,
        ScalarType_kind_char   = 0x40,
        ScalarType_kind_mask   = 0xF0,

        ScalarType_uint8_t    =ScalarType_width_8 | ScalarType_kind_unsigned,
        ScalarType_uint16_t    =ScalarType_width_16 | ScalarType_kind_unsigned,
        ScalarType_uint32_t    =ScalarType_width_32 | ScalarType_kind_unsigned,
        ScalarType_uint64_t    =ScalarType_width_64 | ScalarType_kind_unsigned,

        ScalarType_int8_t    =ScalarType_width_8 | ScalarType_kind_signed,
        ScalarType_int16_t    =ScalarType_width_16 | ScalarType_kind_signed,
        ScalarType_int32_t    =ScalarType_width_32 | ScalarType_kind_signed,
        ScalarType_int64_t    =ScalarType_width_64 | ScalarType_kind_signed,

        ScalarType_half    =ScalarType_width_16 | ScalarType_kind_float,
        ScalarType_float    =ScalarType_width_32 | ScalarType_kind_float,
        ScalarType_double    =ScalarType_width_64 | ScalarType_kind_float,

        ScalarType_char    =ScalarType_width_8 | ScalarType_kind_char
    };

    static ScalarType typeNameToScalarType(const std::string &name)
    {
        if(name=="char") return ScalarType_char;

        if(name=="uint8_t") return ScalarType_uint8_t;
        if(name=="uint16_t") return ScalarType_uint16_t;
        if(name=="uint32_t") return ScalarType_uint32_t;
        if(name=="uint64_t") return ScalarType_uint64_t;

        if(name=="int8_t") return ScalarType_int8_t;
        if(name=="int16_t") return ScalarType_int16_t;
        if(name=="int32_t") return ScalarType_int32_t;
        if(name=="int64_t") return ScalarType_int64_t;

        if(name=="half") return ScalarType_half;
        if(name=="float") return ScalarType_float;
        if(name=="double") return ScalarType_double;

        throw std::runtime_error("toScalarType - unknown type '"+name+"'");
    }

    static unsigned scalarTypeWidthBytes(ScalarType st)
    {
        switch(st & ScalarType_width_mask){
        case ScalarType_width_8: return 1;
        case ScalarType_width_16: return 2;
        case ScalarType_width_32: return 4;
        case ScalarType_width_64: return 8;
        default: assert(0); throw std::runtime_error("Unknown width."); return INT_MAX;
        }
    }
private:
    ScalarType m_type;
    std::string m_typeString;
    std::string m_defaultString;

    template<class T>
    rapidjson::Value binaryToJSONImpl(const char *pBinary, unsigned cbBinary) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        T val=*(const T*)pBinary;
        return rapidjson::Value(val);
    }

    template<class T>
    void JSONToBinaryImpl(const rapidjson::Value &value, char *pBinary, unsigned cbBinary) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        double v = value.GetDouble();
        *((T*)pBinary) = v;
    }

    void JSONToBinaryImplU64(const rapidjson::Value &value, char *pBinary, unsigned cbBinary) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        uint64_t v = value.GetUint64();
        *((uint64_t*)pBinary) = v;
    }

    void JSONToBinaryImplI64(const rapidjson::Value &value, char *pBinary, unsigned cbBinary) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        int64_t v = value.GetInt64();
        *((int64_t*)pBinary) = v;
    }

public:
    TypedDataSpecElementScalar(const std::string &name, const std::string &typeName, std::string defaultValue=std::string())
        : TypedDataSpecElement(name)
        , m_type(typeNameToScalarType(typeName))
        , m_typeString(typeName)
    {
        unsigned cb=scalarTypeWidthBytes(m_type);
        //std::cerr<<"cb = "<<cb<<"\n";
        std::vector<char> tmp(cb, 0);
        if(!defaultValue.empty()){
            rapidjson::Document doc;
            doc.Parse(defaultValue.c_str());
            JSONToBinary(doc, &tmp[0], cb, true);
        }
        //std::cerr<<"cb = "<<cb<<"\n";
        setBinaryDefault(tmp);
    }

    virtual ~TypedDataSpecElementScalar()
    {}

    virtual bool isScalar() const
    { return true; }

    virtual rapidjson::Value binaryToJSON(const char *pBinary, unsigned cbBinary, rapidjson::Document::AllocatorType&) const override
    {
        if(cbBinary!=scalarTypeWidthBytes(m_type)){
            throw std::runtime_error("binaryToJSON - Wrong binary size for element.");
        }

        switch(m_type){
        case ScalarType_uint8_t: return binaryToJSONImpl<uint8_t>(pBinary, cbBinary);
        case ScalarType_uint16_t: return binaryToJSONImpl<uint16_t>(pBinary, cbBinary);
        case ScalarType_uint32_t: return binaryToJSONImpl<uint32_t>(pBinary, cbBinary);
        case ScalarType_uint64_t: return binaryToJSONImpl<uint64_t>(pBinary, cbBinary);

        case ScalarType_int8_t: return binaryToJSONImpl<int8_t>(pBinary, cbBinary);
        case ScalarType_int16_t: return binaryToJSONImpl<int16_t>(pBinary, cbBinary);
        case ScalarType_int32_t: return binaryToJSONImpl<int32_t>(pBinary, cbBinary);
        case ScalarType_int64_t: return binaryToJSONImpl<int64_t>(pBinary, cbBinary);

        case ScalarType_half: throw std::runtime_error("Half not implemented yet.");
        case ScalarType_float: return binaryToJSONImpl<float>(pBinary, cbBinary);
        case ScalarType_double: return binaryToJSONImpl<double>(pBinary, cbBinary);

        case ScalarType_char: return binaryToJSONImpl<char>(pBinary, cbBinary);

        default: throw std::runtime_error("Unknown or not implemented type.");
        }
    }

    virtual void JSONToBinary(const rapidjson::Value &value, char *pBinary, unsigned cbBinary, bool alreadyDefaulted=false) const override
    {
        if(cbBinary!=scalarTypeWidthBytes(m_type)){
            throw std::runtime_error("binaryToJSON - Wrong binary size for element.");
        }

        if(!alreadyDefaulted){
            createBinaryDefault(pBinary, cbBinary);
        }

        switch(m_type){
        case ScalarType_uint8_t:  JSONToBinaryImpl<uint8_t>(value, pBinary, cbBinary); break;
        case ScalarType_uint16_t:  JSONToBinaryImpl<uint16_t>(value, pBinary, cbBinary); break;
        case ScalarType_uint32_t:  JSONToBinaryImpl<uint32_t>(value, pBinary, cbBinary); break;
        case ScalarType_uint64_t:  JSONToBinaryImplU64(value, pBinary, cbBinary); break;

        case ScalarType_int8_t:  JSONToBinaryImpl<int8_t>(value, pBinary, cbBinary); break;
        case ScalarType_int16_t:  JSONToBinaryImpl<int16_t>(value, pBinary, cbBinary); break;
        case ScalarType_int32_t:  JSONToBinaryImpl<int32_t>(value, pBinary, cbBinary); break;
        case ScalarType_int64_t:  JSONToBinaryImplI64(value, pBinary, cbBinary); break;

        case ScalarType_half:  throw std::runtime_error("JSONtoBinary - half not implemented yet."); break;
        case ScalarType_float:  JSONToBinaryImpl<float>(value, pBinary, cbBinary); break;
        case ScalarType_double:  JSONToBinaryImpl<double>(value, pBinary, cbBinary); break;

        case ScalarType_char:  JSONToBinaryImpl<char>(value, pBinary, cbBinary); break;

        default: throw std::runtime_error("JSONToBinary - Unknown scalar type.");
        }
    }

    const std::string &getTypeName() const
    { return m_typeString; }
};
typedef std::shared_ptr<TypedDataSpecElementScalar> TypedDataSpecElementScalarPtr;

class TypedDataSpecElementTuple
  : public TypedDataSpecElement
{
private:
    typedef std::vector<TypedDataSpecElementPtr> elements_by_index_t;
    typedef std::map<std::string,std::pair<TypedDataSpecElementPtr,unsigned> > elements_and_offsets_by_name_t;

    elements_by_index_t m_elementsByIndex;
    elements_and_offsets_by_name_t m_elementsAndOffsetsByName;
    unsigned m_sizeBytes;

    void add(TypedDataSpecElementPtr element)
    {
        if(m_elementsAndOffsetsByName.find(element->getName()) != m_elementsAndOffsetsByName.end()){
            throw std::runtime_error("An element called '"+element->getName()+"' already exists in tuple.");
        }
        m_elementsByIndex.push_back(element);
        m_elementsAndOffsetsByName.insert(std::make_pair(element->getName(),std::make_pair(element,m_sizeBytes)));
        m_sizeBytes+=element->getPayloadSize();
    }
public:
    template<class TIt>
    TypedDataSpecElementTuple(const std::string &name, TIt begin, TIt end)
        : TypedDataSpecElement(name)
        , m_sizeBytes(0)
    {
        std::vector<char> tmp;

        while(begin!=end){
            unsigned cb=(*begin)->getPayloadSize();

            add(*begin);

            unsigned off=tmp.size();
            tmp.resize(off+cb);
            (*begin)->createBinaryDefault(&tmp[off], cb);

            off+=cb;
            ++begin;
        }

        setBinaryDefault(tmp);
    }

    typedef elements_by_index_t::const_iterator const_iterator;

    virtual bool isTuple() const override
    { return true; }

    virtual void JSONToBinary(const rapidjson::Value &value, char *pBinary, unsigned cbBinary, bool alreadyDefaulted=false) const override
    {
        if(cbBinary != getPayloadSize() ){
            throw std::runtime_error("JSONToBinary - Invalid binary size.");
        }
        if(!value.IsObject()){
            throw std::runtime_error("JSONToBinary - initialiser for tuple should be object.");
        }

        if(!alreadyDefaulted){
            createBinaryDefault(pBinary, cbBinary);
        }

        auto it=value.MemberBegin();
        auto end=value.MemberEnd();
        while(it!=end){
            auto &kv = *it;
            auto eo = m_elementsAndOffsetsByName.find( kv.name.GetString() );
            if(eo==m_elementsAndOffsetsByName.end()){
                throw std::runtime_error("Unknown element in JSON initialiser.");
            }
            eo->second.first->JSONToBinary(kv.value, pBinary+eo->second.second, eo->second.first->getPayloadSize(), true);
            ++it;
        }
    }

    virtual rapidjson::Value binaryToJSON(const char *pBinary, unsigned cbBinary, rapidjson::Document::AllocatorType &alloc) const override
    {
        rapidjson::Value res(rapidjson::kObjectType);

        unsigned off=0;
        for(const auto &e : m_elementsByIndex)
        {
            auto val=e->binaryToJSON(pBinary+off, e->getPayloadSize(), alloc);
            res.AddMember(e->getNameValue(), val, alloc);

            off += e->getPayloadSize();
        }

        return res;
    }

    unsigned size() const
    { return m_elementsByIndex.size(); }

    const_iterator begin() const
    { return m_elementsByIndex.begin(); }

    const_iterator end() const
    { return m_elementsByIndex.end(); }
};
typedef std::shared_ptr<TypedDataSpecElementTuple> TypedDataSpecElementTuplePtr;


class TypedDataSpecElementArray
  : public TypedDataSpecElement
{
private:
    unsigned m_eltCount;
    TypedDataSpecElementPtr m_eltType;
public:
    TypedDataSpecElementArray(const std::string &name, unsigned count, TypedDataSpecElementPtr type, const std::string &defaultValue=std::string())
        : TypedDataSpecElement(name)
        , m_eltCount(count)
        , m_eltType(type)
    {
        unsigned size=count*type->getPayloadSize();

        std::vector<char> tmp(size, 0);
        if(!defaultValue.empty()){
            rapidjson::Document doc;
            doc.Parse(defaultValue.c_str());
            JSONToBinary(doc, &tmp[0], size, true);
        }

        setBinaryDefault(tmp);
    }

    virtual bool isArray() const override
    { return true; }

    virtual void JSONToBinary(const rapidjson::Value &value, char *pBinary, unsigned cbBinary, bool alreadyDefaulted=false) const override
    {
        if(cbBinary != getPayloadSize() ){
            throw std::runtime_error("JSONToBinary - Invalid binary size.");
        }

        if(!value.IsArray()){
            throw std::runtime_error("JSONToBinary - initialiser for array should be array.");
        }

        if(!alreadyDefaulted){
            createBinaryDefault(pBinary, cbBinary);
        }

        unsigned off=0;
        unsigned cb=m_eltType->getPayloadSize();
        for(unsigned i=0; i<m_eltCount; i++){
            m_eltType->JSONToBinary(value[i], pBinary+off, cb, true);
            off+=cb;
        }
    }

    virtual rapidjson::Value binaryToJSON(const char *pBinary, unsigned cbBinary, rapidjson::Document::AllocatorType &alloc) const override
    {
        rapidjson::Value res(rapidjson::kArrayType);
        res.Reserve(m_eltCount, alloc);

        unsigned off=0;
        unsigned cb=m_eltType->getPayloadSize();
        for(unsigned i=0; i<m_eltCount; i++)
        {
            auto val=m_eltType->binaryToJSON(pBinary+off, cb, alloc);
            res.PushBack(val, alloc);

            off += cb;
        }

        return res;
    }

    unsigned getElementCount() const
    { return m_eltCount; }

    TypedDataSpecElementPtr getElementType() const
    { return m_eltType; }
};
typedef std::shared_ptr<TypedDataSpecElementArray> TypedDataSpecElementArrayPtr;

/*
class TypedDataSpecElementUnion
  : public TypedDataSpecElement
{
private:
    typedef std::vector<TypedDataSpecElementPtr> elements_by_index_t;
    typedef std::map<std::string,TypedDataSpecElementPtr> elements_by_name_t;

    elements_by_index_t m_elementsByIndex;
    elements_by_name_t m_elementsByName;
    unsigned m_sizeBytes;

    void add(TypedDataSpecElementPtr element)
    {
        if(m_elementsByName.find(element->getName())){
            throw std::runtime_error("An element called '"+element->getName()+"' already exists in tuple.");
        }
        if(m_elementsByIndex.size()>=256){
            throw std::runtime_error("Can't have more than 256 alternatives in union.");
        }
        m_elementsByIndex.push_back(element);
        m_elementsByName.insert(std::make_pair(element->getName(),element));
        m_sizeBytes=std::max(m_sizeBytes, 1+element->getPayloadSize());
    }
public:
    template<TIt>
    TypedDataSpecElementTuple(const std::string &name, TIt begin, TIt end)
        : TypedDataSpecElement(name)
        , m_sizeBytes(1) // For the tag
    {
        while(begin!=end){
            add(*begin);
            ++begin;
        }
    }

    typedef iterator_t elements_by_index_t::const_iterator const_iterator;

    virtual size_t getPayloadSize() const override
    { return m_sizeBytes; }

    virtual bool isTuple() const override
    { return true; }

    unsigned count() const
    { return m_elementsByIndex.size(); }

    TypedDataSpecElementPtr getElement(unsigned index) const
    { return m_elementsByIndex.at(index); }

    TypedDataSpecElementPtr getElement(const std::string &name) const
    { return m_elementsByIndex.at(name); }

    const_iterator begin() const
    { return m_elementsByIndex.begin(); }

    const_iterator end() const
    { return m_elementsByIndex.end(); }
};
*/

TypedDataSpecElementScalarPtr makeScalar(const std::string &name, const std::string &type, const std::string &defaultValue=std::string())
{
    return std::make_shared<TypedDataSpecElementScalar>(name, type, defaultValue);
}

TypedDataSpecElementTuplePtr makeTuple(const std::string &name, const std::initializer_list<TypedDataSpecElementPtr> &elts)
{
    return std::make_shared<TypedDataSpecElementTuple>(name, elts.begin(), elts.end());
}

template<class TIt>
TypedDataSpecElementTuplePtr makeTuple(const std::string &name, TIt begin, TIt end)
{
    return std::make_shared<TypedDataSpecElementTuple>(name, begin, end);
}


TypedDataSpecElementArrayPtr makeArray(const std::string &name, unsigned n, TypedDataSpecElementPtr elt)
{
    return std::make_shared<TypedDataSpecElementArray>(name, n, elt);
}

#endif
