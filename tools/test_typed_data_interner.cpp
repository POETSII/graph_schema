#include "typed_data_interner.hpp"
#include "graph_provider_helpers.hpp"

#include <limits>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

template<class T>
void test_scalar(const char *name, TypedDataInterner &interner)
{
    #pragma pack(push,1)
    struct this_data_t
        : typed_data_t
    {
        T val;
    };
    #pragma pack(pop)
    
    typedef DataPtr<this_data_t> ThisDataPtr;
    
    auto tse=std::make_shared<TypedDataSpecElementScalar>("x", name);
    auto ts=std::make_shared<TypedDataSpecImpl>(makeTuple("_",{tse}));
    
    ThisDataPtr defValue1=ts->create();
    ThisDataPtr defValue2=ts->create();
    
    assert(defValue1.payloadHash() == defValue2.payloadHash());
    assert(defValue1==defValue2);
    
    ThisDataPtr defValueInterned1=interner.intern(defValue1);
    assert(defValueInterned1==defValue1);
    
    ThisDataPtr defValueInterned2=interner.intern(defValue2);
    assert(defValueInterned2==defValue2);
    
    assert( defValueInterned1 == defValueInterned2 );
    assert( defValueInterned1.get() == defValueInterned2.get() );
    
    std::vector<ThisDataPtr> xx;
    
    for(unsigned r=0; r<4; r++){
        for(unsigned i=0; i<100; i++){
            ThisDataPtr v=ts->create();
            v->val=i;
            
            ThisDataPtr iv=interner.intern(v);
            
            if(xx.size()<=i){
                assert(iv==v && iv.get()!=v.get());
                xx.push_back(iv);
            }else{
                assert(xx[i].get() == iv.get());
                assert(iv->val==(T)i);
            }
            
            auto idx=interner.internToIndex(v);
            assert(iv==interner.indexToData(idx));
            assert(iv.payloadHash()==interner.indexToHash(idx));
        }
    }
}



int main()
{
    TypedDataInterner interner;
    
    for(int i=0; i<2; i++){    
        test_scalar<char>("char", interner);
        test_scalar<float>("float", interner);
        test_scalar<double>("double", interner);
        test_scalar<uint8_t>("uint8_t", interner);
        test_scalar<uint16_t>("uint16_t", interner);
        test_scalar<uint32_t>("uint32_t", interner);
        test_scalar<uint64_t>("uint64_t", interner);
        test_scalar<int8_t>("int8_t", interner);
        test_scalar<int16_t>("int16_t", interner);
        test_scalar<int32_t>("int32_t", interner);
        test_scalar<int64_t>("int64_t", interner);
    }
}

