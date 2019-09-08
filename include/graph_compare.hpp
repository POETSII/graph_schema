#ifndef graph_compare_hpp
#define graph_compare_hpp

#include "graph_core.hpp"

bool check_typed_data_specs_structurally_similar(
    const std::string &prefix, TypedDataSpecPtr ref, TypedDataSpecPtr got, bool throw_on_mismatch
){
    if(!ref){
        // Check that got is not present or is empty
        if(got){
            auto right=got->getTupleElement();
            if(right->size()!=0){
                throw graph_type_mismatch_error(prefix + " reference type is empty, but other type contains elements.");
            }
        }
        return true;
    }
    if(!got){
        // Check that ref is not present or is empty
        if(ref){
            auto left=ref->getTupleElement();
            if(left->size()!=0){
                throw graph_type_mismatch_error(prefix + " reference type is not empty, but other type is empty.");
            }
        }
        return true;
    }

    auto left=ref->getTupleElement();
    auto right=got->getTupleElement();

    try{
        return left->check_is_equivalent(right.get(), throw_on_mismatch, prefix);
    }catch(...){
        std::cerr<<"Reference type = \n";
        left->dumpStructure(std::cerr, "  ");   
        std::cerr<<"Other type = \n";
        right->dumpStructure(std::cerr, "  ");
        throw;
    }
}


bool check_graph_types_structurally_similar(
    GraphTypePtr ref,
    GraphTypePtr got,
    bool throw_on_mismatch
){
    auto mismatch = [&](const std::string &msg) -> bool
    {
        if(throw_on_mismatch){
            throw graph_type_mismatch_error(msg.c_str());
        }
        return false;
    };

    if(ref->getId()!=got->getId()){
        mismatch("Expected graph type id '"+ref->getId()+"' but got "+got->getId());
    }

    if(ref->getMessageTypeCount() != got->getMessageTypeCount()){
        return mismatch("Expected "+std::to_string(ref->getMessageTypeCount())+" message types, but got "+std::to_string(got->getMessageTypeCount()));
    }

    for(auto rmt : ref->getMessageTypes()){
        auto gmt=got->getMessageType(rmt->getId());
        if(!gmt){
            return mismatch("Expected message type '"+rmt->getId()+"' is not present in other graph.");
        }

        if(!check_typed_data_specs_structurally_similar("Mismatch on message type '"+rmt->getId()+"' message spec ",
                rmt->getMessageSpec(), gmt->getMessageSpec(), throw_on_mismatch)
        ){
            return false;
        }
    }

    if(ref->getDeviceTypeCount() != got->getDeviceTypeCount()){
        return mismatch("Expected "+std::to_string(ref->getDeviceTypeCount())+" device types, but got "+std::to_string(got->getDeviceTypeCount()));
    }

    for(auto rdt : ref->getDeviceTypes()){
        auto gdt=got->getDeviceType(rdt->getId());
        if(!gdt){
            mismatch("Expected device type '"+rdt->getId()+"' is not present in other graph.");
        }

        if(!check_typed_data_specs_structurally_similar("Mismatch on device type '"+rdt->getId()+"' properties spec ",
                rdt->getPropertiesSpec(), gdt->getPropertiesSpec(), throw_on_mismatch)
        ){
            return false;
        }

        if(!check_typed_data_specs_structurally_similar("Mismatch on device type '"+rdt->getId()+"' state spec ",
                rdt->getStateSpec(), gdt->getStateSpec(), throw_on_mismatch)
        ){
            return false;
        }

        for(auto rip : rdt->getInputs()){
            auto gip=gdt->getInput(rip->getIndex());
            if(!gip){
                mismatch("Expected input pin "+rip->getName()+" on device type '"+rdt->getId()+"' is not present in other graph.");
            }

            if(!check_typed_data_specs_structurally_similar("Mismatch on device type '"+rdt->getId()+"' input pin '"+rip->getName()+"' properties spec ",
                    rip->getPropertiesSpec(), gip->getPropertiesSpec(), throw_on_mismatch)
            ){
                return false;
            }

            if(!check_typed_data_specs_structurally_similar("Mismatch on device type '"+rdt->getId()+"' input pin state spec ",
                    rip->getStateSpec(), gip->getStateSpec(), throw_on_mismatch)
            ){
                return false;
            }
        }

        for(auto rop : rdt->getOutputs()){
            auto gop=gdt->getOutput(rop->getIndex());
            if(!gop){
                mismatch("Expected output pin "+rop->getName()+" on device type '"+rdt->getId()+"' is not present in other graph.");
            }
        }
    }

    return true;
}

#endif
