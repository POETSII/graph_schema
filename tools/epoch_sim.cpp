#include "graph_impl.hpp"

#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <random>

struct EpochSim
  : public GraphLoadEvents
{
  struct output
  {
    unsigned dstDevice;
    unsigned dstPortIndex;
    unsigned dstPortSlot; // This is the actual landing zone within the destination, i.e. where the state is
  };

  struct input
  {
    TypedDataPtr properties;
    TypedDataPtr state;
  };
  
  struct device
  {
    unsigned index;
    std::string id;
    DeviceTypePtr type;
    TypedDataPtr properties;
    TypedDataPtr state;

    std::shared_ptr<bool> readyToSend; // Grrr, std::vector<bool> !
    
    std::vector<std::vector<output> > outputs;
    std::vector<std::vector<input> > inputs;
  };

  GraphTypePtr m_graphType;
  std::string m_id;
  TypedDataPtr m_graphProperties;
  std::vector<device> m_devices;
  

  virtual uint64_t onGraphInstance(const GraphTypePtr &graphType, const std::string &id, const TypedDataPtr &graphProperties) override
  {
    m_graphType=graphType;
    m_id=id;
    m_graphProperties=graphProperties;
    return 0;
  }

  virtual uint64_t onDeviceInstance(uint64_t gId, const DeviceTypePtr &dt, const std::string &id, const TypedDataPtr &deviceProperties) override
  {
    TypedDataPtr state=dt->getStateSpec()->create();
    device d;
    d.index=m_devices.size();
    d.id=id;
    d.type=dt;
    d.properties=deviceProperties;
    d.state=state;
    d.readyToSend.reset(new bool[dt->getOutputCount()], [](bool *p){ delete[](p);} );
    d.outputs.resize(dt->getOutputCount());
    d.inputs.resize(dt->getInputCount());
    m_devices.push_back(d);
    return d.index;
  }

  void onEdgeInstance(uint64_t gId, uint64_t dstDevIndex, const DeviceTypePtr &dstDevType, const InputPortPtr &dstInput, uint64_t srcDevIndex, const DeviceTypePtr &srcDevType, const OutputPortPtr &srcOutput, const TypedDataPtr &properties) override 
  {
    input i;
    i.properties=properties;
    i.state=dstInput->getEdgeType()->getStateSpec()->create();
    auto &slots=m_devices.at(dstDevIndex).inputs.at(dstInput->getIndex());
    unsigned dstPortSlot=slots.size();
    slots.push_back(i);

    output o;
    o.dstDevice=dstDevIndex;
    o.dstPortIndex=dstInput->getIndex();
    o.dstPortSlot=dstPortSlot;
    m_devices.at(srcDevIndex).outputs.at(srcOutput->getIndex()).push_back(o);
					      
  }
  

  unsigned pick_bit(unsigned n, const bool *bits, unsigned rot)
  {
    for(unsigned i=0;i<n;i++){
      unsigned s=(i+rot)%n;
      if(bits[s])
	return s;
    }
    return (unsigned)-1;
  }

  void init()
  {
    for(auto &dev : m_devices){
      auto init=dev.type->getInput("__init__");
      if(init){
	fprintf(stderr, "  init device %d = %s\n", dev.index, dev.id.c_str());      
	init->onReceive(m_graphProperties.get(), dev.properties.get(), dev.state.get(), 0, 0, 0, dev.readyToSend.get());	
      }
    }
  }

  template<class TRng>
  bool step(TRng &rng)
  {
    // Within each step every object gets the chance to send exactly one message.

    bool sent=false;
    
    unsigned rot=rng();
    for(unsigned i=0;i<m_devices.size();i++){
      unsigned index=(i+rot)%m_devices.size();

      auto &src=m_devices[index];

      fprintf(stderr, "  step device %d = %s\n", src.index, src.id.c_str());      
      
      // Pick a random message
      unsigned sel=pick_bit(src.type->getOutputCount(), src.readyToSend.get(), rng());
      if(sel==-1){
	fprintf(stderr, "   not ready to send.\n");
	continue;
      }

      fprintf(stderr, "    output port %d ready\n", sel);

      sent=true;

      src.readyToSend.get()[sel]=false; // Up to them to re-enable

      OutputPortPtr output=src.type->getOutput(sel);
      TypedDataPtr message=output->getEdgeType()->getMessageSpec()->create();

      bool cancel=false;
      output->onSend(m_graphProperties.get(), src.properties.get(), src.state.get(), message.get(), src.readyToSend.get(), &cancel);

      if(cancel){
	fprintf(stderr, "    send aborted.\n");
	continue;
      }

      for(auto &out : src.outputs.at(sel)){
	auto &dst=m_devices.at(out.dstDevice);
	auto &in=dst.inputs.at(out.dstPortIndex);
	auto &slot=in.at(out.dstPortSlot);

	fprintf(stderr, "    sending to device %d = %s\n", dst.index, dst.id.c_str());

	auto port=dst.type->getInput(out.dstPortIndex);

	port->onReceive(m_graphProperties.get(), dst.properties.get(), dst.state.get(), slot.properties.get(), slot.state.get(), message.get(), dst.readyToSend.get());
      }
    }
    return sent;
  }

  
};


int main(int argc, char *argv[])
{
  try{
    RegistryImpl registry;
    
    xmlpp::DomParser parser;
    
    std::istream *src=&std::cin;
    std::ifstream srcFile;
    
    if(argc>1){
      fprintf(stderr,"Reading from '%s'\n", argv[1]);
      srcFile.open(argv[1]);
      if(!srcFile.is_open())
	throw std::runtime_error(std::string("Couldn't open '")+argv[1]+"'");
      src=&srcFile;
      
    }

    fprintf(stderr, "Parsing XML\n");
    parser.parse_stream(*src);
    fprintf(stderr, "Parsed XML\n");

    EpochSim graph;
    
    loadGraph(&registry, parser.get_document()->get_root_node(), &graph);

    fprintf(stderr, "Loaded\n");

    std::mt19937 rng;

    graph.init();

    for(unsigned i=0; i<100; i++){
      fprintf(stderr, "Step %u\n", i);
      
      if(!graph.step(rng))
	break;

    }

    fprintf(stderr, "Done\n");

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
