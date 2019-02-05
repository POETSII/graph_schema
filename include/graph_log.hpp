#ifndef graph_log_hpp
#define graph_log_hpp

#include "graph_core.hpp"

#include "libxml/xmlwriter.h"

#include <mutex>
#include <chrono>

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
private:
    static std::string toStr(const TypedDataSpecPtr &spec, const TypedDataPtr &data)
    {
      std::string res=spec->toJSON(data);
      if(!res.empty()){
        res=res.substr(1,res.size()-2);
      }
      return res;
    }
public:
    enum event_type{
        init_event,
        send_event,
        recv_event
    };

    struct event_t
    {
        event_t()
        {}

        event_t(
            // event
            const char *_eventId,
            double _time,
            double _elapsed
        )
            : eventId(_eventId)
            , time(_time)
            , elapsed(_elapsed)
        {}

        std::string eventId;
        double time;
        double elapsed;
        std::vector<std::pair<bool,std::string> > tags;

        virtual ~event_t()
        {}

        virtual event_type type() const=0;

        virtual void write(XMLWriter &dst) const=0;

        void writeContents(XMLWriter &dst) const
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

        device_event_t()
        {}

        device_event_t(
            // event
            const char *eventId,
            double time,
            double elapsed,
            // device event
            const DeviceTypePtr &dt,
            const char *_dev,
            uint32_t _rts,
            uint64_t _seq,
            const std::vector<std::string> &_logs,
            const TypedDataPtr &_state
        )
            : event_t(eventId, time, elapsed)
            , dev(_dev)
            , rts(_rts)
            , seq(_seq)
            , L(_logs)
            , S(toStr(dt->getStateSpec(), _state))
        {}

        void writeContents(XMLWriter &dst) const
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
        init_event_t()
        {}

        init_event_t(
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
        )
            : device_event_t(eventId, time, elapsed, dt, dev, rts, seq, logs, state)
        {}

        virtual event_type type() const override
        { return init_event; }

        void write(XMLWriter &dst) const override
        {
            dst.startElement("InitEvent");
            writeContents(dst);
            dst.endElement("InitEvent");
        }
    };
    
    struct message_event_t
        : device_event_t
    {
    protected:
        message_event_t()
        {}

        message_event_t(
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
            const PinPtr &pin
        )
            : device_event_t(eventId, time, elapsed, dt, dev, rts, seq, logs, state)
            , pin(pin->getName())
        {}
    public:

        std::string pin;     // pin name
        //std::string msgType;  // msg type id
        //std::string msgId;    // Unique message id

        void writeContents(XMLWriter &dst) const 
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

        send_event_t()
        {}

        send_event_t(
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
            bool _cancel,
            unsigned _fanout,
            const TypedDataPtr &_msg
        )
            : message_event_t(eventId, time, elapsed, dt, dev, rts, seq, logs, state, pin)
            , cancel(_cancel)
            , fanout(_fanout)
            , M(toStr(pin->getMessageType()->getMessageSpec(), _msg))
        {}

        virtual event_type type() const override
        { return send_event; }

        void writeContents(XMLWriter &dst) const
        {
            dst.writeAttribute("cancel", cancel?"1":"0");
            dst.writeFormatAttribute("fanout", "%u", fanout);
            
            message_event_t::writeContents(dst);
            
            if(M.size()){
                dst.writeElementRaw("M", M);
            }
        }
        
        void write(XMLWriter &dst) const override
        {
            dst.startElement("SendEvent");
            writeContents(dst);
            dst.endElement("SendEvent");
        }
    };
    
    struct recv_event_t
        : message_event_t
    {
        recv_event_t()
        {}

        recv_event_t(
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
            const char *_sendEventId
        )
            : message_event_t(eventId, time, elapsed, dt, dev, rts, seq, logs, state, pin )
            , sendEventId(_sendEventId)
        {}

        std::string sendEventId;

        virtual event_type type() const override
        { return recv_event; }

        void writeContents(XMLWriter &dst) const 
        {
            dst.writeAttribute("sendEventId", sendEventId);
            
            message_event_t::writeContents(dst);
        }
        
        void write(XMLWriter &dst) const override
        {
            dst.startElement("RecvEvent");
            writeContents(dst);
            dst.endElement("RecvEvent");
        }
    };


    virtual ~LogWriter()
    {}

    
    virtual void onEvents(
        const event_t *events,
        unsigned n
    ) =0;

    virtual void onEvents(
        const std::vector<std::unique_ptr<event_t> > &events
    ) =0;

    virtual void onEvent(
        const event_t *event
    ) {
        onEvents(event, 1);        
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
    ) {
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
        
        onEvent(&ev);
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
    ) {
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
        
        onEvent(&ev);
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
        // send event
        const char *sendEventId
    ) {
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
        
        onEvent(&ev);
    }
        
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
  
  std::timed_mutex m_mutex;
  
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
            std::unique_lock<std::timed_mutex> lk(m_mutex, std::chrono::seconds(1));

            // NOTE: whether we own the lock or not after a second we still force close it.

            m_dst.endElement("GraphLog");
            m_dst.closeDocument();
        }
    }

    virtual void onEvents(
        const event_t *events,
        unsigned n
    ) override {
        std::lock_guard<std::timed_mutex> lk(m_mutex);

        for(unsigned i=0; i<n; i++){
            events[i].write(m_dst);
        }
    }

    virtual void onEvents(
        const std::vector<std::unique_ptr<event_t> > &events
    ) override {
        std::lock_guard<std::timed_mutex> lk(m_mutex);

        for(unsigned i=0; i<events.size(); i++){
            events[i]->write(m_dst);
        }
    }
};



#endif
