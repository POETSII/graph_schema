#ifndef snapshots_hpp
#define snapshots_hpp

#include "graph.hpp"

#include "libxml/xmlwriter.h"

class SnapshotWriter
{
public:
    virtual ~SnapshotWriter()
    {}

    virtual void startSnapshot(
        const GraphTypePtr &graph,
        const char *id,
        double orchestratorTime,
        unsigned sequenceNumber
    ) =0;

    virtual void endSnapshot() =0;

    virtual void writeDeviceInstance
    (
     const DeviceTypePtr &dt,
     const char *id,
     const TypedDataPtr &state,
     const bool *readyToSendFlags
     ) =0;

    virtual void writeEdgeInstance
    (
     const EdgeTypePtr &dt,
     const char *id,
     const TypedDataPtr &state,
     uint64_t firings,
     unsigned nMessagesInFlight,
     const TypedDataPtr *pMessagesInFlight
     ) =0;
};


class SnapshotWriterToFile
  : public SnapshotWriter
{
private:
  const xmlChar *m_ns=(const xmlChar *)"http://TODO.org/POETS/virtual-graph-schema-v0";

  std::shared_ptr<xmlChar> toXmlStr(const char *v)
  { return std::shared_ptr<xmlChar>(xmlCharStrdup(v), free); }

  xmlTextWriterPtr m_dst;
public:
    SnapshotWriterToFile(const char *dst)
      : m_dst(0)
  {
    m_dst=xmlNewTextWriterFilename(dst, 0);
      xmlTextWriterSetIndent(m_dst, 1);
  }


  virtual ~SnapshotWriterToFile()
    {
        if(m_dst){
            xmlFreeTextWriter(m_dst);
            m_dst=0;
        }
    }

    virtual void startSnapshot(
        const GraphTypePtr &graph,
        const char *id,
        double orchestratorTime,
        unsigned sequence
    ) override
    {
        xmlTextWriterStartDocument(m_dst, NULL, NULL, NULL);
        xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar*)"GraphSnapshot", m_ns);
        xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"graphInstId", (const xmlChar *)id);
        xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"graphTypeid", (const xmlChar *)graph->getId().c_str());
        {
            std::stringstream tmp;
            tmp<<sequence;
            xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"sequenceNumber", (const xmlChar *)tmp.str().c_str());
        }
        {
            std::stringstream tmp;
            tmp<<orchestratorTime;
            xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"orchestratorTime", (const xmlChar *)tmp.str().c_str());
        }
    }

    virtual void endSnapshot() override
    {
        xmlTextWriterEndElement(m_dst);
        xmlTextWriterEndDocument(m_dst);
        xmlTextWriterFlush(m_dst);
    }

private:
    void writeTypedData(const TypedDataSpecPtr &spec, const TypedDataPtr &data, const char *eltName, bool writeIfEmpty)
    {
        if(!data){
            if(writeIfEmpty){
                xmlTextWriterWriteElement(m_dst, (const xmlChar *)eltName, (const xmlChar *)"");
            }
        }else{
            std::string json=spec->toJSON(data);
            if(json.empty() || json=="{}"){
                if(writeIfEmpty){
                    xmlTextWriterWriteElement(m_dst, (const xmlChar *)eltName, (const xmlChar *)"");
                }
            }else{
                assert(json.size()>2);
                json.erase(json.begin()); // get rid of {
                json.erase(json.end()-1); // get rid of }

                xmlTextWriterStartElement(m_dst, (const xmlChar*)eltName);
                xmlTextWriterWriteRaw(m_dst, (const xmlChar *)json.c_str());
                xmlTextWriterEndElement(m_dst);
            }
        }
    }
public:

    virtual void writeDeviceInstance
    (
     const DeviceTypePtr &dt,
     const char *id,
     const TypedDataPtr &state,
     const bool *readyToSendFlags
     ) override
    {
      unsigned numOutputs=dt->getOutputCount();
      if(numOutputs>32)
	throw std::runtime_error("Not supported.");
      uint32_t flags=0;
      for(unsigned i=0;i<numOutputs;i++){
	if(readyToSendFlags[i])
	  flags |= (1ul << i);
      }


      bool isStateDifferent=false;
      std::string stateJSON;
      
      if(state){
	stateJSON=dt->getStateSpec()->toJSON(state);
	if(stateJSON.size()>0){
	  assert(stateJSON.size() >= 2);
	  stateJSON=stateJSON.substr(0,stateJSON.size()-1);
	  stateJSON=stateJSON.substr(1);
	}
      }

      if(flags==0 && stateJSON.empty())
	return; // It has no interesting non-default properties or state
      
      xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar *)"DevS", NULL);

      if(flags!=0){
	xmlTextWriterWriteFormatAttribute(m_dst, (const xmlChar *)"rts", "%x", flags);
      }

      xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id);

      if(stateJSON.size()>0 && stateJSON!="{}"){	
	xmlTextWriterStartElement(m_dst, (const xmlChar *)"S");
	xmlTextWriterWriteRaw(m_dst, (const xmlChar *)stateJSON.c_str());
	xmlTextWriterEndElement(m_dst);
      }

      xmlTextWriterEndElement(m_dst);
    }

    virtual void writeEdgeInstance
    (
     const EdgeTypePtr &et,
     const char *id,
     const TypedDataPtr &state,
     uint64_t firings,
     unsigned nMessagesInFlight,
     const TypedDataPtr *pMessagesInFlight
     ) override
    {
      xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar *)"EdgeS", m_ns);

      xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id);

        if(firings!=0){
	  xmlTextWriterWriteFormatAttribute(m_dst, (const xmlChar *)"firings", "%llx", firings);
        }

        writeTypedData(et->getStateSpec(), state, "S", false);

        if(nMessagesInFlight>0){
            xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar *)"Q", m_ns);
            for(unsigned i=0; i<nMessagesInFlight; i++){
                auto message=pMessagesInFlight[i];
                writeTypedData(et->getMessageSpec(), state, "M", true);
            }
            xmlTextWriterEndElement(m_dst);
        }

        xmlTextWriterEndElement(m_dst);
    }

};

#endif
