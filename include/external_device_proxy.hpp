#ifndef external_device_proxy_hpp
#define external_device_proxy_hpp

#include "graph.hpp"
#include "external_connection.hpp"

struct external_edge_properties_t
  : typed_data_t
{
  unsigned dstDev;
  unsigned dstPort;
  unsigned srcDev;
  unsigned srcPort;
};

TypedDataPtr create_external_edge_properties(unsigned dstDev, unsigned dstPort, unsigned srcDev, unsigned srcPort )
{
  auto res=make_data_ptr<external_edge_properties_t>();
  res->dstDev=dstDev;
  res->dstPort=dstPort;
  res->srcDev=srcDev;
  res->srcPort=srcPort;
  return res;
}

/* This is a fake device type, with fiddled send and receive handlers that
  make them send to or receive from the external channel. */
class ExternalDeviceImpl
  : public DeviceTypeDelegate
{
private:
  std::shared_ptr<ExternalConnection> m_pExternal;
  std::vector<TypedDataPtr> m_outputSlots; // Data coming from the outstide to the inside
  
  struct ExternalInputPinImpl
    : public InputPinDelegate
  {
    ExternalInputPinImpl(InputPinPtr pin, ExternalDeviceImpl *device)
      : InputPinDelegate(pin)
      , m_device(device)
    {}

    ExternalDeviceImpl *m_device;


    virtual void onReceive(OrchestratorServices *orchestrator,
      const typed_data_t *graphProperties,
      const typed_data_t *deviceProperties,
      typed_data_t *deviceState,
      const typed_data_t *edgeProperties,
      typed_data_t *edgeState,
      const typed_data_t *message
      ) const
      {
        assert(edgeProperties);
        auto pEdgeInfo=(const external_edge_properties_t *)edgeProperties;

        external_message_t msg={
          false, // not multi-cast
          pEdgeInfo->dstDev, pEdgeInfo->dstPort,
          pEdgeInfo->srcDev, pEdgeInfo->srcPort,
          clone(message)
        };
        m_device->m_pExternal->write(msg);
      }
  };

  class ExternalOutputPinImpl
    : public OutputPinDelegate
  {
  public:
    ExternalOutputPinImpl(OutputPinPtr pin, ExternalDeviceImpl *device)
      : OutputPinDelegate(pin)
      , m_device(device)
      , m_index(pin->getIndex())
    {}

    ExternalDeviceImpl *m_device;
    unsigned m_index;

    virtual void onSend(OrchestratorServices *orchestrator,
          const typed_data_t *graphProperties,
          const typed_data_t *deviceProperties,
          typed_data_t *deviceState,
          typed_data_t *message,
          bool *doSend
    ) const
    {
      TypedDataPtr data=m_device->m_outputSlots[m_index];
      m_device->m_outputSlots[m_index].release();

      *doSend=false;
      if(data){
        data.copy_to(message);
        *doSend=true;
      }
    }

  };

  std::vector<InputPinPtr> m_inputsByIndex;
  std::map<std::string,std::shared_ptr<ExternalInputPinImpl> > m_inputsByName;

  std::vector<OutputPinPtr > m_outputsByIndex;
  std::map<std::string,std::shared_ptr<ExternalOutputPinImpl> > m_outputsByName;
public:
  ExternalDeviceImpl(
    std::shared_ptr<ExternalConnection> pExternal, // Where output comes from and goes too
    DeviceTypePtr externalType                      // "real" type of the external  
  ) 
    : DeviceTypeDelegate(externalType)
    , m_pExternal(pExternal)
  {
    if(!externalType->isExternal()){
      throw std::runtime_error("Attempt to create external device connection over non-external device.");
    }

    for(auto bi : externalType->getInputs()){
      auto pi=std::make_shared<ExternalInputPinImpl>(bi, this);
      m_inputsByIndex.push_back(pi);
      m_inputsByName[bi->getName()]=pi;
    }
    for(auto bo : externalType->getOutputs()){
      auto po=std::make_shared<ExternalOutputPinImpl>(bo, this);
      m_outputsByIndex.push_back(po);
      m_outputsByName[bo->getName()]=po;
    }
  }

  virtual uint32_t calcReadyToSend(OrchestratorServices *, const typed_data_t *, const typed_data_t *, const typed_data_t *) const override
  {
    uint32_t acc=0;
    for(unsigned i=0; i<m_outputSlots.size(); i++){
      if(m_outputSlots[i]){
        acc |= 1ul<<i;
      }
    }
    return acc;
  }

  virtual InputPinPtr getInput(const std::string &name) const override
  {
    auto it=m_inputsByName.find(name);
    if(it==m_inputsByName.end())
      return InputPinPtr();
    return it->second;
  }

  virtual const std::vector<InputPinPtr> &getInputs() const override
  { return m_inputsByIndex; }

  virtual OutputPinPtr getOutput(const std::string &name) const override
  {
    auto it=m_outputsByName.find(name);
    if(it==m_outputsByName.end())
      return OutputPinPtr();
    return it->second;
  }

  virtual const std::vector<OutputPinPtr> &getOutputs() const override
  { return m_outputsByIndex; }
};

#endif
