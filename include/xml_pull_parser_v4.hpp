#ifndef xml_pull_parser_v4_hpp
#define xml_pull_parser_v4_hpp

#include "xml_pull_parser_shared.hpp"

namespace pull
{
namespace xml_v4
{

class ElementBindingsDeviceInstance
    : public ElementBindingsComposite
{
    typedef std::function<void (std::string &&id, std::string &&deviceType, std::string &&properties, std::string &&state )> sink_t;

private:
    sink_t m_sink;

    std::string m_id;
    std::string m_deviceType; 

    std::string m_properties;
    std::string m_state;
public:
    ElementBindingsDeviceInstance(sink_t sink)
        : m_sink(sink)
    {}

    void onBegin(const Glib::ustring &name) override
    {
        assert( (name=="DevI") || (name=="ExtI") );

        m_id.clear();
        m_deviceType.clear();
        m_properties.clear();
        m_state.clear();
    }

  void onAttribute(const Glib::ustring &name, const Glib::ustring &value) override
  {
    if(name=="id"){
      m_id=value;
    }else if(name=="type"){
      m_deviceType=value;
    }else if(name=="P"){
        m_properties=value;
    }else if(name=="S"){   
        m_state=value; 
    }else{
      throw std::runtime_error("Unknown attribute on DevI or ExtI");
    }
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    {
        throw std::runtime_error("Unexpected element");
    }

    void onExitChild(ElementBindings *) override
    {}

    void onText(const Glib::ustring & ) override
    { throw std::runtime_error("Text found within DevI or ExtI."); }

    virtual void onEnd() override
    {
        m_sink( std::move(m_id), std::move(m_deviceType), std::move(m_properties), std::move(m_state) );
    }
};

class ElementBindingsEdgeInstance
    : public ElementBindingsComposite
{
    typedef std::function<void (std::string &&path, int sendIndex, std::string &&properties, std::string &&state) > sink_t;

private:
    sink_t m_sink;

    std::string m_path;
    int m_sendIndex;

    std::string m_properties;
    std::string m_state;
public:
    ElementBindingsEdgeInstance(sink_t sink)
        : m_sink(sink)
    {}

    void onBegin(const Glib::ustring &name) override
    {
        assert( name=="EdgeI" );

        m_path.clear();
        m_sendIndex=-1;
        m_properties.clear();
    }

  void onAttribute(const Glib::ustring &name, const Glib::ustring &value)
  {
    if(name=="path"){
      m_path=value;
    }else if(name=="sendIndex"){
        m_sendIndex=atoi(value.c_str());
    }else if(name=="P"){
        m_properties=value;
    }else if(name=="S"){
        m_state=value;
    }else{
      throw std::runtime_error("Unexpected attribute on EdgeI");
    }
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    {
        throw std::runtime_error("Unexpected element");
    }

    void onExitChild(ElementBindings *) override
    {}

  void onText(const Glib::ustring &) override
    { throw std::runtime_error("Text found within EdgeI."); }

    virtual void onEnd() override
    {
        m_sink( std::move(m_path), m_sendIndex, std::move(m_properties), std::move(m_state) );
    }
};

class ElementBindingsList
    : public ElementBindingsComposite
{
public:
    typedef std::vector<std::pair<const char *,ElementBindings*> > bindings_list;
private:
    const char *m_name;
    bindings_list m_bindings;
public:
    ElementBindingsList(const char * name, const bindings_list &bindings)
        : m_name(name)
        , m_bindings(bindings)
    {}

    void onBegin(const Glib::ustring &name) override
    {
        assert( name==m_name );
    }

  void onAttribute(const Glib::ustring &, const Glib::ustring &) override
  {
    throw std::runtime_error("Unexpected attribute on element.");
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    {
        for(const auto &np : m_bindings){
            if(name==np.first) return np.second;
        }
        throw std::runtime_error("Unexpected element");
    }

  void onEnd() override
  {}
};

class ElementBindingsGraphInstance
  : public ElementBindingsComposite
{
private:
    std::string m_textGraphProperties;
    
    ElementBindingsDeviceInstance m_ebDeviceInstance;
    ElementBindingsEdgeInstance m_ebEdgeInstance;

    ElementBindingsList m_ebDeviceInstances;
    ElementBindingsList m_ebEdgeInstances;


  GraphLoadEvents *m_events;
  Registry *m_registry;
  std::unordered_map<std::string,GraphTypePtr> &m_localGraphTypes;
    bool m_parseMetaData;

    bool m_skipGraphInstance=false;

    GraphTypePtr m_graphType;
  TypedDataPtr m_graphProperties;
  rapidjson::Document m_graphMetadata;
  std::string m_graphId;
  std::string m_graphTypeId;

    uint64_t m_gId;

    std::unordered_map<std::string,DeviceTypePtr> m_deviceTypes;
    std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> > m_deviceInstances;

    void onDeviceInstance(std::string &&id, std::string &&deviceType, std::string &&properties, std::string &&state)
    {
        auto dt=m_deviceTypes.at(deviceType);
        TypedDataPtr deviceProperties;
        if(properties.empty()){
            deviceProperties=dt->getPropertiesSpec()->create();
        }else{
            deviceProperties=dt->getPropertiesSpec()->loadXmlV4ValueSpec(properties);
        }

        uint64_t dId;
        rapidjson::Document deviceMetadata;
        
        TypedDataPtr deviceState;   
        if(state.empty()){
            deviceState=dt->getStateSpec()->create();
        }else{
            deviceState=dt->getStateSpec()->loadXmlV4ValueSpec(state);
        }
        
        dId=m_events->onDeviceInstance(m_gId, dt, id, deviceProperties, deviceState, std::move(deviceMetadata));

        m_deviceInstances.insert(std::make_pair( id, std::make_pair(dId, dt)));
    }

    void onEdgeInstance(std::string &&path, int sendIndex, std::string &&properties, std::string &&state)
    {
        std::string srcDeviceId, srcPinName, dstDeviceId, dstPinName;
        split_path(path, dstDeviceId, dstPinName, srcDeviceId, srcPinName);
    
        auto &srcDevice=m_deviceInstances.at(srcDeviceId);
        auto &dstDevice=m_deviceInstances.at(dstDeviceId);
    
        auto srcPin=srcDevice.second->getOutput(srcPinName);
        auto dstPin=dstDevice.second->getInput(dstPinName);
    
        if(!srcPin){
            throw std::runtime_error("No source pin called '"+srcPinName+"' on device '"+srcDeviceId);
        }
        if(!dstPin){
            throw std::runtime_error("No sink pin called '"+dstPinName+"' on device '"+dstDeviceId);
        }
    
        if(srcPin->getMessageType()!=dstPin->getMessageType())
            throw std::runtime_error("Edge type mismatch on pins.");

        auto ep=dstPin->getPropertiesSpec();
        auto es=dstPin->getStateSpec();

        TypedDataPtr edgeProperties;
        if(properties.empty()){
            edgeProperties=ep->create();
        }else{
            edgeProperties=ep->loadXmlV4ValueSpec(properties);
        }

        rapidjson::Document edgeMetadata;

        TypedDataPtr edgeState;
        if(state.empty()){
            edgeState=es->create();
        }else{
            edgeState=es->loadXmlV4ValueSpec(state);
        }
        
        uint64_t dstDeviceUnq=dstDevice.first;
        uint64_t srcDeviceUnq=srcDevice.first;
        m_events->onEdgeInstance(m_gId,
                    dstDeviceUnq, dstDevice.second, dstPin,
                    srcDeviceUnq, srcDevice.second, srcPin,
                    sendIndex,
                    edgeProperties,
                    edgeState,
                    std::move(edgeMetadata)
        );
    }

public:

    ElementBindingsGraphInstance(
				 GraphLoadEvents *events,
				 Registry *registry,
                 std::unordered_map<std::string,GraphTypePtr> &localGraphTypes,
                 bool skipGraphInstance
    )
        : m_ebDeviceInstance( std::bind(&ElementBindingsGraphInstance::onDeviceInstance, this, _1, _2, _3, _4) )
        , m_ebEdgeInstance( std::bind(&ElementBindingsGraphInstance::onEdgeInstance, this, _1, _2, _3, _4) )
        , m_ebDeviceInstances{ "DeviceInstances", {{"DevI", &m_ebDeviceInstance}, {"ExtI", &m_ebDeviceInstance}}  }
        , m_ebEdgeInstances{ "EdgeInstances", {{"EdgeI", &m_ebEdgeInstance}} }
      , m_events(events)
	, m_registry(registry)
    , m_localGraphTypes(localGraphTypes)
	, m_parseMetaData(events->parseMetaData())
    , m_skipGraphInstance(skipGraphInstance)
    {}

    void onBegin(const Glib::ustring &name)
    {
        assert(name=="GraphInstance");
        m_graphId.clear();
        m_graphTypeId.clear();
        m_graphType.reset();
        m_textGraphProperties.clear();
        m_graphMetadata=rapidjson::Document();
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
        for(auto et : m_graphType->getMessageTypes()){
            m_events->onMessageType(et);
        }
        for(auto dt : m_graphType->getDeviceTypes()){
            m_events->onDeviceType(dt);
            m_deviceTypes[dt->getId()]=dt;
        }
        m_events->onGraphType(m_graphType);

        if(!m_textGraphProperties.empty()){
            m_graphProperties=m_graphType->getPropertiesSpec()->loadXmlV4ValueSpec(m_textGraphProperties);
        }else{
            m_graphProperties=m_graphType->getPropertiesSpec()->create();
        }

        m_gId=m_events->onBeginGraphInstance(m_graphType, m_graphId, m_graphProperties, std::move(m_graphMetadata) );
        m_events->onBeginDeviceInstances(m_gId);
    }

    ElementBindings *onEnterChild(const Glib::ustring &name)
    {
        if(name=="DeviceInstances"){            
            return &m_ebDeviceInstances;
        }else if(name=="EdgeInstances"){
            m_events->onBeginDeviceInstances(m_gId);
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

class ElementBindingsGraphType
    : public ElementBindings
{
private:
    std::string m_srcPath;
    GraphLoadEvents *m_events;
    Registry *m_registry;
    std::unordered_map<std::string,GraphTypePtr> &m_localGraphTypes;
public:
    ElementBindingsGraphType(std::string srcPath, GraphLoadEvents *events, Registry *registry, std::unordered_map<std::string,GraphTypePtr> &localGraphTypes)
        : m_srcPath(srcPath)
        , m_events(events)
        , m_registry(registry)
        , m_localGraphTypes(localGraphTypes)
    {}

  void onBegin(const Glib::ustring &)
  { throw std::runtime_error("Should be working as node."); }

  void onText(const Glib::ustring &)
  { throw std::runtime_error("Should be working as node."); }

  void onAttribute(const Glib::ustring &, const Glib::ustring &)
  { throw std::runtime_error("Should be worked as node."); }

    bool parseAsNode() const override
    { return true; }

    void onNode(xmlpp::Element *eGraphType) override
    {
        assert(eGraphType->get_name()=="GraphType");

        auto gt=::xml_v4::loadGraphTypeElement(m_srcPath, eGraphType, m_events);
        m_localGraphTypes[gt->getId()]=gt;
    }

  ElementBindings *onEnterChild(const Glib::ustring &) override
  { throw std::runtime_error("Should be working as node."); }

    void onEnd() override
  { throw std::runtime_error("Should be working as node."); }
};

}; // xml_v4
}; // pull

#endif
