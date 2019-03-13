#ifndef external_device_proxy_hpp
#define external_device_proxy_hpp

#include "graph.hpp"
#include "external_connection.hpp"


/* This is a fake device type, with fiddled send and receive handlers that
    can be redirected at load-time.
    Apart from the send and receive handlers changing, it should look the
    same as the original.    
*/
class ExternalDeviceImpl
  : public DeviceTypeDelegate
{
public:
    typedef std::function<void (
      OrchestratorServices *orchestrator,
      const typed_data_t *graphProperties,
      const typed_data_t *deviceProperties,
      unsigned recvDeviceAddress,
      const typed_data_t *edgeProperties,
      unsigned recvPortIndex,
      const typed_data_t *message
    )> on_recv_t;

    typedef std::function<void (
        OrchestratorServices *orchestrator,
        const typed_data_t *graphProperties,
        const typed_data_t *deviceProperties,
        unsigned sendDeviceAddress, 
        unsigned sendPortIndex, // This is needed to intelligently dispatch
        typed_data_t *message,
        bool *doSend,
        unsigned *sendIndex
    )> on_send_t;
    
    typedef std::function<uint32_t (
        OrchestratorServices *orchestrator,
        const typed_data_t *graphProperties,
        const typed_data_t *deviceProperties,
        unsigned deviceAddress
    )> on_rts_t;

private:
    // The address of the device this is a proxy for
    unsigned m_deviceAddress;

    on_send_t m_onSend;
    on_recv_t m_onRecv;
    on_rts_t m_onRTS;
  
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
      ) const override
      {
        m_device->m_onRecv(orchestrator, graphProperties, deviceProperties,
            m_device->m_deviceAddress,
            edgeProperties,
            getBase()->getIndex(),
            message
        );
      }
  };

  class ExternalOutputPinImpl
    : public OutputPinDelegate
  {
  public:
    ExternalOutputPinImpl(OutputPinPtr pin, ExternalDeviceImpl *device)
      : OutputPinDelegate(pin)
      , m_device(device)
    {}

    ExternalDeviceImpl *m_device;

    virtual void onSend(OrchestratorServices *orchestrator,
          const typed_data_t *graphProperties,
          const typed_data_t *deviceProperties,
          typed_data_t *deviceState,
          typed_data_t *message,
          bool *doSend,
          unsigned *sendIndex
    ) const override
    {
      m_device->m_onSend(orchestrator, graphProperties, deviceProperties,
        m_device->m_deviceAddress,
        getBase()->getIndex(),
        message, doSend, sendIndex
     );
    }

  };

  std::vector<InputPinPtr> m_inputsByIndex;
  std::map<std::string,std::shared_ptr<ExternalInputPinImpl> > m_inputsByName;

  std::vector<OutputPinPtr > m_outputsByIndex;
  std::map<std::string,std::shared_ptr<ExternalOutputPinImpl> > m_outputsByName;
public:
  ExternalDeviceImpl(
    DeviceTypePtr externalType,                      // "real" type of the external  
    unsigned deviceAddress,
    on_send_t onSend,
    on_recv_t onRecv,
    on_rts_t onRTS
  ) 
    : DeviceTypeDelegate(externalType)
    , m_deviceAddress(deviceAddress)
    , m_onSend(onSend)
    , m_onRecv(onRecv)
    , m_onRTS(onRTS)
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

  virtual uint32_t calcReadyToSend(OrchestratorServices *orchestrator, const typed_data_t *graphProperties, const typed_data_t *deviceProperties, const typed_data_t *deviceState) const override
  {
    return m_onRTS(orchestrator, graphProperties, deviceProperties, m_deviceAddress);
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

  virtual void init(
           OrchestratorServices *orchestrator,
           const typed_data_t *graphProperties,
           const typed_data_t *deviceProperties,
           typed_data_t *deviceState
  ) const {
    return; // Externals have no meaningful init
  }
};

#endif
