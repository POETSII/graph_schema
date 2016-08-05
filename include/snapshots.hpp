#ifndef snapshots_hpp
#define snapshots_hpp

#include "graph.hpp"

#include "libxml2/xmlwriter.h"

class SnapshotWriter
{
public:
    virtual ~SnapshotWriter()
    {}

    virtual onBeginSnapshot(
        const GraphTypePtr &graph,
        const char *id
    ) =0;

    virtual onEndSnapshot() =0;

    virtual onDeviceInstance(
        const char *id,
        const TypedDataPtr &state,
        const bool *readyToSendFlags
    ) =0;

    virtual onEdgeInstance(
        const char *id,
        const TypedDataPtr &state,
        unsigned nMessagesInFlight,
        const TypedDataPtr *pMessagesInFlight
    ) =0;
};


class SnapshotWriterToFile
{
private:
    xmlTextWriterPtr m_dst;
public:
    SnapshotWriterToFile(const char *dst)
    {
        m_dst=xmlNewTextWriterFilename(dst, 0);
    }


    virtual ~SnapshotWriterToFile
    {
        if(m_dst){
            xmlTextWriterClose(m_dst);
            m_dst=0;
        }
    }

    virtual onBeginSnapshot(
        const GraphTypePtr &graph,
        const std::string &id,
        const TypedDataPtr &properties
    ) override
    {
        xmlTextWriterStartDocument(m_dst, NULL, NULL, NULL);
        xmlTextWriterStartElementNS(m_dst, NULL, "GraphSnapshot", m_ns);
    }

    virtual onEndSnapshot() override
    {
        xmlTextWriterEndElement(m_dst);
        xmlTextWriterEndDocument(m_dst);
        xmlTextWriterFlush(m_dst);
    }

    virtual writeDeviceInstance(
        const char *id,
        const DeviceTypePtr &dt,
        const TypedDataPtr &state,
        const bool *readyToSendFlags
    ) override
    {
        xmlTextWriterStartElementNS(m_dst, NULL, "DevS", m_ns);

        xmlTextWriterWriteAttribute(m_dst, "id", id);

        unsigned numOutputs=dt->getOutputCount();
        if(numOutputs>32)
            throw std::runtime_error("Not supported.");
        uint32_t flags=0;
        for(unsigned i=0;i<numOutputs;i++){
            if(readyToSend[i])
                flags |= (1ul << i);
        }
        if(flags!=0){
            xmlTextWriterWriteFormatAttribute(m_dst, "rts", "%x", rts);
        }

        if(state){
            xmlTextWriterStartElementNS(m_dst,  NULL, "S", m_ns);
            dt->getPropertiesSpec()->save(m_dst, state);
            xmlTextWriterEndElement(m_dst);
        }

        xmlTextWriterEndElement(m_dst);
    }

    virtual writeEdgeInstance(
        const char *id,
        const EdgeTypePtr &dt,
        const TypedDataPtr &state,
        uint64_t firings,
        unsigned nMessagesInFlight,
        const TypedDataPtr *pMessagesInFlight
    ) override
    {
        xmlTextWriterStartElementNS(m_dst, NULL, "EdgeS", m_ns);

        xmlTextWriterWriteAttribute(m_dst, "id", id);

        if(firings!=0){
            xmlTextWriterWriteFormatAttribute(m_dst, "firings", "%llx", firings);
        }

        if(state){
            xmlTextWriterStartElementNS(m_dst,  NULL, "S", m_ns);
            dt->getPropertiesSpec()->save(m_dst, state);
            xmlTextWriterEndElement(m_dst);
        }

        if(nMessagesInFlight>0){
            xmlTextWriterStartElementNS(m_dst, NULL, "Q", m_ns)
            for(unsigned i=0; i<nMessagesInFlight; i++){
                xmlTextWriterStartElementNS(m_dst, NULL, "M", m_ns)
                dt.getMessageSpec()->save(m_dst, pMessagesInFlight[i]);
                xmlTextWriterEndElement(m_dst);
            }
            xmlTextWriterEndElement(m_dst);
        }

        xmlTextWriterEndElement(m_dst);
    }

};

#endif
