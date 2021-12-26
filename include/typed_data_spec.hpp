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
#include <string>

#include "rapidjson/document.h"

// Forward declare
class TypedDataSpec;
typedef std::shared_ptr<TypedDataSpec> TypedDataSpecPtr;

class graph_type_mismatch_error
  : public std::runtime_error
{
private:
  std::string m_graphId;
public:
  graph_type_mismatch_error(const std::string &msg)
    : std::runtime_error(msg)
  {}
};

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

        auto def=binaryToJSON(m_binaryDefault.empty() ? nullptr : &m_binaryDefault[0], m_binaryDefault.size(), m_jsonDefault.GetAllocator());
        def.Swap(m_jsonDefault);

    }
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
    if(!m_binaryDefault.empty()){
        std::memcpy(pBinary, &m_binaryDefault[0], m_binaryDefault.size());
    }
  }

  bool isBinaryDefault(const char *pBinary, unsigned cbBinary) const
  {
    if(cbBinary!=m_binaryDefault.size()){

        std::stringstream tmp;
        tmp<<"createBinaryDefault - incorrect binary size of "<<cbBinary<<", expected "<<m_binaryDefault.size();
        throw std::runtime_error(tmp.str());
    }
    if(m_binaryDefault.empty()){
        return true;
    }
    return !std::memcmp(pBinary, &m_binaryDefault[0], m_binaryDefault.size());
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

  virtual void binaryToXmlV4Value(const char *pBinary, unsigned cbBinary, std::ostream &dst, int formatMinorVersion) const=0;

  virtual void xmlV4ValueToBinary(std::istream &src, char *pBinary, unsigned cbBinary, bool alreadyDefaulted, int formatMinorVersion) const=0;

    virtual bool check_is_equivalent(TypedDataSpecElement *other, bool throw_on_error, const std::string &error_prefix) const =0;

    virtual void dumpStructure(std::ostream &dst, const std::string &indent) const=0;

    struct sub_element_position
    {
        const TypedDataSpecElement *element;
        size_t offset;
        size_t length;
    };

    /* Find the position of an element within the struct.
        Synatax is:
          "" (empty) : identify this thing.
          name : identify a value called "name" within a struct
          name[idx] : identify element at index "idx" within an array called name
          name.member : identify element at index "idx" within 

    */
    virtual sub_element_position findSubElementPosition(const std::string &path) const =0;

    template<class TVal>
    void setScalarSubElement(const sub_element_position &pos, size_t cbDst, void *pDst, TVal val) const
    {
        if(pos.offset+pos.length > getPayloadSize()){
            throw std::runtime_error("Offset is corrupt");
        }
        if(!pos.element->isScalar()){
            throw std::runtime_error("Target element is not scalar.");
        }
        if(pos.length!=sizeof(TVal)){
            throw std::runtime_error("Size of value doesnt match size of element.");
        }
        if(cbDst!=getPayloadSize()){
            throw std::runtime_error("Size of dst doesnt match struct size.");
        }
        memcpy((char*)pDst+pos.offset, &val, pos.length);
    }

    template<class TVal>
    void setScalarSubElement(const std::string &path, size_t cbDst, void *pDst, TVal x) const
    {
        try{
            setScalarSubElement(findSubElementPosition(path), cbDst, pDst, x);
        }catch(std::exception &e)
        {
            std::cerr<<"Exception while trying to set sub-element "+path+"\n";
            throw;
        }
    }

};
typedef std::shared_ptr<TypedDataSpecElement> TypedDataSpecElementPtr;

class TypedDataSpecElementScalar
  : public TypedDataSpecElement
{
public:

    static ScalarType typeNameToScalarType(const std::string &name, const std::vector<TypedDataSpecPtr> typedefs)
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
        // Tinsel requires 4-byte alignment, but not 8-byte
        if(sizeof(T)<4){
            assert( (intptr_t(pBinary )%sizeof(T))==0 );
        }else{
            assert( (intptr_t(pBinary )%4)==0 );
        }
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


    template<class T,class O=T>
    void binaryToXmlV4ValueImpl(const char *pBinary, unsigned cbBinary, std::ostream &dst) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        assert(cbBinary == sizeof(T));
        T val;
        memcpy(&val, pBinary, sizeof(T));
        O tmp=val;
        dst<<tmp;
        if(std::is_same<T,uint64_t>::value && tmp!=0){
            dst<<"ull";
        }
        if(std::is_same<T,int64_t>::value && tmp!=0){
            dst<<"ll";
        }
    }

    void binaryToXmlV4ValueImplFloat(const char *pBinary, unsigned cbBinary, std::ostream &dst) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        float val;
        memcpy(&val, pBinary, sizeof(float));
        int prec=dst.precision(9);
        dst<<std::scientific<<val;
        dst.precision(prec);
    }

    void binaryToXmlV4ValueImplDouble(const char *pBinary, unsigned cbBinary, std::ostream &dst) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        double val;
        memcpy(&val, pBinary, sizeof(double));
        int prec=dst.precision(17);
        dst<<std::scientific<<val;
        dst.precision(prec);
    }

    template<class T>
    void xmlV4ValueToBinaryImpl(std::istream &src, char *pBinary, unsigned cbBinary) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        assert(cbBinary == sizeof(T));
        T val;
        if( ! (src>>val) ){
            throw std::runtime_error("Couldn't parse scalar while extracting xmlV4Value.");
        }
        memcpy(pBinary, &val, sizeof(T));
    }

    template<class T>
    void xmlV4ValueToBinaryImplInt(std::istream &src, char *pBinary, unsigned cbBinary) const
    {
        assert(cbBinary == scalarTypeWidthBytes(m_type));
        assert(cbBinary == sizeof(T));
        T tval;
        if(!std::is_same<T,uint64_t>::value){
            int64_t val;
            if( ! (src>>val) ){
                throw std::runtime_error("Couldn't parse scalar while extracting xmlV4Value.");
            }
            if(val < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < val){
                throw std::runtime_error("Value out of range.");
            }
            tval=(T)val;
        }else{
            uint64_t val;
            if( ! (src>>val) ){
                throw std::runtime_error("Couldn't parse scalar while extracting xmlV4Value.");
            }
            tval=(T)val;
        }
        memcpy(pBinary, &tval, sizeof(T));

        // consume a C type suffix
        if(src.peek()=='u'){
            src.get();
        }
        if(src.peek()=='l'){
            src.get();
        }
        if(src.peek()=='l'){
            src.get();
        }
    }

public:
    TypedDataSpecElementScalar(const std::string &name, const std::string &typeName, std::string defaultValue=std::string())
        : TypedDataSpecElement(name)
        , m_type(typeNameToScalarType(typeName, {}))
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
        case ScalarType_uint64_t:  JSONToBinaryImpl<uint64_t>(value, pBinary, cbBinary); break;

        case ScalarType_int8_t:  JSONToBinaryImpl<int8_t>(value, pBinary, cbBinary); break;
        case ScalarType_int16_t:  JSONToBinaryImpl<int16_t>(value, pBinary, cbBinary); break;
        case ScalarType_int32_t:  JSONToBinaryImpl<int32_t>(value, pBinary, cbBinary); break;
        case ScalarType_int64_t:  JSONToBinaryImpl<int64_t>(value, pBinary, cbBinary); break;

        case ScalarType_half:  throw std::runtime_error("JSONtoBinary - half not implemented yet."); break;
        case ScalarType_float:  JSONToBinaryImpl<float>(value, pBinary, cbBinary); break;
        case ScalarType_double:  JSONToBinaryImpl<double>(value, pBinary, cbBinary); break;

        default: throw std::runtime_error("JSONToBinary - Unknown/unsupported scalar type.");
        }
    }

    virtual void binaryToXmlV4Value(const char *pBinary, unsigned cbBinary, std::ostream &dst, int formatMinorVersion) const
    {
        if(formatMinorVersion!=0){
            throw std::runtime_error("binaryToXmlV4Value - Only formatMinorVersion==0 supported by this program.");
        }
        if(cbBinary!=scalarTypeWidthBytes(m_type)){
            throw std::runtime_error("binaryToXmlV4Value - Wrong binary size for element.");
        }

        switch(m_type){
        case ScalarType_uint8_t: binaryToXmlV4ValueImpl<uint8_t,unsigned>(pBinary, cbBinary, dst); return;
        case ScalarType_uint16_t:binaryToXmlV4ValueImpl<uint16_t,unsigned>(pBinary, cbBinary, dst); return;
        case ScalarType_uint32_t:binaryToXmlV4ValueImpl<uint32_t>(pBinary, cbBinary, dst); return;
        case ScalarType_uint64_t:binaryToXmlV4ValueImpl<uint64_t>(pBinary, cbBinary, dst); return;

        case ScalarType_int8_t:binaryToXmlV4ValueImpl<int8_t,int>(pBinary, cbBinary, dst); return;
        case ScalarType_int16_t:binaryToXmlV4ValueImpl<int16_t,int>(pBinary, cbBinary, dst); return;
        case ScalarType_int32_t:binaryToXmlV4ValueImpl<int32_t>(pBinary, cbBinary, dst); return;
        case ScalarType_int64_t:binaryToXmlV4ValueImpl<int64_t>(pBinary, cbBinary, dst); return;

        case ScalarType_half: throw std::runtime_error("Half not implemented yet.");
        case ScalarType_float:binaryToXmlV4ValueImplFloat(pBinary, cbBinary, dst); return;
        case ScalarType_double:binaryToXmlV4ValueImplDouble(pBinary, cbBinary, dst); return;

        case ScalarType_char: throw std::runtime_error("TODO: Is char actually a legal type for v4?");

        default: throw std::runtime_error("Unknown or not implemented type.");
        }
    }

    virtual void xmlV4ValueToBinary(std::istream &src, char *pBinary, unsigned cbBinary, bool alreadyDefaulted, int formatMinorVersion) const
    {
        if(formatMinorVersion!=0){
            throw std::runtime_error("xmlV4ValueToBinary - Only formatMinorVersion==0 supported by this program.");
        }
        if(cbBinary!=scalarTypeWidthBytes(m_type)){
            throw std::runtime_error("xmlV4ValueToBinary - Wrong binary size for element.");
        }

        if(!alreadyDefaulted){
            createBinaryDefault(pBinary, cbBinary);
        }

        switch(m_type){
        case ScalarType_uint8_t:  xmlV4ValueToBinaryImplInt<uint8_t>(src,pBinary, cbBinary); break;
        case ScalarType_uint16_t:  xmlV4ValueToBinaryImplInt<uint16_t>(src,pBinary, cbBinary); break;
        case ScalarType_uint32_t:  xmlV4ValueToBinaryImpl<uint32_t>(src,pBinary, cbBinary); break;
        case ScalarType_uint64_t:  xmlV4ValueToBinaryImpl<uint64_t>(src, pBinary, cbBinary); break;

        case ScalarType_int8_t:  xmlV4ValueToBinaryImplInt<int8_t>(src,pBinary, cbBinary); break;
        case ScalarType_int16_t:  xmlV4ValueToBinaryImplInt<int16_t>(src,pBinary, cbBinary); break;
        case ScalarType_int32_t:  xmlV4ValueToBinaryImpl<int32_t>(src,pBinary, cbBinary); break;
        case ScalarType_int64_t:  xmlV4ValueToBinaryImpl<int64_t>(src, pBinary, cbBinary); break;

        case ScalarType_half:  throw std::runtime_error("JSONtoBinary - half not implemented yet."); break;
        case ScalarType_float:  xmlV4ValueToBinaryImpl<float>(src,pBinary, cbBinary); break;
        case ScalarType_double:  xmlV4ValueToBinaryImpl<double>(src,pBinary, cbBinary); break;

        case ScalarType_char:  xmlV4ValueToBinaryImpl<char>(src,pBinary, cbBinary); break;

        default: throw std::runtime_error("JSONToBinary - Unknown scalar type.");
        }

    }

    const std::string &getTypeName() const
    { return m_typeString; }

    bool check_is_equivalent(TypedDataSpecElement *other, bool throw_on_error, const std::string &error_prefix) const
    {
        auto tother = dynamic_cast<TypedDataSpecElementScalar*>(other);
        if(!tother){
            if(throw_on_error){
                throw graph_type_mismatch_error(error_prefix + "Other element is not a scalar.");
            }
            return false;
        }

        if(getTypeName() != tother->getTypeName()){
            if(throw_on_error){
                throw graph_type_mismatch_error(error_prefix + "Expected scalar type "+getTypeName()+" but got "+tother->getTypeName());
            }
            return false;
        }
        return true;
    }

    virtual void dumpStructure(std::ostream &dst, const std::string &indent) const
    {
        dst<<indent<<"<Scalar name='"<<getName()<<"' type='"<<getTypeName()<<"' />\n";
    }

    sub_element_position findSubElementPosition(const std::string &path) const override
    {
        if(!path.empty()){
            throw std::runtime_error("Can only use empty path on scalar, but got "+path);
        }
        return {
            this,
            0,
            scalarTypeWidthBytes(m_type)
        };
    }

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
            if(!e->isBinaryDefault(pBinary+off, e->getPayloadSize())){
                auto val=e->binaryToJSON(pBinary+off, e->getPayloadSize(), alloc);
                res.AddMember(e->getNameValue(), val, alloc);
            }

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

    TypedDataSpecElementPtr at(unsigned i) const
    {
        return m_elementsByIndex.at(i);
    }

    virtual void binaryToXmlV4Value(const char *pBinary, unsigned cbBinary, std::ostream &dst, int formatMinorVersion) const
    {
        if(formatMinorVersion!=0){
            throw std::runtime_error("formatMinorVersion!=0");
        }

        dst<<'{';

        unsigned off=0;
        for(const auto &e : m_elementsByIndex)
        {
            if(off!=0){
                dst<<',';
            }

            e->binaryToXmlV4Value(pBinary+off, e->getPayloadSize(), dst, formatMinorVersion);
            off += e->getPayloadSize();
        }

        dst<<'}';
    }

    virtual void xmlV4ValueToBinary(std::istream &src, char *pBinary, unsigned cbBinary, bool alreadyDefaulted, int formatMinorVersion) const
    {
        if(formatMinorVersion!=0){
            throw std::runtime_error("formatMinorVersion!=0");
        }

        auto expect=[&](char value)
        {
            char got;
            if(!(src>>got)){
                throw std::runtime_error("xmlV4ValueToBinary - Couldn't read char while parsing tuple value.");
            }
            if(got!=value){
                std::stringstream tmp;
                tmp<<"xmlV4ValueToBinary - Got unexpected char while parsing tuple value. Got '"<<got<<"', but expected '"<<value<<"'";
                throw std::runtime_error(tmp.str().c_str());
            }
        };

        expect('{');

        unsigned off=0;
        for(const auto &e : m_elementsByIndex){
            if(off!=0){
                if(src.peek()=='u'){
                    src.get();
                }
                expect(',');
            }

            e->xmlV4ValueToBinary(src, pBinary+off, e->getPayloadSize(), alreadyDefaulted, formatMinorVersion);
            off += e->getPayloadSize();
        }

        expect('}');
    }

    bool check_is_equivalent(TypedDataSpecElement *other, bool throw_on_error, const std::string &error_prefix) const
    {
        auto tother = dynamic_cast<TypedDataSpecElementTuple*>(other);
        if(!tother){
            if(throw_on_error){
                throw graph_type_mismatch_error(error_prefix + "Other element is not a tuple.");
            }
            return false;
        }

        if(size()!=tother->size()){
            if(throw_on_error){
                throw graph_type_mismatch_error(error_prefix + "Expected "+std::to_string(size())+" members, but got "+std::to_string(tother->size()));
            }
            return false;
        }

        for(unsigned i=0; i<size(); i++){
            TypedDataSpecElementPtr left=m_elementsByIndex.at(i);
            TypedDataSpecElementPtr right=tother->at(i);

            if(left->getName() != right->getName()){
                if(throw_on_error){
                    throw graph_type_mismatch_error(error_prefix + "Expected "+std::to_string(i)+"'th element to be called "+left->getName()+", but it was called "+right->getName());
                }
                return false;
            }

            if(!left->check_is_equivalent(right.get(), true, error_prefix+"::"+left->getName())){
                return false;
            }
        }

        return true;
    }

    virtual void dumpStructure(std::ostream &dst, const std::string &indent) const
    {
        dst<<indent<<"<Tuple name='"<<getName()<<"'>\n";
        for(auto e : m_elementsByIndex){
            e->dumpStructure(dst, indent+"  ");
        }
        dst<<indent<<"</Tuple>\n";
    }


    sub_element_position findSubElementPosition(const std::string &path) const override
    {
        if(path.empty()){
            return {
                this,
                0,
                m_sizeBytes
            };
        }

        auto epos=path.find_first_of("[.");
        std::string now=path.substr(0,epos);
        std::string next;
        if(epos!=std::string::npos){
            next=path.substr(epos+1, std::string::npos);
        }
        fprintf(stderr, "now=%s, next=%s\n", now.c_str(), next.c_str());

        if(now.empty()){
            throw std::runtime_error("No immediate part to index into tuple.");
        }

        auto it=m_elementsAndOffsetsByName.find(now);
        if(it==m_elementsAndOffsetsByName.end()){
            throw std::runtime_error("Unknown tuple member "+now);
        }

        size_t offset=it->second.second;
        auto res=it->second.first->findSubElementPosition(next);
        res.offset += offset;

        return res;        
    }
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
        if(value.Size()>m_eltCount){
            throw std::runtime_error("JSONToBinary - expected array with at most "+std::to_string(m_eltCount)+" entries, but value had "+std::to_string(value.Size())+" .");
        }

        if(!alreadyDefaulted){
            createBinaryDefault(pBinary, cbBinary);
        }

        unsigned off=0;
        unsigned cb=m_eltType->getPayloadSize();
        // If there are less then m_eltCount values then we rely on the default
        for(unsigned i=0; i<value.Size(); i++){
            m_eltType->JSONToBinary(value[i], pBinary+off, cb, true);
            off+=cb;
        }
    }

    virtual rapidjson::Value binaryToJSON(const char *pBinary, unsigned cbBinary, rapidjson::Document::AllocatorType &alloc) const override
    {
        rapidjson::Value res(rapidjson::kArrayType);
        res.Reserve(m_eltCount, alloc);

        unsigned cb=m_eltType->getPayloadSize();

        unsigned lastNonDefault=m_eltCount;
        while(0 < lastNonDefault){
            if(!m_eltType->isBinaryDefault(pBinary+((lastNonDefault-1)*cb), cb)){
                break;
            }
            lastNonDefault--;
        }

        unsigned off=0;
        
        for(unsigned i=0; i<lastNonDefault; i++)
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


    virtual void binaryToXmlV4Value(const char *pBinary, unsigned cbBinary, std::ostream &dst, int formatMinorVersion) const
    {
        if(cbBinary != getPayloadSize() ){
            throw std::runtime_error("binaryToXmlV4Value - Invalid binary size.");
        }
        if(formatMinorVersion!=0){
            throw std::runtime_error("formatMinorVersion!=0");
        }

        dst<<'{';

        unsigned off=0;
        unsigned cb=m_eltType->getPayloadSize();
        for(unsigned i=0; i<m_eltCount; i++)
        {
            if(off!=0){
                dst<<',';
            }
            m_eltType->binaryToXmlV4Value(pBinary+off, cb, dst, formatMinorVersion);
            off += cb;
        }

        dst<<'}';
    }

    virtual void xmlV4ValueToBinary(std::istream &src, char *pBinary, unsigned cbBinary, bool alreadyDefaulted, int formatMinorVersion) const
    {
        if(cbBinary != getPayloadSize() ){
            throw std::runtime_error("binaryToXmlV4Value - Invalid binary size.");
        }
        if(formatMinorVersion!=0){
            throw std::runtime_error("formatMinorVersion!=0");
        }

        auto expect=[&](char value)
        {
            char got;
            if(!(src>>got)){
                throw std::runtime_error("xmlV4ValueToBinary - Couldn't read char while parsing tuple value.");
            }
            if(got!=value){
                throw std::runtime_error("xmlV4ValueToBinary - Got unexpected char while parsing tuple value");
            }
        };

        expect('{');

        unsigned off=0;
        unsigned cb=m_eltType->getPayloadSize();
        for(unsigned i=0; i<m_eltCount; i++){
            if(off!=0){
                expect(',');
            }

            m_eltType->xmlV4ValueToBinary(src, pBinary+off, m_eltType->getPayloadSize(), alreadyDefaulted, formatMinorVersion);
            off += cb;
        }

        expect('}');
    }

    bool check_is_equivalent(TypedDataSpecElement *other, bool throw_on_error, const std::string &error_prefix) const
    {
        auto tother = dynamic_cast<TypedDataSpecElementArray*>(other);
        if(!tother){
            if(throw_on_error){
                throw graph_type_mismatch_error(error_prefix + "Other element is not an array.");
            }
            return false;
        }

        if(getElementCount()!=tother->getElementCount()){
            if(throw_on_error){
                throw std::runtime_error(error_prefix + "Expected length of "+std::to_string(getElementCount())+", but got "+std::to_string(tother->getElementCount()));
            }
            return false;
        }

        if(!getElementType()->check_is_equivalent(tother->getElementType().get(), true, error_prefix+"::[...]")){
            return false;
        }

        return true;
    }

    virtual void dumpStructure(std::ostream &dst, const std::string &indent) const
    {
        dst<<indent<<"<Array name='"<<getName()<<"' length='"<<getElementCount()<<"' >\n";
        getElementType()->dumpStructure(dst,indent+"  ");
        dst<<indent<<"</Array>\n";
    }

    sub_element_position findSubElementPosition(const std::string &path) const override
    {
        if(path.empty()){
            return {
                this,
                0,
                m_eltCount * m_eltType->getPayloadSize()
            };
        }

        if(path[0]!='['){
            throw std::runtime_error("Array path doesnt start with [");
        }

        auto epos=path.find(']');
        if(epos==std::string::npos){
            throw std::runtime_error("Array path doesnt contain ]");
        }
        std::string index_s=path.substr(1,epos);
        std::string next=path.substr(epos+1);

        if(index_s.empty()){
            throw std::runtime_error("No index chars into array.");
        }

        size_t end=0;
        unsigned long index=std::stoul(index_s, &end);
        if(end!=index_s.size()){
            throw std::runtime_error("Coulndt parse array index as integer.");
        }
        if(end >= m_eltCount){
            throw std::runtime_error("Array index out of range.");
        }

        size_t offset=index*m_eltType->getPayloadSize();
        auto res=m_eltType->findSubElementPosition(next);
        res.offset+=offset;

        return res;        
    }
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

inline TypedDataSpecElementScalarPtr makeScalar(const std::string &name, const std::string &type, const std::string &defaultValue=std::string())
{
    return std::make_shared<TypedDataSpecElementScalar>(name, type, defaultValue);
}

inline TypedDataSpecElementTuplePtr makeTuple(const std::string &name, const std::initializer_list<TypedDataSpecElementPtr> &elts)
{
    return std::make_shared<TypedDataSpecElementTuple>(name, elts.begin(), elts.end());
}

template<class TIt>
inline TypedDataSpecElementTuplePtr makeTuple(const std::string &name, TIt begin, TIt end)
{
    return std::make_shared<TypedDataSpecElementTuple>(name, begin, end);
}


inline TypedDataSpecElementArrayPtr makeArray(const std::string &name, unsigned n, TypedDataSpecElementPtr elt)
{
    return std::make_shared<TypedDataSpecElementArray>(name, n, elt);
}

#endif
