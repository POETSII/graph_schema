
class TypedData
{
public:
  virtual void save(xmlpp::Element *dst) const =0;
  virtual void load(xmlpp::Element *src) =0;
};
typedef std::shared_ptr<TypedData> TypedDataPtr;

class TypedDataSpec
{
public:
  virtual TypedDataPtr createInstance()=0;
  virtual TypedDataPtr loadInstance(lxml *src)=0;
};
typedef std::shared_ptr<TypedDataSpec> TypedDataSpecPtr;

class EdgeType
{
public:
  virtual const char *getId() const=0;

  virtual const TypedDataSpecPtr &getPropertiesSpec() const=0;
  virtual const TypedDataSpecPtr &getStateSpec() const=0;
  virtual const TypedDataSpecPtr &getMessageSpec() const=0;
};
typedef std::shared_ptr<EdgeType> EdgeTypePtr;

class Port
{
public:
  virtual const DeviceType *getDeviceType() const=0;
  
  virtual const std::string &getName() const=0;
  virtual unsigned getIndex() const=0;

  virtual const EdgeTypePtr &getEdgeType() const=0;
};
typedef std::shared_ptr<Port> PortPtr;

class InputPort
  : public Port
{
public:
  virtual void onReceive(const TypedData *graphProperties,
			 const TypedData *deviceProperties,
			 TypedData *deviceState,
			 const TypedData *edgeProperties,
			 TypedData *edgeState,
			 bool *requestSendPerOutput
		      ) const=0;
};
typedef std::shared_ptr<InputPort> InputPortPtr;

class OutputPort
  : public Port
{
public:
  virtual void onSend(const TypedData *graphProperties,
		      const TypedData *deviceProperties,
		      TypedData *deviceState,
		      TypedData *message,
		      bool *requestSendPerOutput,
		      bool *abortThisSend
		      ) const=0;
};
typedef std::shared_ptr<OutputPort> OutputPortPtr;

class DeviceType
{
public:
  virtual const std::string &getId() const=0;

  virtual const TypedDataSpecPtr &getPropertiesSpec() const=0;
  virtual const TypedDataSpecPtr &getStateSpec() const=0;

  virtual unsigned getInputCount() const=0;
  virtual const InputPortPtr &getInput(unsigned index) const=0;
  virtual const InputPortPtr &getInput(const std::string &name) const=0;

  virtual unsigned getOutputCount() const=0;
  virtual const OutputPortPtr &getOutput(unsigned index) const=0;
  virtual const OutputPortPtr &getOutput(const std::string &name) const=0;
};

template<class TDeviceInstance>
class GraphLoadEvents
{
protected:
  virtual void onAddEdgeType(const EdgeTypePtr &et) =0;

  virtual void onAddDeviceType(const DeviceTypePtr &dt) =0;

  virtual TDeviceInstance onAddDeviceInstance
  (
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state
  ) =0;

  virtual void onAddEdgeInstance
  (
   TDeviceInstance dstDevInst, const DeviceTypePtr &dstDevType, const InputPortPtr &dstPort,
   TDeviceInstance srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPortPtr &srcPort,
   const TypedDataPtr properties,
   TypedDataPtr state
  ) =0;
};


class EpochSim
{
  struct output
  {
    unsigned dstDevice;
    unsigned dstPort;
    OutputPortPtr port;
  };

  struct input
  {
    TypedDataPtr properties;
    TypedDataPtr state;
    InputPortPtr port;
  };
  
  struct device
  {
    DeviceTypePtr type;
    TypedDataPtr properties;
    TypedDataPtr *state;

    std::shared_array<bool> readyToSend; // Grrr, std::vector<bool> !
    
    std::vector<std::vector<output> > outputs;
    std::vector<std::vector<input> > inputs;
  };

  std::vector<device> m_devices;

  unsigned pick_bit(unsigned n, const bool *bits, unsigned rot)
  {
    for(unsigned i=0;i<n;i++){
      unsigned s=(i+rot)%n;
      if(bits[s])
	return s;
    }
    return (unsigned)-1;
  }


  template<class TRng>
  void step(TRng &rng)
  {
    // Within each step every object gets the chance to send exactly one message.
    
    unsigned rot=rng();
    for(unsigned i=0;i<m_devices.size();i++){
      unsigned index=(i+rot)%m_devices.size();

      auto &src=m_devices[index];
      
      // Pick a random message
      unsigned sel=pick_bit(src.type->getOutputCount(), src.readyToSend.get(), rng());
      if(sel==-1)
	continue;

      src.readyToSend[sel]=false; // Up to them to re-enable

      auto &out=src.outputs[sel];

      TypedDataPtr message=out.port->getEdgeType()->getMessageSpec()->createInstance();

      bool cancel=false;
      out.port->onSend(m_graphProperties, src.properties, src.state, message, out.readyToSend, &cancel);

      if(cancel)
	continue;

      for(auto &out : di.outputs[sel]){
	auto &dst=m_devices.at(out.dstDevice);
	auto &in=dst.inputs.at(out.dstPort);

	dst.type->onReceive(m_graphProperties, dst.properties, dst.state, in.properties, in.state, message, dst.readyToSend);
      }
    }
  }

  
};
