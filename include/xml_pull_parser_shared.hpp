#ifndef xml_pull_parser_shared_hpp
#define xml_pull_parser_shared_hpp

#include "graph_persist_dom_reader.hpp"

#include "libxml++/parsers/textreader.h"

#include <algorithm>
#include <functional>

// TODO: Clean up dependency chains a bit
#include "graph_persist_dom_reader_v3.hpp"

namespace pull
{

    using namespace xmlpp;

    using namespace std::placeholders;

    using xml_v3::split_path;

  TypedDataPtr parseTypedDataFromText(TypedDataSpecPtr spec, const std::string &value)
  {
    Document doc;
    Element *elt=doc.create_root_node("X");
    elt->set_child_text(value);
    return spec->load(elt);
  }

  rapidjson::Document parseMetadataFromText(const std::string &value)
  {
    rapidjson::Document document;
    if(!value.empty()){
        std::string text="{"+value+"}";
        
        document.Parse(text.c_str());
        assert(document.IsObject());
    }
    return document;
  }

class ElementBindings
{
public:
    // Sometimes we want internal pointers, so we make all of these non-copyable
    ElementBindings(const ElementBindings &) = delete;
    ElementBindings &operator=(const ElementBindings &) = delete;

    ElementBindings() = default;

    virtual bool skipElement() const
    { return false; }

    virtual bool parseAsNode() const
    { return false; }

    virtual bool seperateCDATA() const
    { return false; }

    /* Used to explicitly request a namespace check. Most of the time we don't bother to improve speed. */
    virtual bool doCheckElementNamespace() const
    { return false; }

    virtual void checkElementNamespace(const Glib::ustring &nsURI) const
    {}

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

    if(bindings->doCheckElementNamespace()){
        bindings->checkElementNamespace(reader->get_namespace_uri());
    }

    if(bindings->skipElement()){
        reader->next();
        return;
    }

    if(bindings->parseAsNode()){
        Element *elt=(Element *)reader->expand();
        bindings->onNode(elt);
        xmlpp::Node::free_wrappers(elt->cobj());
        reader->next();
        return;
    }

    bindings->onBegin(reader->get_local_name());

    if(reader->has_attributes()){
        while(reader->move_to_next_attribute()){
            bindings->onAttribute(reader->get_local_name(), reader->get_value());
        }
        reader->move_to_element();
    }
    bindings->onAttributesFinished();
    if(!reader->is_empty_element()){
        Glib::ustring text;
        bool isCDATA=false;

        auto flush_text = [&]()
        {
            if(!text.empty()){
                auto it=std::find_if( text.begin(), text.end(), [](char c){ return !isspace(c); } );
                if(it!=text.end()){
                    bindings->onText(text);
                }
                text.clear();
            }
        };
        
        bool finished=false;
        while(!finished && reader->read()){
            switch(reader->get_node_type()){
            case TextReader::CDATA:
                isCDATA=true;
                 [[fallthrough]];
            case TextReader::Text:
            case TextReader::SignificantWhitespace:
            case TextReader::Whitespace:
                text += reader->get_value();
                if(isCDATA && bindings->seperateCDATA()){
                    flush_text();
                }
                break;
            case TextReader::Element:
                {
                    flush_text();
                    auto child=bindings->onEnterChild(reader->get_local_name());
                    parseElement( reader, child );
                    bindings->onExitChild(child);
                }
                break;
            case TextReader::EndElement:
                flush_text();
                finished=true;
                break;
            default:
                throw std::runtime_error("Unexpected node type.");
            }
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
        case TextReader::Comment:
            continue;
        case TextReader::Element:
            parseElement(&reader, bindings);
            break;
        default:
            throw std::runtime_error("Unexpected root node, node type="+std::to_string(reader.get_node_type()));
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

}; // namespace pull

#endif
