#ifndef xml_pull_parser_hpp
#define xml_pull_parser_hpp

#include "graph_persist_dom_reader.hpp"

#include "libxml++/parsers/textreader.h"

#include <algorithm>

namespace pull
{

    using namespace xmlpp;

      using namespace std::placeholders;

  TypedDataPtr parseTypedDataFromText(TypedDataSpecPtr spec, const std::string &value)
  {
    Document doc;
    Element *elt=doc.create_root_node("X");
    elt->set_child_text(value);
    return spec->load(elt);
  }

  rapidjson::Document parseMetadataFromText(const std::string &value)
  {
    rapidjson::Document doc;

    throw std::runtime_error("Not implemented.");

    return doc;
  }

class ElementBindings
{
public:
    // Sometimes we want internal pointers, so we make all of these non-copyable
    ElementBindings(const ElementBindings &) = delete;
    ElementBindings &operator=(const ElementBindings &) = delete;

  ElementBindings() = default;

    virtual bool parseAsNode() const
    { return false; }

    virtual void onNode(xmlpp::Element *node)
    { throw std::runtime_error("is you want parseAsNode, you need to implement onNode"); }

    virtual void onBegin(const Glib::ustring &name)=0;

    virtual void onAttribute(const Glib::ustring &name, const Glib::ustring &value)=0;

    virtual void onAttributesFinished()
    {} // Usually nothing happens here. Errors happen in onAttribute

    virtual ElementBindings *onEnterChild(const Glib::ustring &name)=0;

    virtual void onExitChild(ElementBindings *childBindings)
    {} // Usually nothing happens here; errors happen in onEnterChild

    virtual void onText(const Glib::ustring &value)=0;

    virtual void onEnd()=0;
};


void parseElement(TextReader *reader, ElementBindings *bindings)
{
    assert(reader->get_node_type()==TextReader::Element);

    if(bindings->parseAsNode()){
        Element *elt=(Element *)reader->expand();
        bindings->onNode(elt);
        reader->next();
        return;
    }

    bindings->onBegin(reader->get_name());

    if(reader->has_attributes()){
        while(reader->move_to_next_attribute()){
            bindings->onAttribute(reader->get_name(), reader->get_value());
        }
        reader->move_to_element();
    }
    bindings->onAttributesFinished();
    if(!reader->is_empty_element()){
        Glib::ustring text;
        bool finished=false;
        while(!finished && reader->read()){
            switch(reader->get_node_type()){
            case TextReader::Text:
            case TextReader::CDATA:
            case TextReader::SignificantWhitespace:
            case TextReader::Whitespace:
                text += reader->get_value();
                break;
            case TextReader::Element:
                {
                    auto child=bindings->onEnterChild(reader->get_name());
                    parseElement( reader, child );
                    bindings->onExitChild(child);
                }
                break;
            case TextReader::EndElement:
                finished=true;
                break;
            default:
                throw std::runtime_error("Unexpected node type.");
            }
        }
        auto it=std::find_if( text.begin(), text.end(), [](char c){ return !isspace(c); } );
        if(it!=text.end()){
            bindings->onText(text);
        }
    }

    bindings->onEnd();
}

void parseDocumentViaElementBindings(const char *path, ElementBindings *bindings)
{
    xmlpp::TextReader reader(path);

    while(reader.read()){
        switch(reader.get_node_type()){
        case TextReader::XmlDeclaration:
        case TextReader::DocumentType:
        case TextReader::ProcessingInstruction:
            continue;
        case TextReader::Element:
            parseElement(&reader, bindings);
            break;
        default:
            throw std::runtime_error("Unexpected root node.");
        }
    }
}

class ElementBindingsText
    : public ElementBindings
{

public:
    std::string text;
  
    void onBegin(const Glib::ustring &) override
    { text.clear(); }

    void onAttribute(const Glib::ustring &name, const Glib::ustring &value)
    {
        throw std::runtime_error("Unexpected attribute for text node.");
    }

    ElementBindings *onEnterChild(const Glib::ustring &name)
    {
        throw std::runtime_error("Unexpected child element for text node.");
    }

    void onText(const Glib::ustring &dst) override
    { text=dst; }

    void onEnd() override
    {}
};

class ElementBindingsComposite
    : public ElementBindings
{
public:
    void onText(const Glib::ustring &)
    {
        throw std::runtime_error("Unexpected text in composite element");
    }
};


class ElementBindingsDeviceInstance
    : public ElementBindingsComposite
{
    typedef std::function<void (std::string &&id, std::string &&deviceType, std::string &&properties, std::string &&metadata )> sink_t;

private:
    sink_t m_sink;

    std::string m_id;
    std::string m_deviceType; 

    ElementBindingsText m_properties;
    ElementBindingsText m_metadata;
public:
    ElementBindingsDeviceInstance(sink_t sink)
        : m_sink(sink)
    {}

    void onBegin(const Glib::ustring &name) override
    {
        assert( (name=="DevI") || (name=="ExtI") );

        m_id.clear();
        m_deviceType.clear();
        m_properties.text.clear();
        m_metadata.text.clear();
    }

  void onAttribute(const Glib::ustring &name, const Glib::ustring &value) override
  {
    if(name=="id"){
      m_id=value;
    }else if(name=="type"){
      m_deviceType=value;
    }else{
      throw std::runtime_error("Unknown attribute on DevI or ExtI");
    }
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    {
        if(name=="P") return &m_properties;
        if(name=="M") return &m_metadata;
        throw std::runtime_error("Unexpected element");
    }

    void onExitChild(ElementBindings *) override
    {}

  void onText(const Glib::ustring & ) override
    { throw std::runtime_error("Text found within DevI or ExtI."); }

    virtual void onEnd() override
    {
        m_sink( std::move(m_id), std::move(m_deviceType), std::move(m_properties.text), std::move(m_metadata.text) );
    }
};

class ElementBindingsEdgeInstance
    : public ElementBindingsComposite
{
typedef std::function<void (std::string &&path, std::string &&properties, std::string &&metadata) > sink_t;

private:
    sink_t m_sink;

    std::string m_path;

    ElementBindingsText m_properties;
    ElementBindingsText m_metadata;
public:
    ElementBindingsEdgeInstance(sink_t sink)
        : m_sink(sink)
    {}

    void onBegin(const Glib::ustring &name) override
    {
        assert( name=="EdgeI" );

        m_path.clear();
        m_properties.text.clear();
        m_metadata.text.clear();
    }

  void onAttribute(const Glib::ustring &name, const Glib::ustring &value)
  {
    if(name=="path"){
      m_path=value;
    }else{
      throw std::runtime_error("Unexpected attribute on EdgeI");
    }
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    {
        if(name=="P") return &m_properties;
        if(name=="M") return &m_metadata;
        throw std::runtime_error("Unexpected element");
    }

    void onExitChild(ElementBindings *) override
    {}

  void onText(const Glib::ustring &) override
    { throw std::runtime_error("Text found within EdgeI."); }

    virtual void onEnd() override
    {
        m_sink( std::move(m_path), std::move(m_properties.text), std::move(m_metadata.text) );
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
    ElementBindingsText m_ebGraphProperties;
      ElementBindingsText m_ebGraphMetadata;

    ElementBindingsDeviceInstance m_ebDeviceInstance;
    ElementBindingsEdgeInstance m_ebEdgeInstance;

    ElementBindingsList m_ebDeviceInstances;
    ElementBindingsList m_ebEdgeInstances;


  GraphLoadEvents *m_events;
  Registry *m_registry;
      bool m_parseMetaData;

    GraphTypePtr m_graphType;
  TypedDataPtr m_graphProperties;
  rapidjson::Document m_graphMetadata;
  std::string m_graphId;
  std::string m_graphTypeId;

    uint64_t m_gId;

    std::unordered_map<std::string,DeviceTypePtr> m_deviceTypes;
    std::unordered_map<std::string, std::pair<uint64_t,DeviceTypePtr> > m_deviceInstances;

    void onDeviceInstance(std::string &&id, std::string &&deviceType, std::string &&properties, std::string &&metadata)
    {
        auto dt=m_deviceTypes.at(deviceType);
        TypedDataPtr deviceProperties;
        if(properties.empty()){
            deviceProperties=dt->getPropertiesSpec()->create();
        }else{
            deviceProperties=parseTypedDataFromText(dt->getPropertiesSpec(), properties);
        }

        uint64_t dId;
        rapidjson::Document deviceMetadata;
        if(m_parseMetaData && !metadata.empty()){
            deviceMetadata=parseMetadataFromText(metadata);
        }
        dId=m_events->onDeviceInstance(m_gId, dt, id, deviceProperties, std::move(deviceMetadata));

        m_deviceInstances.insert(std::make_pair( id, std::make_pair(dId, dt)));
    }

    void onEdgeInstance(std::string &&path, std::string &&properties, std::string &&metadata)
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

        auto et=dstPin->getPropertiesSpec();

        TypedDataPtr edgeProperties;
        if(properties.empty()){
            edgeProperties=et->create();
        }else{
            edgeProperties=parseTypedDataFromText(et, properties);
        }

        rapidjson::Document edgeMetadata;
        if(m_parseMetaData && !metadata.empty()){
            edgeMetadata=parseMetadataFromText(metadata);
        }
        m_events->onEdgeInstance(m_gId,
                    dstDevice.first, dstDevice.second, dstPin,
                    srcDevice.first, srcDevice.second, srcPin,
                    edgeProperties,
                    std::move(edgeMetadata)
        );
    }

public:

    ElementBindingsGraphInstance(
				 GraphLoadEvents *events,
				 Registry *registry
    )
        : m_ebDeviceInstance( std::bind(&ElementBindingsGraphInstance::onDeviceInstance, this, _1, _2, _3, _4) )
        , m_ebEdgeInstance( std::bind(&ElementBindingsGraphInstance::onEdgeInstance, this, _1, _2, _3) )
        , m_ebDeviceInstances{ "DeviceInstances", {{"DevI", &m_ebDeviceInstance}, {"ExtI", &m_ebDeviceInstance}}  }
        , m_ebEdgeInstances{ "EdgeInstances", {{"EdgeI", &m_ebEdgeInstance}} }
      , m_events(events)
	, m_registry(registry)
	, m_parseMetaData(events->parseMetaData())
    {}

    void onBegin(const Glib::ustring &name)
    {
        assert(name=="GraphInstance");
        m_graphId.clear();
        m_graphTypeId.clear();
        m_graphType.reset();
        m_gId=-1;
    }

    void onAttribute(const Glib::ustring &name, const Glib::ustring &value)
    {
        if(name=="id"){
            m_graphId=value;
        }else if(name=="graphTypeId"){
            m_graphTypeId=value;
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

        m_graphType=m_registry->lookupGraphType(m_graphTypeId);
        for(auto et : m_graphType->getMessageTypes()){
            m_events->onMessageType(et);
        }
        for(auto dt : m_graphType->getDeviceTypes()){
            m_events->onDeviceType(dt);
            m_deviceTypes[dt->getId()]=dt;
        }
        m_events->onGraphType(m_graphType);
    }

    ElementBindings *onEnterChild(const Glib::ustring &name)
    {
        if(name=="Properties"){
            return &m_ebGraphProperties;
        }else if(name=="MetaData"){
            return &m_ebGraphMetadata;
        }else if(name=="DeviceInstances"){
	  if(m_gId!=(uint64_t)-1){
                throw std::runtime_error("DeviceInstances appeared twice.");
            }

            m_gId=m_events->onBeginGraphInstance(m_graphType, m_graphId, m_graphProperties, std::move(m_graphMetadata) );
            m_events->onBeginDeviceInstances(m_gId);

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
        if(bindings==&m_ebGraphProperties){
            if(m_graphProperties){
                throw std::runtime_error("Graph properties appeared twice.");
            }
            m_graphProperties=parseTypedDataFromText(m_graphType->getPropertiesSpec(), m_ebGraphProperties.text);
        }else if(bindings==&m_ebGraphMetadata){
            m_graphMetadata=parseMetadataFromText(m_ebGraphMetadata.text);
        }else if(bindings==&m_ebDeviceInstances){
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
public:
    ElementBindingsGraphType(std::string srcPath, GraphLoadEvents *events, Registry *registry)
        : m_srcPath(srcPath)
        , m_events(events)
        , m_registry(registry)
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

        loadGraphTypeElement(m_srcPath, eGraphType, m_events);
    }

  ElementBindings *onEnterChild(const Glib::ustring &) override
  { throw std::runtime_error("Should be working as node."); }

    void onEnd() override
  { throw std::runtime_error("Should be working as node."); }
};

class ElementBindingsGraphs
  : public ElementBindingsComposite
{
private:
    GraphLoadEvents *m_events;
    Registry *m_registry;
    
    ElementBindingsGraphType m_ebGraphType;
    ElementBindingsGraphInstance m_ebGraphInstance;

    bool m_hadGraph=false;
public:
    ElementBindingsGraphs(std::string srcPath, GraphLoadEvents *events, Registry *registry)
        : m_events(events)
        , m_registry(registry)
        , m_ebGraphType(srcPath, events, registry)
        , m_ebGraphInstance(m_events, registry) 
    {}

    void onBegin(const Glib::ustring &name) override
    {
        if(name!="Graphs"){
            throw std::runtime_error("Root of graph is not GraphInstance");
        }
    }

  void onAttribute(const Glib::ustring &, const Glib::ustring &)
  {
    //throw std::runtime_error("Didn't expect attribute here.");
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    {
        if(name=="GraphType"){
            return &m_ebGraphType;
        }else if(name=="GraphInstance"){
            if(m_hadGraph){
                throw std::runtime_error("Can't handle more than one graph instance in a file (TODO)");
            }
            return &m_ebGraphInstance;
        }else if(name=="GraphTypeReference"){
            throw std::runtime_error("Not implemented yet.");
        }else{
            throw std::runtime_error("Unexpected element type.");
        }
    }

    void onExitChild(ElementBindings *bindings) override
    {
        if(bindings==&m_ebGraphInstance){
            m_hadGraph=true;
        }
    }

  void onEnd() override
  {}
};

}; // namespace pull

void loadGraphPull(Registry *registry, const filepath &srcPath, GraphLoadEvents *events)
{
  pull::ElementBindingsGraphs bindings(srcPath.native(), events, registry);

  pull::parseDocumentViaElementBindings(srcPath.c_str(), &bindings);
}

#endif
