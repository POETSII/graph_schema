#ifndef epoch_sim_hpp
#define epoch_sim_hpp

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
