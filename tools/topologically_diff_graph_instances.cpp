#define RAPIDJSON_HAS_STDSTRING 1
#include "graph_dom.hpp"

#include "xml_pull_parser.hpp"
#include "graph_provider_helpers.hpp"

int main(int argc, char *argv[])
{
  try{
    filepath refSrcFileName("-");
    filepath otherSrcFileName("");

    if(argc>1){
      refSrcFileName=std::string(argv[1]);
    }

    if(argc>2){
      otherSrcFileName=std::string(argv[2]);
    }

    RegistryImpl registry;

    GraphDOMBuilder builder_ref;
    loadGraphPull(&registry, refSrcFileName, &builder_ref);
    GraphDOM ref{std::move(builder_ref.g)};

    GraphDOMBuilder builder_got;
    loadGraphPull(&registry, otherSrcFileName, &builder_got);
    GraphDOM got{std::move(builder_got.g)};

    int differences=0;

    auto ref_devices=ref.get_sorted_devices();
    auto got_devices=got.get_sorted_devices();

    if(ref_devices.size()!=got_devices.size()){
      fprintf(stderr, "Ref has %u devices, while got has %u\n", ref_devices.size(), got_devices.size());
      differences++;
    }

    auto diff_data=[&](const TypedDataSpecPtr &spec, TypedDataPtr ref, TypedDataPtr got, const std::string &thing, const char *attr)
    {
      if(!ref && !got){
        return;
      }
      if(!ref){
        ref=spec->create();
      }
      if(!got){
        got=spec->create();
      }
      if(ref.payloadSize()!=spec->payloadSize()){
        throw std::runtime_error("< Invalid typed data size on "+thing+" "+attr);
      }
      if(ref.payloadSize()!=spec->payloadSize()){
        throw std::runtime_error("> Invalid typed data size on "+thing+" "+attr);
      }

      if(ref.payloadSize()==0){
        return;
      }

      if(memcmp(ref.payloadPtr(), got.payloadPtr(), ref.payloadSize())){
        fprintf(stderr, "<> Thing %s %s\n", thing.c_str(), attr);
        fprintf(stderr, "   < %s`\n", spec->toJSON(ref).c_str());
        fprintf(stderr, "   > %s`\n", spec->toJSON(got).c_str());
        differences++;
      }
    };

    auto c_ref=ref.devices.begin(), e_ref=ref.devices.end();
    auto c_got=got.devices.begin(), e_got=got.devices.end();

    while(c_ref!=e_ref && c_got!=e_got){
      if(c_ref->first < c_got->first){
        fprintf(stderr, "< device %s\n", c_ref->first.c_str());
        ++c_ref;
        ++differences;
      }else if(c_ref->first > c_got->first){
        fprintf(stderr, "> device %s\n", c_got->first.c_str());
        ++c_got;
        ++differences;
      }else{
        fprintf(stderr, "= device %s\n", c_ref->first.c_str());
        diff_data(c_ref->second.type->getPropertiesSpec(), c_ref->second.properties, c_got->second.properties, c_ref->first, "properties");
        diff_data(c_ref->second.type->getStateSpec(), c_ref->second.state, c_got->second.state, c_ref->first, "state");
        ++c_got;
        ++c_ref;
      }
    }
    
    return differences==0 ? 0 : 1;
  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
