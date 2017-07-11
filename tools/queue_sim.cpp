#include "graph.hpp"

#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <unordered_set>
#include <algorithm>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>

#include <boost/lockfree/queue.hpp>

#include <cstring>
#include <cstdlib>

static unsigned  logLevel=2;



struct QueueSim
  : public GraphLoadEvents
{
    static unsigned lmo(uint32_t x)
    {
      assert(x);
      unsigned r=0;
      if(! (x&0xFFFF) ){  r=r+16; x=x>>16; }
      if(! (x&0xFF) ){  r=r+8; x=x>>8; }
      if(! (x&0xF) ){  r=r+4; x=x>>4; }
      if(! (x&0x3) ){  r=r+2; x=x>>2; }
      if(! (x&0x1) ){  r=r+1; x=x>>1; }
      return r;
    }




  
  typedef std::mutex mutex_t;
  typedef std::unique_lock<mutex_t> lock_t;
  
  std::unordered_set<std::string> m_interned;

  // Return a stable C pointer to the name. Avoids us to store
  // pointers in the data structures, and avoid calling .c_str() everywhere
  const char *intern(const std::string &name)
  {
    auto it=m_interned.insert(name);
    return it.first->c_str();
  }

  struct broadcast_t
  {
    broadcast_t()
      : message(0)
    {}
    
    broadcast_t(unsigned _bid, TypedDataPtr _message) // Note that the message is by-value to ensure a reference
      : bid(_bid)
      , message(_message.detach())
    {}
    
    unsigned bid; // Represent a bundle of devices in the target queue
    typed_data_t *message;
  };

    struct edge_bundle_id_t
  {
    unsigned queue; // Index of the queue it goes to
    unsigned bid; // Index of the delivery batch within that queue
  };

  struct device_t;

  /*! This contains the information needed to deliver to a target */
  struct edge_t
  {
    device_t *device;
    InputPinPtr pin;
    TypedDataPtr state;
    TypedDataPtr properties;
    const char *pinName;
    const char *endpointName; // Endpoint name in id:pin format. Probably a waste of memory, shoudl construct on the fly
  };



  
  struct output_pin_t
  {
    const char *id; // Name in id:pin format. Probably a waste of memory, better to strcat in local buffer?
    OutputPinPtr pin;
    TypedDataSpecPtr spec; // Used to allocate messages
    std::vector<edge_bundle_id_t> batches; // Bundle to deliver to each queue.
    std::vector<edge_t> local; // edges to deliver to this queue
  };





  struct edge_bundle_t
  {
    unsigned bid;
    std::vector<edge_t> edges;
  };



  struct device_t
  {
    device_t(device_t &&) = default;
    
    device_t(const device_t &) = delete;
    device_t &operator=(const device_t &) = delete;

    device_t()
      : id(0)
      , owner(-1)
      , isOnReady(false)
      , readyIndex(-1)
      , readyPrev(0)
      , readyNext(0)
    {}
    
    const char *id;
    DeviceTypePtr type;
    TypedDataPtr properties;
    TypedDataPtr state;

    int owner; // Which queue currently has responsibility

    uint32_t rtsFlags;

    unsigned outputCount;
    std::vector<output_pin_t> outputs;
    
    // Each device is threaded onto a free list so that we can find
    // devices ready to send in O(1) time
    bool isOnReady;
    int readyIndex; // -1 means not ready, anything else is guaranteed to be a ready slot
    device_t *readyPrev;
    device_t *readyNext;
  };



  struct queue_t
  {
    uint64_t sentinelA=0x12345678;
    
    QueueSim *parent;
    
    unsigned index; // Unique index of the queue

    // Maps "sourceDevice:outputPin" -> bundleId
    std::unordered_map< const char* , unsigned > findBatchId;
    std::vector<edge_bundle_t> edge_bundles; // Batches of places to deliver messages to

    std::vector<device_t*> devices; // devices managed by this queue

    device_t *readyBegin;
    device_t *readyEnd;
    unsigned readyCount;

    std::shared_ptr<std::mutex> m_pMutex;
    //    std::queue<broadcast_t> m_broadcasts;
    std::unique_ptr<boost::lockfree::queue<broadcast_t> > m_broadcasts;

    uint64_t sentinelB=0x23456781;

    queue_t() = delete;
    queue_t(const queue_t &)=delete;
    queue_t &operator=(const queue_t &)=delete;

    queue_t(queue_t &&)=default;

    queue_t(QueueSim *_parent, unsigned _index)
      : parent(_parent)
      , index(_index)
      , readyBegin(0)
      , readyEnd(0)
      , readyCount(0)
      , m_pMutex(new mutex_t())
      , m_broadcasts(new boost::lockfree::queue<broadcast_t>(0))
    {    }

    ~queue_t()
    {

    }

    // Note: srcEndpoint _must_be interned
    edge_bundle_t &getEdgeBundle(const char *srcEndpoint, bool &newBundle)
    {
      unsigned bid;
      auto it=findBatchId.find(srcEndpoint);
      if(it!=findBatchId.end()){
	bid=it->second;
	newBundle=false;
      }else{
	bid=edge_bundles.size();
	edge_bundle_t eb;
	eb.bid=bid;
	edge_bundles.push_back(eb);
	findBatchId[srcEndpoint]=bid;
	newBundle=true;
      }
      return edge_bundles[bid];
    }
    

    // This can be called whether or no device is currently on list
    void readyAdd(device_t *device)
    {
      if(logLevel>4){
	fprintf(stderr, "      readyAdd(%s), count=%d\n", device->id, readyCount);
      }
      
      assert(!device->isOnReady);
      assert(device->readyIndex!=-1);
      assert( (device->rtsFlags >> (device->readyIndex) ) & 1);

      assert(device->readyPrev==0 && device->readyNext==0);
      assert( (readyBegin==0) == (readyEnd==0) );
      
      device->isOnReady=true;

      if(readyEnd==0){
	readyBegin=device;
	readyEnd=device;
      }else{
	device->readyPrev=readyEnd;
	assert(readyEnd->readyNext==0);
	readyEnd->readyNext=device;
	readyEnd=device;
      }

      readyCount++;
      
      assert( (readyBegin!=0) && (readyEnd!=0) );
    }

    // This should only be called if the device is known to be on ready list
    void readyRemove(device_t *device)
    {
      if(logLevel>4){
	fprintf(stderr, "      readyRem(%s), count=%d\n", device->id, readyCount);
      }
      
      assert(device->isOnReady);
      assert( (readyBegin!=0) );
      assert( (readyEnd!=0) );
      assert(readyCount>0);

      if(device->readyPrev==0){
	assert(readyBegin==device);
	readyBegin=device->readyNext;
      }else{
	device->readyPrev->readyNext = device->readyNext;
      }
      if(device->readyNext==0){
	assert(readyEnd==device);
	readyEnd=device->readyPrev;
      }else{
	device->readyNext->readyPrev = device->readyPrev;
      }

      readyCount--;

      device->readyPrev=0;
      device->readyNext=0;
      device->isOnReady=true;

      assert( (readyBegin==0) == (readyEnd==0) );
    }

    device_t *readyPop()
    {
      if(logLevel>4){
	fprintf(stderr, "      readyPop(head=%s), count=%d, totalQueued=%u\n", readyBegin ? readyBegin->id : "<null>", readyCount, (unsigned)parent->m_queuedMessages);
      }
      
      assert( (readyBegin==0) == (readyEnd==0) );
      
      device_t *device=readyBegin;
      if(device==0)
	return 0;

      assert(device->readyIndex!=-1);
      assert(device->isOnReady==true);
      assert((device->rtsFlags >> device->readyIndex) & 1);
      assert( (readyBegin!=0) && (readyEnd!=0) );

      readyBegin=readyBegin->readyNext;
      if(readyBegin==0){
	readyEnd=0;
	assert(readyCount==1);
      }
      readyCount--;

      device->isOnReady=false;
      device->readyNext=0;
      device->readyPrev=0;

      assert( (readyBegin==0) == (readyEnd==0) );

      return device;
    }

    void post(unsigned bid, const TypedDataPtr &message)
    {
      //      lock_t lock(*m_pMutex);
      //m_broadcasts.emplace(bid, message);
      assert(m_broadcasts);
      bool ok=m_broadcasts->push(broadcast_t{bid, message});
      if(!ok){
	fprintf(stderr, "Push failed.\n");
	exit(1);
      }
    }

    bool pop(unsigned &bid, TypedDataPtr &message)
    {
      
      //lock_t lock(*m_pMutex);
      /*if(m_broadcasts.empty())
	return false;
      
      bid=m_broadcasts.front().bid;
      message=m_broadcasts.front().message;
      m_broadcasts.pop();

      return true;
      */
      broadcast_t res;
      assert(m_broadcasts);
      if(!m_broadcasts->pop(res))
	return false;
      bid=res.bid;
      message.attach(res.message);
      return true;
    }

    void deliver(unsigned bid, const TypedDataPtr &message)
    {
    	assert(bid < edge_bundles.size());

	edge_bundle_t &bundle=edge_bundles[bid];

	deliver(bundle.edges, message);
    }


    void deliver(std::vector<edge_t> &edges, const TypedDataPtr &message)
    {
	const typed_data_t *graphPropertiesPtr=parent->m_graphProperties.get();
	
	ReceiveOrchestratorServicesImpl services(logLevel, stderr, 0, 0);

	for(edge_t &e : edges){
	  device_t *device=e.device;

	  services.setReceiver(device->id, e.pinName);
	  
	  e.pin->onReceive(&services, graphPropertiesPtr,
			  device->properties.get(), device->state.get(),
			  e.properties.get(), e.state.get(),
			  message.get()
			    );

	  device->rtsFlags = device->type->calcReadyToSend(&services,
						     graphPropertiesPtr,
						     device->properties.get(),
						     device->state.get()
						     );
						     

	  if( (device->rtsFlags!=0) == (device->readyIndex!=-1)){
	    // do nothing. It is eiter still ready or still not ready
	  }else if(device->readyIndex==-1){
	    // it was previously not ready
        assert(device->rtsFlags);
	    device->readyIndex = lmo(device->rtsFlags);
	    readyAdd(device);
	  }else{
	    // it was previously not ready
	    device->readyIndex = -1;
	    readyRemove(device);
	  }
	}
    }
    
    void send(device_t *device)
    {
      assert(device->readyIndex!=-1);
      assert( (device->rtsFlags >> device->readyIndex) & 1 );

      assert(device->readyIndex < (int)device->outputs.size());
      auto &output=device->outputs[device->readyIndex];

      SendOrchestratorServicesImpl services(logLevel, stderr, device->id, output.pin->getName().c_str());

      TypedDataPtr message=output.spec->create();
      bool doSend=true;
      output.pin->onSend(&services, parent->m_graphProperties.get(), device->properties.get(), device->state.get(), message.get(), &doSend);

      if(logLevel>3){
        fprintf(stderr, "  Send %s\n", device->id);
      }

      device->rtsFlags = device->type->calcReadyToSend(&services, parent->m_graphProperties.get(), device->properties.get(), device->state.get());

      if(device->rtsFlags==0){
        device->readyIndex=-1;
      }else{
        assert(device->rtsFlags);
        device->readyIndex=lmo(device->rtsFlags);
        readyAdd(device);
      }

      
      if(!doSend){
        if(logLevel>3){
          fprintf(stderr, "    Cancelled\n");
        }
        return;
      }

      parent->m_queuedMessages+=output.batches.size();

      // Send remote batches
      for(auto &x : output.batches){
	if(logLevel>3){
	  fprintf(stderr, "    post %u\n", x.bid);
	}
	parent->m_queues[x.queue].post(x.bid, message);
      }

      // deliver locally
      deliver(output.local, message);
    }
  };

  
  GraphTypePtr m_graphType;
  std::string m_id;
  TypedDataPtr m_graphProperties;

  // This is all the devices in one list. We use a list so that
  // pointers stay valid (!!! Idiot)
  std::list<device_t> m_devicesStg;

  std::vector<device_t *> m_devices;

  // Each queue is an isolation zone, which points into the main device list
  std::vector<queue_t> m_queues;

  // Points us towards a device given an id, and also transitively the owning queue
  // Strings _must_ be interned
  std::unordered_map<const char *,device_t*> m_idToDevice;

  // Incremented when something is added to queues. The variable
  // is not un-incremented until the message has been delivered
  // to all clients. So any arising messages will cause an increment
  // before this causes a decrement
  std::atomic<unsigned> m_queuedMessages;
  
  // Incremented whenever there is something in flight on any thread
  unsigned m_busyCount;
  mutex_t m_idleMutex;
  std::condition_variable m_idleCondition;

  bool m_quit;

  QueueSim(unsigned nQueues)
  {
    
    m_queuedMessages=0;
    for(unsigned i=0;i<nQueues;i++){
      m_queues.emplace_back(this, i);
    }
    m_quit=false;
  }

  void dump()
  {
    ReceiveOrchestratorServicesImpl services(0, stderr, 0, "__print__");
    for(auto *device : m_devices){
      auto print=device->type->getInput("__print__");
      if(print){
	    fprintf(stderr, "Dump\n");
	services.setReceiver(device->id, "__print__");
	print->onReceive(&services, m_graphProperties.get(), device->properties.get(), device->state.get(), 0, 0, 0);
      }
    }
  }
  
  void loop(int self)
  {
    queue_t &queue=m_queues.at(self);

    const typed_data_t *graphPropertiesPtr=m_graphProperties.get();

    // Deliver init to all devices we own. We have not started
    // despatching received messages yet, so this must be the
    // first thing they get
    for(auto *device : queue.devices){
      ReceiveOrchestratorServicesImpl services(logLevel, stderr, device->id, "__init__");
      
      if(logLevel>3){
        fprintf(stderr, "  queue %u: trying __init__ on %p=%s, type=%p\n", queue.index, device, device->id, device->type.get());
      }
      auto init=device->type->getInput("__init__");
      if(!init){
        if(logLevel>3){
          fprintf(stderr,"  No init on %s\n", device->id);
        }
      }else{
        if(logLevel>3){
          fprintf(stderr,"  Init %s\n", device->id);
        }

        init->onReceive(&services, graphPropertiesPtr, device->properties.get(), device->state.get(), 0, 0, 0);

      }
      device->rtsFlags = device->type->calcReadyToSend( &services, graphPropertiesPtr, device->properties.get(), device->state.get());
      if(device->rtsFlags){
        device->readyIndex=lmo(device->rtsFlags);
      }else{
        device->readyIndex=-1;
      }
      if(logLevel>3){
        fprintf(stderr,"  Initial RTS of %s = %x, index=%d\n", device->id, device->rtsFlags, device->readyIndex);
      }
      if(device->readyIndex!=-1){
        queue.readyAdd(device);
      }
    }

    uint32_t rng=0;

    unsigned steps=0;

    unsigned idleSteps=0;
    unsigned idleCheckDelta=16;

    while(!m_quit){
      if(logLevel>2){
	fprintf(stderr, "\n==========================================================\n");
	fprintf(stderr, "q = %u, steps = %u\n", queue.index, steps);
      }
      steps++;
      
      unsigned bid;
      TypedDataPtr message;

      if(queue.pop(bid, message)){
	queue.deliver(bid, message);
	m_queuedMessages--;
      }else{
	device_t *device=queue.readyPop();
	if(device){
	  assert(device->owner==self);
	  assert(device->readyIndex!=-1);
	  assert( (device->rtsFlags >> device->readyIndex) & 1 );

	  queue.send(device);
	  
	  for(unsigned i=0;i<64;i++){
	    device=queue.readyPop();
	    if(!device)
	      break;
	    queue.send(device);
	  }

	  // inefficient
	  m_idleCondition.notify_all();
	}else{
	  if(idleSteps < idleCheckDelta){
	    idleSteps++;
	    std::this_thread::yield();
	  }else{
	    lock_t idleLock(m_idleMutex);

	    if(logLevel>3){
	      fprintf(stderr, "  idle : %d\n", queue.index);
	    }
	    
	    m_busyCount--;
	    
	    if(m_busyCount==0 && m_queuedMessages==0){
	      m_idleCondition.notify_all();
	      if(logLevel>2){
		fprintf(stderr,"  queue exit : %d\n", queue.index);
	      }
	      return;
	    }
	    
	    m_idleCondition.wait_for(idleLock, std::chrono::milliseconds(100));
	  
	    m_busyCount++;

	    idleSteps=0;
	  }
	}
      }
    }
  }

  virtual uint64_t onBeginGraphInstance(const GraphTypePtr &graphType, const std::string &id, const TypedDataPtr &graphProperties) override
  {
    m_graphType=graphType;
    m_id=id;
    m_graphProperties=graphProperties;
    return 0;
  }

  unsigned chooseOwner(const char *id)
  {
    assert(id==intern(id));
    
    std::hash<const char *> hf;
    return hf(id) % m_queues.size();
  }

  virtual uint64_t onDeviceInstance(uint64_t gId, const DeviceTypePtr &dt, const std::string &id, const TypedDataPtr &deviceProperties) override
  {
    unsigned index=-1;
    
    {
      const char *pid=intern(id);
      
      unsigned owner=chooseOwner(pid);
      
      
      TypedDataPtr state=dt->getStateSpec()->create();
      device_t d;
      d.id=pid;
      d.type=dt;
      d.properties=deviceProperties;
      d.state=state;
      d.owner=owner;
      d.outputCount=dt->getOutputCount();
      d.rtsFlags=0;
      for(unsigned i=0; i<d.outputCount; i++){
	output_pin_t out;
	out.pin=dt->getOutput(i);
	std::string pid=id+":"+out.pin->getName();
	out.id=intern(pid.c_str());
	out.spec=out.pin->getMessageType()->getMessageSpec();
	d.outputs.push_back(out);
      }
      d.readyIndex=-1;
      d.isOnReady=false;
      d.readyPrev=0;
      d.readyNext=0;
      
      m_devicesStg.push_back(std::move(d));
      index=m_devicesStg.size()-1;
      device_t *device=&m_devicesStg.back();
      m_devices.push_back(device);
      
      m_idToDevice[d.id] = device;

      if(logLevel>2){
	fprintf(stderr, " q[%d].add(%p=%s), type=%p\n", d.owner, device, device->id, device->type.get());
      }
      m_queues.at(d.owner).devices.push_back(device);
   
    }


    return index;
  }

  void onEdgeInstance(uint64_t gId, uint64_t dstDevIndex, const DeviceTypePtr &dstDevType, const InputPinPtr &dstInput, uint64_t srcDevIndex, const DeviceTypePtr &srcDevType, const OutputPinPtr &srcOutput, const TypedDataPtr &properties) override
  {
    device_t *dstDevice=m_devices.at(dstDevIndex);
    device_t *srcDevice=m_devices.at(srcDevIndex);

    const char *dstEp=intern(dstDevice->id+std::string(":")+dstInput->getName());
    const char *srcEp=intern(srcDevice->id+std::string(":")+srcOutput->getName());
    
    edge_t edge;
    edge.endpointName=dstEp;
    edge.pinName=intern(dstInput->getName());
    edge.device=dstDevice;
    edge.pin=dstInput;
    edge.state=dstInput->getStateSpec()->create();
    edge.properties=properties;

    unsigned dstQueue=dstDevice->owner;
    unsigned srcQueue=srcDevice->owner;

    output_pin_t &out=srcDevice->outputs[srcOutput->getIndex()];
    
    if(dstQueue==srcQueue){
      // local
      out.local.push_back(edge);
    }else{
      // remote
      bool newBundle;
      auto &eb=m_queues[dstQueue].getEdgeBundle(srcEp, newBundle);
      eb.edges.push_back(edge); // Woo!
      
      if(newBundle){
	out.batches.emplace_back(edge_bundle_id_t{dstQueue, eb.bid});
      }
    }
  }


  void writeSnapshot(SnapshotWriter *dst, double orchestratorTime, unsigned sequenceNumber)
  {
    dst->startSnapshot(m_graphType, m_id.c_str(), orchestratorTime, sequenceNumber);

    throw std::runtime_error("Not implemented.");
    
    /*
    for(auto &dev : m_devices){
        dst->writeDeviceInstance(dev.type, dev.name, dev.state, dev.readyToSend.get());
	for(unsigned i=0; i<dev.inputs.size(); i++){
	  const auto &et=dev.type->getInput(i)->getEdgeType();
	  for(auto &slot : dev.inputs[i]){
	    dst->writeEdgeInstance(et, slot.id, slot.state, slot.firings, 0, 0);
	    slot.firings=0;
	  }
	}
	}*/
	  
    dst->endSnapshot();
  }

  double m_statsSends=0;
  unsigned m_epoch=0;

};


void usage()
{
  fprintf(stderr, "queue_sim [options] sourceFile?\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  --log-level n\n");
  fprintf(stderr, "  --max-steps n\n");
  fprintf(stderr, "  --snapshots interval destFile\n");
  fprintf(stderr, "  --prob-send probability\n");
  fprintf(stderr, "  --threads count (default is number of cpus).\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  try{

    std::string srcFilePath="-";

    std::string snapshotSinkName;
    unsigned snapshotDelta=0;

    unsigned statsDelta=1;

    int maxSteps=INT_MAX;

    double probSend=0.9;

    unsigned nQueues=std::thread::hardware_concurrency();
    if(nQueues==0)
      nQueues=1;

    int ia=1;
    while(ia < argc){
      if(!strcmp("--help",argv[ia])){
	usage();
      }else if(!strcmp("--log-level",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --log-level\n");
          usage();
        }
        logLevel=strtoul(argv[ia+1], 0, 0);
        ia+=2;
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
      }else if(!strcmp("--threads",argv[ia])){
        if(ia+1 >= argc){
          fprintf(stderr, "Missing argument to --threads\n");
          usage();
        }
        nQueues=strtoul(argv[ia+1], 0, 0);
        ia+=2;
      }else{
        srcFilePath=argv[ia];
        ia++;
      }
    }

    RegistryImpl registry;

    std::istream *src=&std::cin;
    std::ifstream srcFile;
    filepath srcPath(current_path());

    if(srcFilePath!="-"){
      filepath p(srcFilePath);
      p=absolute(p);
      if(logLevel>1){
        fprintf(stderr,"Reading from '%s' ( = '%s' absolute)\n", srcFilePath.c_str(), p.c_str());
      }
      srcFile.open(p.c_str());
      if(!srcFile.is_open())
        throw std::runtime_error(std::string("Couldn't open '")+p.native()+"'");
      src=&srcFile;
      srcPath=p.parent_path();    

    }

    xmlpp::DomParser parser;

    if(logLevel>1){
      fprintf(stderr, "Parsing XML\n");
    }
    parser.parse_stream(*src);
    if(logLevel>1){
      fprintf(stderr, "Parsed XML\n");
    }


    QueueSim graph(nQueues);

    loadGraph(&registry, srcPath, parser.get_document()->get_root_node(), &graph);
    if(logLevel>1){
      fprintf(stderr, "Loaded\n");
    }

    std::unique_ptr<SnapshotWriter> snapshotWriter;
    if(snapshotDelta!=0){
      snapshotWriter.reset(new SnapshotWriterToFile(snapshotSinkName.c_str()));
    }

    //graph.m_busyCount++;
    //graph.loop(0);

    graph.m_busyCount+=nQueues;
    std::vector<std::thread> threads;
    for(unsigned i=0; i<nQueues;i++){
      unsigned ii=i;
      threads.emplace_back([=,&graph](){ graph.loop(ii); });
    }

    for(unsigned i=0;i<nQueues;i++){
      threads[i].join();
    }
   
    
    graph.dump();

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
