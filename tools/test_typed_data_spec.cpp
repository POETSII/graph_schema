#include "typed_data_spec.hpp"

#include <limits>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"


template<class T>
void test_scalar(const char *name)
{
    auto ts=std::make_shared<TypedDataSpecElementScalar>("x", name);

    assert( ts->getPayloadSize() == sizeof(T) );

    T tmp=1;

    ts->createBinaryDefault((char*)&tmp, sizeof(T));
    assert(tmp==0);
    
    rapidjson::Document doc;
    auto v=ts->binaryToJSON((char*)&tmp, sizeof(T), doc.GetAllocator());
    assert(v.IsNumber());
    assert(v.GetDouble()==0);

    tmp=10;
    v=ts->binaryToJSON((char*)&tmp, sizeof(T), doc.GetAllocator());
    assert(v.IsNumber());
    assert(v.GetDouble()==10);

    rapidjson::Value v0(0);
    ts->JSONToBinary(v0, (char*)&tmp, sizeof(T), false);
    assert(tmp==0);
    
    rapidjson::Value v10(10);
    ts->JSONToBinary(v10, (char*)&tmp, sizeof(T), false);
    assert(tmp==10);

    T vmax=std::numeric_limits<T>::max();
    v=ts->binaryToJSON((char*)&vmax, sizeof(T), doc.GetAllocator());
    ts->JSONToBinary(v, (char*)&tmp, sizeof(T));
    assert(tmp==vmax);

    T vmaxsub1=vmax-1;
    v=ts->binaryToJSON((char*)&vmaxsub1, sizeof(T), doc.GetAllocator());
    ts->JSONToBinary(v, (char*)&tmp, sizeof(T));
    assert(tmp==vmaxsub1);

    T vmin=std::numeric_limits<T>::lowest();
    v=ts->binaryToJSON((char*)&vmin, sizeof(T), doc.GetAllocator());
    ts->JSONToBinary(v, (char*)&tmp, sizeof(T));
    assert(tmp==vmin);


    auto ts2=std::make_shared<TypedDataSpecElementScalar>("x", name, "1");

    assert( ts->getPayloadSize() == sizeof(T) );

    tmp=0;

    ts2->createBinaryDefault((char*)&tmp, sizeof(T));
    assert(tmp==1);
}

template<unsigned N, class T>
void test_array(const char *eltType)
{
    auto elt=std::make_shared<TypedDataSpecElementScalar>("_", eltType);

    auto ts=std::make_shared<TypedDataSpecElementArray>("t", N, elt);

    assert( ts->getPayloadSize() == sizeof(T)*N );

    T tmp[N]={1};

    ts->createBinaryDefault((char*)&tmp[0], sizeof(T)*N);
    for(unsigned i=0; i<N; i++){
        assert(tmp[i]==0);
    }

    rapidjson::Document doc;
    auto v=ts->binaryToJSON((char*)&tmp[0], sizeof(T)*N, doc.GetAllocator());
    assert( v.IsArray() );
    assert( v.Size() == N );
    for(unsigned i=0; i<N; i++){
        assert( v[i].GetDouble() == 0 );
    }

    for(unsigned i=0; i<N; i++){
        tmp[i]=i+1;
    }
    v=ts->binaryToJSON((char*)&tmp[0], sizeof(T)*N, doc.GetAllocator());
    assert( v.IsArray() );
    assert( v.Size() == N );
    for(unsigned i=0; i<N; i++){
        assert( v[i].GetDouble() == (T)(i+1) );
    }

    for(unsigned i=0; i<N; i++){
        v[i].SetDouble(3u+i);
    }
    ts->JSONToBinary(v, (char*)&tmp[0], sizeof(T)*N);
    for(unsigned i=0; i<N; i++){
        assert(tmp[i] == (T)(3+i));
    }
}

std::string str(const rapidjson::Value &v)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    v.Accept(writer);
    return std::string(buffer.GetString());
}

void test_object_empty()
{
    std::vector<TypedDataSpecElementPtr> elts;
    
    auto ts=std::make_shared<TypedDataSpecElementTuple>("x", elts.begin(), elts.end());

    std::vector<char> tmp;

    rapidjson::Document doc;
    auto v=ts->binaryToJSON(&tmp[0], 0, doc.GetAllocator());
    assert(v.IsObject());
    assert(v.MemberCount()==0);

    ts->JSONToBinary(v, &tmp[0], 0);
}

template<class T0>
void test_object_single(const char *n0)
{
    std::vector<TypedDataSpecElementPtr> elts;
    elts.push_back(std::make_shared<TypedDataSpecElementScalar>("x", n0, "10"));
    
    auto ts=std::make_shared<TypedDataSpecElementTuple>("x", elts.begin(), elts.end());

    T0 tmp=1;

    rapidjson::Document doc;
    auto v=ts->binaryToJSON((char*)&tmp, sizeof(T0), doc.GetAllocator());
    std::cerr<<str(v)<<"\n";
    assert(v.IsObject());
    assert(v.MemberCount()==1);
    assert(v.HasMember("x"));
    assert(v["x"].IsNumber());
    assert(v["x"].GetDouble()==1);

    v["x"]=2;
    ts->JSONToBinary(v, (char*)&tmp, sizeof(T0));
    assert(tmp==2);
    
    v.RemoveMember("x");
    ts->JSONToBinary(v, (char*)&tmp, sizeof(T0));
    assert(tmp==10); // The default
}


template<class T0, class T1>
void test_object_dual(const char *n0, const char *n1)
{
    std::vector<TypedDataSpecElementPtr> elts;
    elts.push_back(std::make_shared<TypedDataSpecElementScalar>("x", n0, "10"));
    elts.push_back(std::make_shared<TypedDataSpecElementScalar>("y", n1, "11"));
    
    auto ts=std::make_shared<TypedDataSpecElementTuple>("x", elts.begin(), elts.end());

#pragma pack(push,1)
    struct {
        T0 x;
        T1 y;
    } tmp = { 1, 2 };
#pragma pack(pop)
   

    rapidjson::Document doc;
    auto v=ts->binaryToJSON((char*)&tmp, sizeof(T0), doc.GetAllocator());
    std::cerr<<str(v)<<"\n";
    assert(v.IsObject());
    assert(v.MemberCount()==2);
    assert(v.HasMember("x"));
    assert(v["x"].IsNumber());
    assert(v["x"].GetDouble()==1);
    assert(v.HasMember("y"));
    assert(v["y"].IsNumber());
    assert(v["y"].GetDouble()==2);

    v["x"]=66;
    ts->JSONToBinary(v, (char*)&tmp, sizeof(tmp));
    assert(tmp.x==66);

    v["y"]=67;
    ts->JSONToBinary(v, (char*)&tmp, sizeof(tmp));
    assert(tmp.y==67);
    
    v.RemoveMember("x");
    ts->JSONToBinary(v, (char*)&tmp, sizeof(tmp));
    assert(tmp.x==10); // The default

    v.RemoveMember("y");
    ts->JSONToBinary(v, (char*)&tmp, sizeof(tmp));
    assert(tmp.y==11); // The default
    
}

rapidjson::Document parse(const std::string &v)
{
    rapidjson::Document doc;
    doc.Parse(v.c_str());
    return doc;
}

void test_array_in_tuple()
{
    auto ts=makeTuple("t", {
            makeScalar("x", "int8_t"),
                makeArray("y", 4, makeScalar("_", "float")),
                makeScalar("z", "double")
                });

    assert(ts->getPayloadSize()==25);

#pragma pack(push,1)
    struct{
        char x;
        float y[4];
        double z;
    } tmp;
#pragma pack(pop)
    assert(sizeof(tmp)==ts->getPayloadSize());

    
    auto d1=parse("{}");
    ts->JSONToBinary(d1, (char*)&tmp, sizeof(tmp));
    assert(tmp.x==0);
    assert(tmp.y[0]==0);
    assert(tmp.z==0);

    auto d2=parse(R"({"x":1})");
    std::cerr<<"d2="<<str(d2)<<"\n";
    ts->JSONToBinary(d2, (char*)&tmp, sizeof(tmp));
    assert(tmp.x==1);
    assert(tmp.y[0]==0);
    assert(tmp.z==0);

    auto d3=parse(R"({"z":3})");
    std::cerr<<"d3="<<str(d3)<<"\n";
    ts->JSONToBinary(d3, (char*)&tmp, sizeof(tmp));
    assert(tmp.x==0);
    assert(tmp.y[0]==0);
    assert(tmp.z==3);

    auto d4=parse(R"({"y":[3,4,5,6]})");
    std::cerr<<"d4="<<str(d4)<<"\n";
    ts->JSONToBinary(d4, (char*)&tmp, sizeof(tmp));
    assert(tmp.x==0);
    assert(tmp.y[0]==3);
    assert(tmp.z==0);
}

int main()

{
    test_scalar<char>("char");
    test_scalar<float>("float");
    test_scalar<double>("double");
    test_scalar<uint8_t>("uint8_t");
    test_scalar<uint16_t>("uint16_t");
    test_scalar<uint32_t>("uint32_t");
    test_scalar<uint64_t>("uint64_t");
    test_scalar<int8_t>("int8_t");
    test_scalar<int16_t>("int16_t");
    test_scalar<int32_t>("int32_t");
    test_scalar<int64_t>("int64_t");

    test_array<1,char>("char");
    test_array<2,char>("char");
    test_array<3,char>("char");
    test_array<1,double>("double");
    test_array<2,double>("double");
    test_array<3,double>("double");

    test_object_empty();

    test_object_single<float>("float");
    test_object_single<double>("double");
    test_object_single<char>("char");
    test_object_single<uint64_t>("uint64_t");
    test_object_single<int64_t>("int64_t");

    test_object_dual<float,float>("float","float");
    test_object_dual<float,char>("float","char");
    test_object_dual<char,float>("char","float");
    
    test_array_in_tuple();
}

