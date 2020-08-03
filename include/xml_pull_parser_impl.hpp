#ifndef xml_pull_parser_impl_hpp
#define xml_pull_parser_impl_hpp

#include "xml_pull_parser_shared.hpp"
#include "xml_pull_parser_v3.hpp"
#include "xml_pull_parser_v4.hpp"
#include "xml_pull_parser_base85.hpp"

namespace pull
{

class ElementBindingsGraphs
  : public ElementBindingsComposite
{
private:
    GraphLoadEvents *m_events;
    Registry *m_registry;
    std::unordered_map<std::string,GraphTypePtr> m_localGraphTypes;

    xml_v3::ElementBindingsGraphType m_ebGraphTypeV3;
    xml_v3::ElementBindingsGraphInstance m_ebGraphInstanceV3;

    xml_v4::ElementBindingsGraphType m_ebGraphTypeV4;
    xml_v4::ElementBindingsGraphInstance m_ebGraphInstanceV4;

    xml_base85::ElementBindingsGraphInstance m_ebGraphInstanceBase85;

    bool m_graphTypeOnly=false;

    bool m_hadGraph=false;

    const char *ns_v4="https://poets-project.org/schemas/virtual-graph-schema-v4";
    const char *ns_v3="https://poets-project.org/schemas/virtual-graph-schema-v3";

    mutable int m_activeXmlVersion=-1;

public:
    ElementBindingsGraphs(std::string srcPath, GraphLoadEvents *events, Registry *registry, bool graphTypeOnly=false)
        : m_events(events)
        , m_registry(registry)

        , m_ebGraphTypeV3(srcPath, events, registry, m_localGraphTypes)
        , m_ebGraphInstanceV3(m_events, registry, m_localGraphTypes, graphTypeOnly) 

        , m_ebGraphTypeV4(srcPath, events, registry, m_localGraphTypes)
        , m_ebGraphInstanceV4(m_events, registry, m_localGraphTypes, graphTypeOnly) 

        , m_ebGraphInstanceBase85(m_events, registry, m_localGraphTypes) 

        , m_graphTypeOnly(graphTypeOnly)
    {}

    bool doCheckElementNamespace() const override
    { return true; }

    void checkElementNamespace(const Glib::ustring &nsURI) const override
    {
        std::cerr<<"Check\n";
        if(nsURI!=ns_v3 && nsURI!=ns_v4){
            throw std::runtime_error("Element came from the wrong namespace.");
        }
        if(nsURI==ns_v3){
            m_activeXmlVersion=3;
        }
        if(nsURI==ns_v4){
            m_activeXmlVersion=4;
        }
    }

    void onBegin(const Glib::ustring &name) override
    {
        if(name!="Graphs"){
            throw std::runtime_error("Root of graph is not GraphInstance");
        }
    }

  void onAttribute(const Glib::ustring &, const Glib::ustring &)
  {
      // TODO: this should handle namespaces correctly
    //throw std::runtime_error("Didn't expect attribute here.");
  }

    ElementBindings *onEnterChild(const Glib::ustring &name) override
    {
        if(name=="GraphType"){
            if(m_activeXmlVersion==3){
                return &m_ebGraphTypeV3;
            }else if(m_activeXmlVersion==4){
                return &m_ebGraphTypeV4;
            }else{
                throw std::runtime_error("No implementation for v4 graph.");
            }
        }else if(name=="GraphInstance"){
            if(m_hadGraph){
                throw std::runtime_error("Can't handle more than one graph instance in a file (TODO)");
            }
            if(m_activeXmlVersion==3){
                return &m_ebGraphInstanceV3;
            }else if(m_activeXmlVersion==4){
                return &m_ebGraphInstanceV4;
            }else{
                throw std::runtime_error("No implementation for v4 graph.");
            }
        }else if(name=="GraphInstanceBase85"){
            if(m_hadGraph){
                throw std::runtime_error("Can't handle more than one graph instance in a file (TODO)");
            }
            if(m_activeXmlVersion==3){
                throw std::runtime_error("Base85 graphs should be in xml-v4 namespace");
            }else if(m_activeXmlVersion==4){
                return &m_ebGraphInstanceBase85;
            }else{
                throw std::runtime_error("No implementation for base85 graph.");
            }
        }else if(name=="GraphTypeReference"){
            throw std::runtime_error("Not implemented yet.");
        }else{
            throw std::runtime_error("Unexpected element type." + name);
        }
    }

    void onExitChild(ElementBindings *bindings) override
    {
        if(bindings==&m_ebGraphInstanceV3){
            m_hadGraph=true;
        }else if(bindings==&m_ebGraphInstanceV4){
            m_hadGraph=true;
        }else if(bindings==&m_ebGraphInstanceBase85){
            m_hadGraph=true;
        }
    }

  void onEnd() override
  {}
};

}; // namespace pull

/*
    TODO: This doesn't handle namespaces properly
*/
void loadGraphPull(Registry *registry, const filepath &srcPath, GraphLoadEvents *events)
{
  pull::ElementBindingsGraphs bindings(srcPath.native(), events, registry);

  pull::parseDocumentViaElementBindings(srcPath.c_str(), &bindings);
}

void loadGraphTypePull(Registry *registry, const filepath &srcPath, GraphLoadEvents *events)
{
  pull::ElementBindingsGraphs bindings(srcPath.native(), events, registry, true);

  pull::parseDocumentViaElementBindings(srcPath.c_str(), &bindings);
}

#endif
