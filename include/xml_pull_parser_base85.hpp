#ifndef xml_pull_parser_base85_hpp
#define xml_pull_parser_base85_hpp

#include "xml_pull_parser_shared.hpp"

#include "xml_pull_parser_base85.hpp"

#include "graph_persist_sax_writer_base85.hpp"

namespace pull
{
namespace xml_base85
{

class ElementBindingsTextChunks
    : public ElementBindingsComposite
{
    typedef std::function<void (const Glib::ustring &data)> sink_t;

private:
    Glib::ustring m_eltName;
    sink_t m_sink;
public:
    ElementBindingsTextChunks(const Glib::ustring &eltName, sink_t sink)
        : m_eltName(eltName)
        , m_sink(sink)
    {}

    void onBegin(const Glib::ustring &name) override
    {
        if(name!=m_eltName){
            throw std::runtime_error("Unexpected element name.");
        }
    }

  void onAttribute(const Glib::ustring &name, const Glib::ustring &value) override
  {
    throw std::runtime_error("Didnt expect attributes in DeviceInstances85");
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    { throw std::runtime_error("Didnt expect children in DeviceInstances85")}

    void onExitChild(ElementBindings *) override
    { throw std::runtime_error("Didnt expect children in DeviceInstances85"); }

    void onText(const Glib::ustring &text ) override
    { 
        m_sink(text);
    }

    void onEnd() override
    {
    }
};

class ElementBindingsGraphInstance
  : public ElementBindingsComposite
{
private:

    std::string m_textGraphProperties;
    
    ElementBindingsTextChunks m_ebDeviceInstance;
    ElementBindingsTextChunks m_ebEdgeInstance;

    GraphLoadEvents *m_events;
    Registry *m_registry;
    std::unordered_map<std::string,GraphTypePtr> &m_localGraphTypes;

    GraphTypePtr m_graphType;
    TypedDataPtr m_graphProperties;
    std::string m_graphId;
    std::string m_graphTypeId;

    detail::graph_type_info m_gi;
    Base85Codec m_codec;

    uint64_t m_gId;

    std::vector<std::string, std::pair<uint64_t,unsigned> > m_deviceSinkIds;

    unsigned m_bitsPerDeviceIndex;

    unsigned m_deviceTypeIndex=0
    std::string m_deviceId;
    
    void dechunkify(const Glib::ustring &text, std::function<void(const std::vector<std::pair<const char *,const char*>> &)> &cb)
    {
        std::vector<std::pair<const char*,const char *>> lines;
        unsigned pos=0;
        while(pos < text.size()){
            auto e=text.find('\n');
            if(e==Glib::ustring::npos){
                e=text.size();
            }
            if(e-pos > 0){
                lines.push_back({text.data()+pos, text.data()+e});
            }
            pos=e+1;
        }

        cb(lines);
    }

    void on_device_chunk(const std::vector<std::pair<const char *,const char *>> &chunk)
    {
        using Flags = detail::GraphSAXWriterBase85::DeviceFlags;

        unsigned deviceTypeIndex=0;
        std::string deviceId;

        for(auto [begin,end] : chunk){
            while(begin!=end){
                unsigned flags=m_codec.decode_digit(begin, end);

                if(flags&Flags::DeviceInst_ChangeDeviceType){
                    deviceTypeIndex=m_codec.decode_bits(begin, end, m_gi.bits_per_device_type_index);
                }

                const auto &di = m_gi.device_types.at(m_deviceTypeIndex);

                if(flags&Flags::DeviceInst_NotIncrementalId){
                    deviceId=m_codec.device_c_identifier(begin, end);
                }else{
                    assert(!m_prevDeviceId.empty());
                    deviceId.back() += 1;
                }

                TypedDataPtr properties(di.default_properties);
                if(flags&Flags::DeviceInst_HasProperties){
                    m_codec.decode(begin, end, properties.payloadSize(), properties.payloadPtr());
                }

                TypedDataPtr state(di.default_state);
                if(flags&Flags::DeviceInst_HasState){
                    m_codec.decode(begin, end, state.payloadSize(), state.payloadPtr());
                }

                auto dId=m_events->onDeviceInstance(
                    m_gId, di.deviceType, m_deviceId,
                    properties, state
                );

                m_deviceSinkIds.push_back({dId, deviceTypeIndex});
            }
        }
    }

    void on_device_instance_text(const Glib::ustring &text)
    {
        dechunkify(text, [&](const char *begin, const char *end){
            on_device_line(begin, end);
        });
    }

    void on_edge_chunk(const std::vector<std::pair<const char *,const char *>> &chunk)
    {
        using Flags = detail::GraphSAXWriterBase85::EdgeFlags;

        unsigned deviceIndexBits=m_gi.bits_per_device_type_index;
        unsigned dstPinBits=m_gi.max_input_pin_bits;
        unsigned srcPinBits=m_gi.max_output_pin_bits;
        unsigned dstBits=deviceIndexBits+dstPinBits;
        unsigned srcBits=deviceIndexBits+srcPinBits;
        unsigned totalBits=dstBits+srcBits;

        uint64_t deviceIndexMask=(1u<<deviceIndexBits)-1;
        uint64_t srcPinMask=(1u<<srcPinBits)-1;
        uint64_t dstPinMask=(1u<<dstPinBits)-1;

        unsigned dstDeviceIndex=0;
        unsigned dstPinIndex=0;
        unsigned srcDeviceIndex=0;
        unsigned srcPinIndex=0;

        for(auto [begin,end] : chunk){
            while(begin!=end){
                unsigned flags=m_codec.decode_digit(begin, end);

                if( (flags&Flags::EdgeInst_ChangeBothEndpoints) == EdgeInst_ChangeBothEndpoints ){
                    auto both=m_codec.decode_bits(begin, end, totalBits);
                    dstDeviceIndex=(both>>(dstPinBits+srcBits)) & deviceIndexMask;
                    dstPinIndex=(both>>srcBits) & dstPinMask;
                    srcDeviceIndex=(both>>srcPinIndex) & deviceIndexMask;
                    srcPinIndex=both&srcPinMask;
                }else if(flags&Flags::EdgeInst_ChangeDestEndpoint){
                    auto dst=m_codec.decode_bits(begin, end, dstBits);
                    dstDeviceIndex=(dst>>dstPinBits) & deviceIndexMask;
                    dstPinIndex=dst & dstPinMask;
                }else if(flags&Flags::EdgeInst_ChangeSrcEndpoint){
                    auto src=m_codec.decode_bits(begin, end, srcBits);
                    srcDeviceIndex=(src>>srcPinBits) & deviceIndexMask;
                    srcPinIndex=src & srcPinMask;
                }else{
                    throw std::runtime_error("Repeated edge - we dont support multiedges.");
                }

                const std::pair<uint64_t,unsigned> &dst_di=m_deviceSinkIds.at(dstDeviceIndex);
                const std::pair<uint64_t,unsigned> &src_di=m_deviceSinkIds.at(srcDeviceIndex);

                const auto &dst_dt=m_gi.device_types[dst_di.second];
                const auto &src_dt=m_gi.device_types[src_di.second];

                const auto &dst_ip=dst_dt.input_pins.at(dstPinIndex);
                const auto &src_op=src_dt.output_pins.at(srcPinIndex);

                int sendIndex=-1;
                if(flags & Flags::EdgeInst_HasIndex){
                    if(!src_op.pin->isIndexed()){
                        throw std::runtime_error("Send index specified for non indexed output pin.");
                    }
                    sendIndex=m_codec.decode_bits(begin, end, m_bitsPerDeviceIndex);
                }

                TypedDataPtr properties(dst_ip.default_properties);
                if(flags&Flags::EdgeInst_HasProperties){
                    m_codec.decode_bytes(begin, end, properties.payloadSize(), properties.payloadPtr());
                }

                TypedDataPtr state(dst_ip.default_state);
                if(flags&Flags::EdgeInst_HasState){
                    m_codec.decode(begin, end, state.payloadSize(), state.payloadPtr());
                }

                m_events->onEdgeInstance(m_gId,
                    dstDeviceIndex, dst_dt.deviceType, dst_ip.pin,
                    srcDeviceIndex, src_dt.deviceType, src_op.pin,
                    sendIndex,
                    properties, state
                );
            }
        }
    }

    void on_edge_instance_text(const Glib::ustring &text)
    {
        dechunkify(text, [&](const char *begin, const char *end){
            on_edge_chunk(begin, end);
        });
    }

public:

    ElementBindingsGraphInstance(
				 GraphLoadEvents *events,
				 Registry *registry,
                 std::unordered_map<std::string,GraphTypePtr> &localGraphTypes
    )
        : m_events(events)
        , m_ebDeviceInstances("DeviceInstancesBase85", [&](const Glib::ustring &s){
            on_device_instance_text(text);
        })
        , m_ebEdgeInstances("EdgeInstancesBase85", [&](const Glib::ustring &s){
            on_edge_instance_text(text);
        })
        , m_registry(registry)
        , m_localGraphTypes(localGraphTypes)
    {}

    void onBegin(const Glib::ustring &name)
    {
        assert(name=="GraphInstanceBase85");
        m_graphId.clear();
        m_graphTypeId.clear();
        m_graphType.reset();
        m_textGraphProperties.clear();
        m_gId=-1;
    }

    void onAttribute(const Glib::ustring &name, const Glib::ustring &value)
    {
        if(name=="id"){
            m_graphId=value;
        }else if(name=="graphTypeId"){
            m_graphTypeId=value;
        }else if(name=="P"){
            m_textGraphProperties=value;
        }else{
            throw std::runtime_error("Unknown attribute on GraphInstance");
        }
    }

    void onAttributesFinished()
    {
        if(m_graphId.empty()){
            throw std::runtime_error("Missing id on GraphInstance");
        }
        if(m_graphTypeId.empty()){
            throw std::runtime_error("Missing graphTypeId on GraphInstance");
        }

        if(m_registry){
            try{
                m_graphType=m_registry->lookupGraphType(m_graphTypeId);
            }catch(unknown_graph_type_error &){
                // pass
            }
        }
        if(!m_graphType){
            auto it=m_localGraphTypes.find(m_graphTypeId);
            if(it==m_localGraphTypes.end()){
                throw unknown_graph_type_error(m_graphTypeId);
            }
            m_graphType=it->second;
        }
        for(auto dt : m_graphType->getDeviceTypes()){
            m_deviceTypes[dt->getId()]=dt;
        }
        m_events->onGraphType(m_graphType);

        m_gi=detail::graph_type_info(m_graphType);

        if(!m_textGraphProperties.empty()){
            m_graphProperties=m_graphType->getPropertiesSpec()->loadXmlV4ValueSpec(m_textGraphProperties);
        }else{
            m_graphProperties=m_graphType->getPropertiesSpec()->create();
        }

        m_gId=m_events->onBeginGraphInstance(m_graphType, m_graphId, m_graphProperties, {} );
        m_events->onBeginDeviceInstances(m_gId);
    }

    ElementBindings *onEnterChild(const Glib::ustring &name)
    {
        if(name=="DeviceInstancesBase85"){        
            m_events->onBeginDeviceInstances(m_gId);
            return &m_ebDeviceInstances;
        }else if(name=="EdgeInstancesBase85"){
            m_events->onBeginEdgeInstances(m_gId);
            return &m_ebEdgeInstances;
        }else{
            throw std::runtime_error("Unknown child of GraphInstance");
        }
    }

    void onExitChild(ElementBindings *bindings)
    {
        if(bindings==&m_ebDeviceInstances){
            m_events->onEndDeviceInstances(m_gId);
        }else if(bindings==&m_ebEdgeInstances){
            m_events->onEndEdgeInstances(m_gId);
        }else{
            throw std::runtime_error("Unexpected end of elemnet");
        }
    }

    void onEnd()
    {
        m_events->onEndGraphInstance(m_gId);
    }

};


}; // xml_base85
}; // pull

#endif
