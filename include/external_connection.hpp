#ifndef external_connection_hpp
#define external_connection_hpp

#include "graph.hpp"

#include <libxml++/document.h>

#include "rapidjson/filereadstream.h"


#include <queue>
#include <thread>

struct external_message_t
{
  bool isMulticast;
  unsigned dstDev;
  unsigned dstPort;
  unsigned srcDev;
  unsigned srcPort;
  TypedDataPtr data;
};

class ExternalConnection
{
private:
  
public:
  virtual void onExternalEdgeInstance(
    const char *dstDevId, unsigned dstDevAddress, const DeviceTypePtr &dstDevType, const InputPinPtr &dstInput,
    const char *srcDevId, unsigned srcDevAddress, const DeviceTypePtr &srcDevType, const OutputPinPtr &srcOutput
  )=0;


  virtual void startPump()=0;

  virtual bool canWrite() =0;

  /* A multi-cast message needs to be expanded on the receiving side, while uni-cast
  is just one message with a specified destination (dev,port) pair.*/
  virtual void write(
    const external_message_t &msg
  ) =0;

  virtual bool canRead() =0;

  /* A multi-cast messages needs to be fanned out to all devices within the
    simulation. A single-cast message has already been expanded. */
  virtual void read(
    external_message_t &msg
  ) =0;
};

class JSONExternalConnection
  : public ExternalConnection
{
private:
  FILE *m_dst;
  FILE *m_src;


  std::vector<std::pair<std::string,DeviceTypePtr> > m_deviceAddressToIdAndType;
  std::unordered_map<std::string,unsigned> m_deviceIdToAddress;

  // Need to get around blocking IO
  std::thread m_readerThread;
  bool m_readerThreadRunning;

  std::mutex m_mutex;
  std::queue<external_message_t> m_readerQueue;

public:
  JSONExternalConnection(FILE *src, FILE *dst)
    : m_dst(dst)
    , m_src(src)
    , m_readerThreadRunning(false)
  {
  }

  ~JSONExternalConnection()
  {
    // TODO: this is quite thread unsafe
    if(m_readerThreadRunning){
      m_readerThread.join();
    }
  }

  void startPump()
  {
    assert(!m_readerThreadRunning);

    if(m_src){
      m_readerThreadRunning=true;

      m_readerThread=std::thread([&](){
        auto checkJSON=[](bool cond, const char *msg){
          if(!cond){
            fprintf(stderr, "Received bad JSON from external : %s\n", msg);
            exit(1);
          }
        };
        auto getField=[&](rapidjson::Document &doc, const char *name) -> std::string {
          checkJSON(doc.HasMember(name), name);
          checkJSON(doc[name].IsString(), name);
          return doc[name].GetString();
        };

        xmlpp::Document xmlDoc;
        xmlpp::Element *xmlElt=xmlDoc.create_root_node("Bleh");

        char readBuffer[4096];
        rapidjson::FileReadStream is(m_src, readBuffer, sizeof(readBuffer));
        
        while(1){
          rapidjson::Document d;
          d.ParseStream<rapidjson::kParseStopWhenDoneFlag>(is);
          if(d.HasParseError()){
            if(d.GetParseError()==rapidjson::kParseErrorDocumentEmpty){
              m_readerThreadRunning=false;
              break; // End of stream
            }
            checkJSON(false, "Couldn't parse complete JSON out of stream.");
          }

          checkJSON(d.IsObject(), "message was not a JSON object." );
          external_message_t msg;
          std::string dstDevName=getField(d,"dstDev");
          std::string dstPortName=getField(d,"dstPort");
          std::string srcDevName=getField(d,"srcDev");
          std::string srcPortName=getField(d,"srcPort");
          checkJSON(d.HasMember("payload"), "message has no payload");
          checkJSON(d["payload"].IsObject(), "payload is not an object");

          auto dstIt=m_deviceIdToAddress.find(dstDevName);
          checkJSON(dstIt!=m_deviceIdToAddress.end(), "Destination port is unknown");
          msg.dstDev=dstIt->second;
          DeviceTypePtr dstType=m_deviceAddressToIdAndType[msg.dstDev].second;
          InputPinPtr dstPort=dstType->getInput(dstPortName);
          checkJSON(!!dstPort, "Destination port is unknown");
          msg.dstPort=dstPort->getIndex();

          auto srcIt=m_deviceIdToAddress.find(srcDevName);
          checkJSON(srcIt!=m_deviceIdToAddress.end(), "Source port is unknown");
          msg.srcDev=srcIt->second;
          DeviceTypePtr srcType=m_deviceAddressToIdAndType[msg.srcDev].second;
          InputPinPtr srcPort=srcType->getInput(srcPortName);
          checkJSON(!!srcPort, "Source port is unknown");
          msg.srcPort=srcPort->getIndex();

          // TODO: this is insane. We have to construct an XML element just to get
          // something to pass to the typed data spec...
          // Which means we have to turn it back into F'ing JSON!
          // Slow hand-clap for dt10!
          std::string payloadString;
          {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            d["payload"].Accept(writer);
            payloadString=buffer.GetString();
          }
          // WWWWooooooooooooooooooooo!!!!!
          xmlElt->set_child_text(payloadString);
          // YYYaaaaaaayyyyy!!!!!
          msg.data=srcPort->getMessageType()->getMessageSpec()->load(xmlElt);
          
          {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_readerQueue.push(msg);
          }
        }
      });
    }
  }

  void onExternalRelatedInstance(
    const DeviceTypePtr &dt, const std::string &id, unsigned devAddress
  )
  {
    auto it=m_deviceIdToAddress.find(id);
    if(it==m_deviceIdToAddress.end()){
      m_deviceIdToAddress.insert(it,std::make_pair(id,devAddress));
      if(devAddress >= m_deviceAddressToIdAndType.size()){
        m_deviceAddressToIdAndType.resize(devAddress+1);
      }
      m_deviceAddressToIdAndType[devAddress]=std::make_pair(id, dt);
    }
  }

  void onExternalEdgeInstance(
    const char *dstDevId, unsigned dstDevAddress, const DeviceTypePtr &dstDevType, const InputPinPtr &,
    const char *srcDevId, unsigned srcDevAddress, const DeviceTypePtr &srcDevType, const OutputPinPtr &
  )  override
  {
    onExternalRelatedInstance(dstDevType, dstDevId, dstDevAddress);
    onExternalRelatedInstance(srcDevType, srcDevId, srcDevAddress);
  }

  bool canWrite() override
  { return true; }

  virtual void write(
    const external_message_t &msg
  ) override
  {
    assert(!msg.isMulticast);

    const auto &dstInfo=m_deviceAddressToIdAndType[msg.dstDev];
    const auto &srcInfo=m_deviceAddressToIdAndType[msg.srcDev];
    const auto &dstPort=dstInfo.second->getInput(msg.dstPort);
    const auto &srcPort=srcInfo.second->getOutput(msg.srcPort);

    std::string payload=srcPort->getMessageType()->getMessageSpec()->toJSON(msg.data);
    
    fprintf(m_dst,
      R"({"dstDev":"%s","dstPort":"%s","srcPort":"%s","srcPort":"%s","payload"=%s})",
      dstInfo.first.c_str(), dstPort->getName().c_str(),
      srcInfo.first.c_str(), srcPort->getName().c_str(),
      payload.c_str()
    );
    fputc('\n', m_dst);
  }

  virtual bool canRead() override
  {
    std::unique_lock<std::mutex> lock_guard(m_mutex);
    return !m_readerQueue.empty();
  }

  virtual void read(external_message_t &msg) override
  {
    std::unique_lock<std::mutex> lock_guard(m_mutex);
    assert(!m_readerQueue.empty());
    msg=m_readerQueue.front();
    m_readerQueue.pop();
  }
};

#endif