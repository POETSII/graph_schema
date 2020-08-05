#ifndef graph_dom_hpp
#define graph_dom_hpp

#include <vector>

#include "graph_core.hpp"
#include "graph_persist.hpp"

#include "robin_hood.hpp"

/*
This is a simple low performance DOM-style representation of a graph.

It is mainly for quick-and-dirty validation tools, and will not scale poorly to
large graphs in both memory or time.

Invalid graphs can be constructed, so graphs should be validated if
coming from an untrusted source.
*/
struct GraphDOM
{
    struct DeviceData
    {
        DeviceTypePtr type;
        TypedDataPtr properties;
        TypedDataPtr state;
        rapidjson::Document metadata;
    };

    struct EdgeKey
    {
        std::string dstDevice;
        InputPinPtr dstPin;
        std::string srcDevice;
        OutputPinPtr srcPin;

        bool operator==(const EdgeKey &o) const
        { return dstDevice==o.dstDevice && srcDevice==o.srcDevice && dstPin->getName()==o.dstPin->getName() && srcPin->getName()==o.srcPin->getName(); }
    
        bool operator<(const EdgeKey &o) const
        {
            int c=srcDevice.compare(o.srcDevice);
            if(c!=0) return c<0;

            c=srcPin->getName().compare(o.srcPin->getName());
            if(c!=0) return c<0;

            c=dstDevice.compare(o.dstDevice);
            if(c!=0) return c<0;

            return dstPin->getName() < dstPin->getName();
        }
    };

    struct EdgeData
    {
        int sendIndex=-1;
        TypedDataPtr properties;
        TypedDataPtr state;
        rapidjson::Document metadata;
    };

    struct EdgeKeyHash
    {
        size_t operator()(const EdgeKey &k) const
        {
            std::hash<std::string> sh;
            return 3890346739ul*sh(k.dstDevice) + 3586334599ul*sh(k.srcDevice) +
                    545404213ul*sh(k.dstPin->getName()) + 4161255407ul*sh(k.srcPin->getName());
        }
    };

    using device_map_t = robin_hood::unordered_node_map<std::string,DeviceData>;
    using edge_map_t = robin_hood::unordered_node_map<EdgeKey,EdgeData,EdgeKeyHash>;


    GraphTypePtr graphType;
    std::string id;
    TypedDataPtr properties;
    rapidjson::Document metadata;
    device_map_t devices;
    edge_map_t edges;

    void clear()
    {
        graphType.reset();
        id.clear();
        properties.reset();
        metadata.Clear();
        devices.clear();
        edges.clear();
    }


    std::vector<device_map_t::value_type *> get_sorted_devices()
    {
        using ptr_t = GraphDOM::device_map_t::value_type *;

        std::vector<ptr_t> res;
        for(auto &kv : devices){
            res.push_back(&kv);
        }

        std::sort(res.begin(), res.end(), [](ptr_t a, ptr_t b) -> bool {
            return a->first < b->first;
        });

        return res; // Still no required NVRO in c++17?
    }

    std::vector<edge_map_t::value_type *> get_sorted_edges()
    {
        using ptr_t = GraphDOM::edge_map_t::value_type *;

        std::vector<ptr_t> res;
        for(auto &kv : edges){
            res.push_back(&kv);
        }

        std::sort(res.begin(), res.end(), [](ptr_t a, ptr_t b) -> bool {
            return a->first < b->first;
        });

        return res;
    }
};


class GraphValidationError
    : public std::runtime_error
{
public:
    GraphValidationError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

void validate_graph(const GraphDOM &graph)
{
    auto check_valid_id=[&](const std::string &id)
    {
        if(id.empty()){
            throw GraphValidationError("Found an empty device id.");
        }
        if(!isalpha(id.front())){
            throw GraphValidationError("Found invalid id : "+id);
        }
        for(unsigned i=1; i<id.size(); i++){
            if(!(isalnum(id[i]) || (id[i]!='_'))){
                throw GraphValidationError("Found invalid id : "+id);
            }
        }
    };

    auto check_valid_data=[&](const TypedDataSpecPtr &spec, const TypedDataPtr &value, const char *name)
    {
        if(value){
            if(value.payloadSize() != spec->payloadSize()){
                std::stringstream tmp;
                tmp<<"Invalid data size for "<<name<<", expected "<<spec->payloadSize()<<", got "<<value.payloadSize();
                throw GraphValidationError(tmp.str());
            }
        }
    };

    auto check_same_device_type=[&](const DeviceTypePtr &ref, const DeviceTypePtr &got)
    {
        if(ref!=got){
            if(ref->getId() == got->getId()){
                throw GraphValidationError("Different DeviceType pointers, event though ids match - this method requires pointer equality.");
            }else{
                throw GraphValidationError("Different DeviceType ids.");
            }
        }
    };

    GraphTypePtr gt=graph.graphType;
    if(!gt){
        throw GraphValidationError("No graphType.");
    }
    check_valid_id(graph.id);
    check_valid_data(gt->getPropertiesSpec(), graph.properties, "device properties");

    auto check_owned_device_type=[&](const DeviceTypePtr &dt) -> DeviceTypePtr
    {
        if(!dt){
            throw GraphValidationError("No device type.");
        }
        DeviceTypePtr ref=gt->getDeviceType(dt->getId());
        if(!ref){
            throw GraphValidationError("No device type with this id in graph.");
        }
        check_same_device_type(ref, dt);
        return ref;
    };


    for(const auto &kv : graph.devices){
        try{
            check_valid_id(kv.first);
            auto dt=check_owned_device_type(kv.second.type);
            check_valid_data(dt->getPropertiesSpec(), kv.second.properties, "properties");
            check_valid_data(dt->getStateSpec(), kv.second.properties, "state");
        }catch(const GraphValidationError &e){
            throw GraphValidationError("While processing device "+kv.first+" : "+e.what());
        }
    }


    for(const auto &kv : graph.edges){
        try{
            if(kv.first.srcDevice.empty()){
                throw GraphValidationError("Edge with empty source id.");
            }
            if(!kv.first.srcPin){
                throw GraphValidationError("Edge with empty source pin.");
            }
            if(kv.first.dstDevice.empty()){
                throw GraphValidationError("Edge with empty destination id.");
            }
            if(!kv.first.dstPin){
                throw GraphValidationError("Edge with empty dest pin.");
            }
            const auto &dst_d = graph.devices.at(kv.first.dstDevice);
            const auto &src_d = graph.devices.at(kv.first.srcDevice);

            auto dst_p=kv.first.dstPin;
            check_owned_device_type(dst_p->getDeviceType());
            check_same_device_type(dst_d.type, dst_p->getDeviceType());

            auto src_p=kv.first.srcPin;
            check_owned_device_type(src_p->getDeviceType());
            check_same_device_type(src_d.type, src_p->getDeviceType());

            check_valid_data(dst_p->getPropertiesSpec(), kv.second.properties, "properties");
            check_valid_data(dst_p->getStateSpec(), kv.second.state, "state");
        }catch(const GraphValidationError &e){
            const auto &k=kv.first;
            std::string edge=k.dstDevice+":"+k.dstPin->getName()+"-"+k.srcDevice+":"+k.srcPin->getName();
            throw GraphValidationError("While processing edge "+edge+" : "+e.what());
        }
    }
}


struct GraphDOMBuilder
    : public GraphLoadEvents
{
    using DeviceData = GraphDOM::DeviceData;
    using EdgeKey = GraphDOM::EdgeKey;
    using EdgeData = GraphDOM::EdgeData;

    GraphDOM g;
    std::vector<std::string> ids;

    uint64_t onBeginGraphInstance(
        const GraphTypePtr &graph,
        const std::string &id,
        const TypedDataPtr &properties,
        rapidjson::Document &&metadata
    ) override {
        g.graphType=graph;
        g.id=id;
        g.properties=properties;
        g.metadata=std::move(metadata);
        return 17;
    }


  uint64_t onDeviceInstance
  (
   uint64_t graphInst,
   const DeviceTypePtr &dt,
   const std::string &id,
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
   rapidjson::Document &&metadata=rapidjson::Document()
  ) override {
    auto index=ids.size();

    DeviceData data{dt, properties, state, std::move(metadata)};
    auto iti=g.devices.insert({id,std::move(data)});
    if(!iti.second){
        throw std::runtime_error("Duplicate device id");
    }
    ids.push_back(id);
    return index;
  }

  void onEdgeInstance
  (
   uint64_t graphInst,
   uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
   uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
   int sendIndex, // -1 if it is not indexed pin, or if index is not explicitly specified
   const TypedDataPtr &properties,
   const TypedDataPtr &state,
    rapidjson::Document &&metadata=rapidjson::Document()
  ) override {
    EdgeKey k{ids.at(dstDevInst), dstPin, ids.at(srcDevInst), srcPin};
    EdgeData d{sendIndex, properties, state, std::move(metadata)};
    
    auto iti=g.edges.insert({std::move(k), std::move(d)});
    if(!iti.second){
        throw std::runtime_error("Duplicate edge id.");
    }
  }
};

void save_impl(GraphDOM &g, GraphLoadEvents *events, bool destroy_graph)
{
    bool doMetadata=events->parseMetaData();

    auto meta=[=](rapidjson::Document &&d) -> rapidjson::Document
    {
        rapidjson::Document ret;
        if(doMetadata){
            if(destroy_graph){
                ret=std::move(d);
            }else{
                ret.CopyFrom(d, ret.GetAllocator());
            }
        }
        return ret;
    };

    events->onGraphType(g.graphType);
    auto gid=events->onBeginGraphInstance(g.graphType, g.id, g.properties, meta(std::move(g.metadata)));

    robin_hood::unordered_map<std::string,uint64_t> indices;

    events->onBeginDeviceInstances(gid);
    for(auto *p : g.get_sorted_devices()){
        auto i=events->onDeviceInstance(gid,
            p->second.type, p->first,
            p->second.properties, p->second.state,
            meta(std::move(p->second.metadata))
        );
        indices.insert({p->first,i});
    }
    events->onEndDeviceInstances(gid);

    events->onBeginEdgeInstances(gid);
    for(auto *p : g.get_sorted_edges()){
        events->onEdgeInstance(gid,
            indices.at(p->first.dstDevice), p->first.dstPin->getDeviceType(), p->first.dstPin,
            indices.at(p->first.srcDevice), p->first.srcPin->getDeviceType(), p->first.srcPin,
            p->second.sendIndex,
            p->second.properties, p->second.state,
            meta(std::move(p->second.metadata))
        );
    }
    events->onEndEdgeInstances(gid);

    events->onEndGraphInstance(gid);
}

void save(GraphDOM &&g, GraphLoadEvents *events)
{
    save_impl(g, events, true);
}

void save(const GraphDOM &g, GraphLoadEvents *events)
{
    save_impl(const_cast<GraphDOM&>(g), events, false);
}

#endif