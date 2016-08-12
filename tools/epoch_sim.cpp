#include "graph_impl.hpp"

#include <libxml++/parsers/domparser.h>

#include "snapshots.hpp"

#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <unordered_set>
#include <algorithm>

#include <cstring>
#include <cstdlib>

static unsigned  logLevel=2;

struct EpochSim
  : public GraphLoadEvents
{
  std::unordered_set<std::string> m_interned;

  // Return a stable C pointer to the name. Avoids us to store
  // pointers in the data structures, and avoid calling .c_str() everywhere
  const char *intern(const std::string &name)
  {
    auto it=m_interned.insert(name);
    return it.first->c_str();
  }

  struct output
  {
    unsigned dstDevice;
    unsigned dstPortIndex;
    unsigned dstPortSlot; // This is the actual landing zone within the destination, i.e. where the state is
    const char *dstDeviceId;
    const char *dstInputName;
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
    const char *name; // interned id
    DeviceTypePtr type;
    TypedDataPtr properties;
    TypedDataPtr state;

    std::shared_ptr<bool> readyToSend; // Grrr, std::vector<bool> !

    std::vector<const char *> outputNames; // interned names

    std::vector<std::vector<output> > outputs;
    std::vector<std::vector<input> > inputs;

    bool anyReady() const
    { return std::any_of(readyToSend.get(),readyToSend.get()+type->getOutputCount(), [](bool b){ return b; }); }
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

  virtual uint64_t onDeviceInstance(uint64_t gId, const DeviceTypePtr &dt, const std::string &id, const TypedDataPtr &deviceProperties, const double */*nativeLocation*/) override
  {
    TypedDataPtr state=dt->getStateSpec()->create();
    device d;
    d.index=m_devices.size();
    d.id=id;
    d.name=intern(id);
    d.type=dt;
    d.properties=deviceProperties;
    d.state=state;
    d.readyToSend.reset(new bool[dt->getOutputCount()], [](bool *p){ delete[](p);} );
    d.outputs.resize(dt->getOutputCount());
    for(unsigned i=0;i<dt->getOutputCount();i++){
      d.readyToSend.get()[i]=false;
      d.outputNames.push_back(intern(dt->getOutput(i)->getName()));
    }
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
    o.dstDeviceId=m_devices.at(dstDevIndex).name;
    o.dstInputName=intern(dstInput->getName());
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

  void writeSnapshot(SnapshotWriter *dst, double orchestratorTime, unsigned sequenceNumber)
  {
    dst->startSnapshot(m_graphType, m_id.c_str(), orchestratorTime, sequenceNumber);

    for(const auto &dev : m_devices){
        dst->writeDeviceInstance(dev.type, dev.name, dev.state, dev.readyToSend.get());
    }

    dst->endSnapshot();
  }

  void init()
  {
    for(auto &dev : m_devices){
      auto init=dev.type->getInput("__init__");
      if(init){
	if(logLevel>2){
	  fprintf(stderr, "  init device %d = %s\n", dev.index, dev.id.c_str());
	}

	ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, dev.name, "__init__"  };
	init->onReceive(&receiveServices, m_graphProperties.get(), dev.properties.get(), dev.state.get(), 0, 0, 0, dev.readyToSend.get());
      }
    }
  }

  double m_statsSends=0;
  unsigned m_epoch=0;

  template<class TRng>
  bool step(TRng &rng)
  {
    // Within each step every object gets the chance to send exactly one message.

    ReceiveOrchestratorServicesImpl receiveServices{logLevel, stderr, 0, 0};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Send: ";
      receiveServices.setPrefix(tmp.str().c_str());
    }

    SendOrchestratorServicesImpl sendServices{logLevel, stderr, 0, 0};
    {
      std::stringstream tmp;
      tmp<<"Epoch "<<m_epoch<<", Recv: ";
      receiveServices.setPrefix(tmp.str().c_str());
    }



    bool sent=false;
    bool anyReady=false;

    unsigned rot=rng();
    for(unsigned i=0;i<m_devices.size();i++){
      unsigned index=(i+rot)%m_devices.size();

      auto &src=m_devices[index];

      if(logLevel>3){
	fprintf(stderr, "  step device %d = %s\n", src.index, src.id.c_str());
      }

      // Pick a random message
      unsigned sel=pick_bit(src.type->getOutputCount(), src.readyToSend.get(), rng());
      if(sel==-1){
	if(logLevel>3){
	  fprintf(stderr, "   not ready to send.\n");
	}
	continue;
      }

      if(logLevel>3){
	fprintf(stderr, "    output port %d ready\n", sel);
      }

      m_statsSends++;

      src.readyToSend.get()[sel]=false; // Up to them to re-enable

      OutputPortPtr output=src.type->getOutput(sel);
      TypedDataPtr message=output->getEdgeType()->getMessageSpec()->create();

      bool cancel=false;
      {
	sendServices.setSender(src.name, src.outputNames[sel]);
	output->onSend(&sendServices, m_graphProperties.get(), src.properties.get(), src.state.get(), message.get(), src.readyToSend.get(), &cancel);
      }

      if(cancel){
	if(logLevel>3){
	  fprintf(stderr, "    send aborted.\n");
	}
	anyReady = anyReady || src.anyReady();
	continue;
      }

      sent=true;

      for(auto &out : src.outputs.at(sel)){
	auto &dst=m_devices.at(out.dstDevice);
	auto &in=dst.inputs.at(out.dstPortIndex);
	auto &slot=in.at(out.dstPortSlot);

	if(logLevel>3){
	  fprintf(stderr, "    sending to device %d = %s\n", dst.index, dst.id.c_str());
	}

	auto port=dst.type->getInput(out.dstPortIndex);

	receiveServices.setReceiver(out.dstDeviceId, out.dstInputName);
	port->onReceive(&receiveServices, m_graphProperties.get(), dst.properties.get(), dst.state.get(), slot.properties.get(), slot.state.get(), message.get(), dst.readyToSend.get());

	anyReady = anyReady || dst.anyReady();
      }
    }

    ++m_epoch;
    return sent || anyReady;
  }


};


void usage()
{
  fprintf(stderr, "epoch_sim [options] sourceFile?\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  --max-steps n\n");
  fprintf(stderr, "  --snapshots interval destFile\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  try{

    std::string srcFilePath="-";

    std::string snapshotSinkName;
    unsigned snapshotDelta=0;

    unsigned statsDelta=1;

    unsigned maxSteps=INT_MAX;

    int ia=1;
    while(ia < argc){
      if(!strcmp("--help",argv[ia])){
	usage();
      }else if(!strcmp("--max-steps",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --max-steps\n");
          usage();
        }
        maxSteps=strtoul(argv[ia+1], 0, 0);
        ia+=2;
      }else if(!strcmp("--snapshots",argv[ia])){
        if(ia+2 >= argc){
          fprintf(stderr, "Missing two arguments to --snapshots interval destination \n");
          usage();
        }
        snapshotDelta=strtoul(argv[ia+1], 0, 0);
        snapshotSinkName=argv[ia+2];
        ia+=3;
      }else{
        srcFilePath=argv[ia];
        ia++;
      }
    }

    RegistryImpl registry;

    std::istream *src=&std::cin;
    std::ifstream srcFile;

    if(srcFilePath!="-"){
      if(logLevel>1){
        fprintf(stderr,"Reading from '%s'\n", srcFilePath.c_str());
      }
      srcFile.open(srcFilePath.c_str());
      if(!srcFile.is_open())
        throw std::runtime_error(std::string("Couldn't open '")+srcFilePath+"'");
      src=&srcFile;
    }

    xmlpp::DomParser parser;

    if(logLevel>1){
      fprintf(stderr, "Parsing XML\n");
    }
    parser.parse_stream(*src);
    if(logLevel>1){
      fprintf(stderr, "Parsed XML\n");
    }

    EpochSim graph;

    loadGraph(&registry, parser.get_document()->get_root_node(), &graph);
    if(logLevel>1){
      fprintf(stderr, "Loaded\n");
    }

    std::unique_ptr<SnapshotWriter> snapshotWriter;
    if(snapshotDelta!=0){
      snapshotWriter.reset(new SnapshotWriterToFile(snapshotSinkName.c_str()));
    }

    std::mt19937 rng;

    graph.init();

    if(snapshotWriter){
      graph.writeSnapshot(snapshotWriter.get(), 0.0, 0);
    }
    int nextStats=0;
    int nextSnapshot=snapshotDelta ? snapshotDelta-1 : -1;
    unsigned snapshotSequenceNum=1;

    for(unsigned i=0; i<maxSteps; i++){
      bool running = graph.step(rng);

      if(logLevel>2 || i==nextStats){
        fprintf(stderr, "Epoch %u : sends/device/epoch = %f (%f / %u)\n", i, graph.m_statsSends / graph.m_devices.size() / statsDelta, graph.m_statsSends/statsDelta, (unsigned)graph.m_devices.size());
      }
      if(i==nextStats){
        nextStats=nextStats+statsDelta;
        graph.m_statsSends=0;
      }

      if(snapshotWriter && i==nextSnapshot){
        graph.writeSnapshot(snapshotWriter.get(), i, snapshotSequenceNum);
        nextSnapshot += snapshotDelta;
        snapshotSequenceNum++;
      }

      if(!running){
        break;
      }
    }

    if(logLevel>1){
      fprintf(stderr, "Done\n");
    }

  }catch(std::exception &e){
    std::cerr<<"Exception : "<<e.what()<<"\n";
    exit(1);
  }catch(...){
    std::cerr<<"Exception of unknown type\n";
    exit(1);
  }

}
