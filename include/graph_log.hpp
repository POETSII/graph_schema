#ifndef graph_log_hpp
#define graph_log_hpp

#include "graph_core.hpp"

#include "libxml/xmlwriter.h"

class XMLWriter
{
private:    
    xmlTextWriterPtr m_dst;
    std::vector<std::string> m_stack;
public: 
    void openFileDocument(
        const char *file
    ){
        assert(m_dst==0);  

        m_dst=xmlNewTextWriterFilename(file, 0);
        if(!m_dst)
            throw std::runtime_error("Couldn't create output file.");
        xmlTextWriterSetIndent(m_dst, 1);
        xmlTextWriterStartDocument(m_dst, NULL, NULL, NULL);
        
    }
    
    void closeDocument()
    {
        assert(m_dst);
        assert(m_stack.empty());
        
        xmlTextWriterEndDocument(m_dst);
        xmlFreeTextWriter(m_dst);
        
        m_dst=0;
    }
    
    bool is_open() const
    {
        return m_dst!=0;
    }
    
    ~XMLWriter()
    {
        if(m_dst){
            closeDocument();
        }
    }

    void writeFormatAttribute(const char *name, const char *format, ...)
    {
        va_list args;
        va_start(args, format);        
        xmlTextWriterWriteVFormatAttribute(m_dst, (const xmlChar *)name, format, args);
        va_end(args);
    }
    
    void writeAttribute(const char *name, const char *value)
    { xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)name, (const xmlChar *)value); }
    
    void writeAttribute(const char *name, const std::string &value)
    { xmlTextWriterWriteAttribute(m_dst, (const xmlChar *)name, (const xmlChar *)value.c_str()); }
    
    void startElement(const char *name)
    {
        xmlTextWriterStartElement(m_dst, (const xmlChar *)name);
        m_stack.push_back(name);
    }
    
    void startElementNS(const char *name, const char *ns)
    {
        xmlTextWriterStartElementNS(m_dst, NULL, (const xmlChar*)name, (const xmlChar *)ns);
        m_stack.push_back(name);
    }
    
    void endElement(const char *name)
    {
        assert(!m_stack.empty());
        assert(m_stack.back()==name);
        m_stack.pop_back();
        xmlTextWriterEndElement(m_dst);
    }
    
    void writeRaw(const char *value)
    { xmlTextWriterWriteRaw(m_dst, (const xmlChar *)value); }
        
    void writeElement(const char *name, const char *contents)
    { xmlTextWriterWriteElement(m_dst, (const xmlChar*)name,(const xmlChar *)contents); }
    
    void writeElement(const char *name, const std::string &contents)
    { xmlTextWriterWriteElement(m_dst, (const xmlChar*)name,(const xmlChar *)contents.c_str()); }
        
    void writeElementRaw(const char *name, const std::string &contents)
    { 
        writeElementRaw(name,contents.c_str());
    }
    
    void writeElementRaw(const char *name, const char *contents)
    { 
        xmlTextWriterStartElement(m_dst, (const xmlChar*)name);
        xmlTextWriterWriteRaw(m_dst, (const xmlChar *)contents);
        xmlTextWriterEndElement(m_dst);
    }
    
};

class LogWriter
{
public:
    virtual ~LogWriter()
    {}

    virtual void onInitEvent(
        // event
        const char *eventId,
        double time,
        double elapsed,
        // device event
        const DeviceTypePtr &dt,
        const char *dev,
        uint32_t rts,
        uint64_t seq,
        const std::vector<std::string> &logs,
        const TypedDataPtr &state
    ) =0;
    
    virtual void onSendEvent(
        // event
        const char *eventId,
        double time,
        double elapsed,
        // device event
        const DeviceTypePtr &dt,
        const char *dev,
        uint32_t rts,
        uint64_t seq,
        const std::vector<std::string> &logs,
        const TypedDataPtr &state,
        // message event
        const OutputPinPtr &pin,
        // send event
        bool cancel,
        unsigned fanout,
        const TypedDataPtr &msg
    ) =0;
    
    virtual void onRecvEvent(
        // event
        const char *eventId,
        double time,
        double elapsed,
        // device event
        const DeviceTypePtr &dt,
        const char *dev,
        uint32_t rts,
        uint64_t seq,
        const std::vector<std::string> &logs,
        const TypedDataPtr &state,
        // message event
        const InputPinPtr &pin,
        // send event
        const char *sendEventId
    ) =0;
        
    //! End the log and flush the stream
    virtual void close()=0;
};


class LogWriterToFile
  : public LogWriter
{
private:
  const char *m_ns="https://poets-project.org/schemas/virtual-graph-schema-v2";

  std::shared_ptr<xmlChar> toXmlStr(const char *v)
  { return std::shared_ptr<xmlChar>(xmlCharStrdup(v), free); }

  XMLWriter m_dst;
  
  std::string toStr(const TypedDataSpecPtr &spec, const TypedDataPtr &data)
  {
      std::string res=spec->toJSON(data);
      if(!res.empty()){
        res=res.substr(1,res.size()-2);
      }
      return res;
  }
    
    struct event_t
    {
        std::string eventId;
        double time;
        double elapsed;
        
        void writeContents(XMLWriter &dst)
        {
            dst.writeAttribute("eventId", eventId);
            dst.writeFormatAttribute("time", "%g", time);
            dst.writeFormatAttribute("elapsed", "%g", elapsed);
        }
    };
    
    
    struct device_event_t
        : public event_t
    {
        std::string dev;
        uint32_t rts;
        uint64_t seq;
        
        std::vector<std::string> L;
        std::string S;

        void writeContents(XMLWriter &dst)
        {
            dst.writeAttribute("dev", dev);
            dst.writeFormatAttribute("rts", "0x%x", rts);
            dst.writeFormatAttribute("seq", "%llu", seq);
            
            event_t::writeContents(dst);
            
            for(const auto &s : L){
                dst.writeElementRaw("L", s);
            }
            
            dst.writeElementRaw("S", S);
        }
    };
    
    struct init_event_t
        : device_event_t
    {
        void write(XMLWriter &dst)
        {
            dst.startElement("InitEvent");
            writeContents(dst);
            dst.endElement("InitEvent");
        }
    };
    
    struct message_event_t
        : device_event_t
    {
        std::string pin;     // pin name
        std::string msgType;  // msg type id
        std::string msgId;    // Unique message id
        
        void writeContents(XMLWriter &dst)
        {
            dst.writeAttribute("pin", pin);
            
            device_event_t::writeContents(dst);
        }
    };
  
 
    struct send_event_t
        : message_event_t
    {
        bool cancel;
        unsigned fanout;
        std::string M;
        
        void writeContents(XMLWriter &dst)
        {
            dst.writeAttribute("cancel", cancel?"1":"0");
            dst.writeFormatAttribute("fanout", "%u", fanout);
            
            message_event_t::writeContents(dst);
            
            if(M.size()){
                dst.writeElementRaw("M", M);
            }
        }
        
        void write(XMLWriter &dst)
        {
            dst.startElement("SendEvent");
            writeContents(dst);
            dst.endElement("SendEvent");
        }
    };
    
    struct recv_event_t
        : message_event_t
    {
        std::string sendEventId;
        
        void writeContents(XMLWriter &dst)
        {
            dst.writeAttribute("sendEventId", sendEventId);
            
            message_event_t::writeContents(dst);
        }
        
        void write(XMLWriter &dst)
        {
            dst.startElement("RecvEvent");
            writeContents(dst);
            dst.endElement("RecvEvent");
        }
    };
    
public:
    LogWriterToFile(const char *dest)
    {
        m_dst.openFileDocument(dest);
        
        m_dst.startElementNS("GraphLog", m_ns);
    }
    
    ~LogWriterToFile()
    {
        close();
    }
    
    virtual void close()
    {   
        if(m_dst.is_open()){
            m_dst.endElement("GraphLog");
            m_dst.closeDocument();
        }
    }

    void onInitEvent(
        // event
        const char *eventId,
        double time,
        double elapsed,
        // device event
        const DeviceTypePtr &dt,
        const char *dev,
        uint32_t rts,
        uint64_t seq,
        const std::vector<std::string> &logs,
        const TypedDataPtr &state
    ) override {
        init_event_t ev;
        // event
        ev.eventId=eventId;
        ev.time=time;
        ev.elapsed=elapsed;
        // device event
        ev.dev=dev;
        ev.rts=rts;
        ev.seq=seq;
        ev.L=logs;
        ev.S=toStr(dt->getStateSpec(), state);
        
        ev.write(m_dst);
    }
    
    void onSendEvent(
        // event
        const char *eventId,
        double time,
        double elapsed,
        // device event
        const DeviceTypePtr &dt,
        const char *dev,
        uint32_t rts,
        uint64_t seq,
        const std::vector<std::string> &logs,
        const TypedDataPtr &state,
        // message event
        const OutputPinPtr &pin,
        // send event
        bool cancel,
        unsigned fanout,
        const TypedDataPtr &msg
    ) override {
        send_event_t ev;
        // event
        ev.eventId=eventId;
        ev.time=time;
        ev.elapsed=elapsed;
        // device event
        ev.dev=dev;
        ev.rts=rts;
        ev.seq=seq;
        ev.L=logs;
        ev.S=toStr(dt->getStateSpec(), state);
        // message event
        ev.pin=pin->getName();
        // send event
        ev.cancel=cancel;
        ev.fanout=fanout;
        ev.M=toStr(pin->getMessageType()->getMessageSpec(), msg);
        
        ev.write(m_dst);
    }

    void onRecvEvent(
        // event
        const char *eventId,
        double time,
        double elapsed,
        // device event
        const DeviceTypePtr &dt,
        const char *dev,
        uint32_t rts,
        uint64_t seq,
        const std::vector<std::string> &logs,
        const TypedDataPtr &state,
        // message event
        const InputPinPtr &pin,
        // recv event
        const char *sendEventId
    ) override {
        recv_event_t ev;
        // event
        ev.eventId=eventId;
        ev.time=time;
        ev.elapsed=elapsed;
        // device event
        ev.dev=dev;
        ev.rts=rts;
        ev.seq=seq;
        ev.L=logs;
        ev.S=toStr(dt->getStateSpec(), state);
        // message event
        ev.pin=pin->getName();
        // send event
        ev.sendEventId=sendEventId;
        
        ev.write(m_dst);
    }
};



#endif
