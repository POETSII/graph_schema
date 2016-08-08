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
	const TypedDataPtr &state
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
        const TypedDataPtr &properties
    ) override
    {
        xmlTextWriterStartDocument(m_dst, NULL, NULL, NULL);
        xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar*)"GraphSnapshot", m_ns);
    }

    virtual void endSnapshot() override
    {
        xmlTextWriterEndElement(m_dst);
        xmlTextWriterEndDocument(m_dst);
        xmlTextWriterFlush(m_dst);
    }

    virtual void writeDeviceInstance
    (
     const DeviceTypePtr &dt,
     const char *id,
     const TypedDataPtr &state,
     const bool *readyToSendFlags
     ) override
    {
      xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar *)"DevS", m_ns);

      xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)"id", (const xmlChar *)id);

        unsigned numOutputs=dt->getOutputCount();
        if(numOutputs>32)
            throw std::runtime_error("Not supported.");
        uint32_t flags=0;
        for(unsigned i=0;i<numOutputs;i++){
            if(readyToSendFlags[i])
                flags |= (1ul << i);
        }
        if(flags!=0){
	  xmlTextWriterWriteFormatAttribute(m_dst, (const xmlChar *)"rts", "%x", flags);
        }

        if(state){
	  xmlTextWriterStartElementNS(m_dst,  NULL, (const xmlChar *)"S", m_ns);
            dt->getPropertiesSpec()->save(m_dst, state);
            xmlTextWriterEndElement(m_dst);
        }

        xmlTextWriterEndElement(m_dst);
    }

    virtual void writeEdgeInstance
    (
     const EdgeTypePtr &dt,
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

        if(state){
	  xmlTextWriterStartElementNS(m_dst,  NULL, (const xmlChar *)"S", m_ns);
            dt->getPropertiesSpec()->save(m_dst, state);
            xmlTextWriterEndElement(m_dst);
        }

        if(nMessagesInFlight>0){
	  xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar *)"Q", m_ns);
            for(unsigned i=0; i<nMessagesInFlight; i++){
	      xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar *)"M", m_ns);
                dt->getMessageSpec()->save(m_dst, pMessagesInFlight[i]);
                xmlTextWriterEndElement(m_dst);
            }
            xmlTextWriterEndElement(m_dst);
        }

        xmlTextWriterEndElement(m_dst);
    }

};

#endif
