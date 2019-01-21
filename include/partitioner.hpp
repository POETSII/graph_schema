#ifndef graph_partitioner_hpp
#define graph_partitioner_hpp

#include "graph.hpp"

#include "rapidjson/rapidjson.h"

#include <random>

class Partitioner
    : public GraphLoadEvents
{
public:
    Partitioner(unsigned nPartitions=2)
    {
        for(unsigned i=0;i<nPartitions;i++){
            m_partitions.push_back(std::make_shared<partition_t>(partition_t{i,0}));
        }
    }


private:

    typedef int32_t count_t;
    typedef int32_t cost_t;

    struct node_t;
    typedef std::shared_ptr<node_t> node_ptr_t;

    struct edge_t;
    typedef std::shared_ptr<edge_t> edge_ptr_t;

    struct partition_t;
    typedef std::shared_ptr<partition_t> partition_ptr_t;

    struct edge_t
    {
        cost_t weight; // Weight, e.g. due to reduced communication frequency
        node_t *src; // Weak (non-owning) pointers
        node_t *dst;
        OutputPinPtr srcPin;
        InputPinPtr dstPin;
        TypedDataPtr properties;
        rapidjson::Document metadata;
    };

    struct node_t
    {
        std::string id;
        unsigned index;
        DeviceTypePtr deviceType;
        cost_t cost;
        partition_t *partition; // Weak.
        std::vector<edge_ptr_t> inputs;
        std::vector<edge_ptr_t> outputs;
        TypedDataPtr properties;
        rapidjson::Document metadata;
    };

    struct partition_t
    {
        unsigned index;
        count_t nodeCount;
    };

    GraphTypePtr m_graphType;
    std::string m_graphId;
    TypedDataPtr m_graphProperties;
    rapidjson::Document m_metadata;

    std::vector<node_ptr_t> m_nodes;
    std::vector<edge_ptr_t> m_edges;
    std::vector<partition_ptr_t> m_partitions;


    count_t m_imbalanceWeight = 1;
    int m_imbalanceExponent=2;
    int m_crossingWeight=1;

    count_t m_targetCount;
    cost_t m_globalCost;

    std::mt19937 m_urng;


    cost_t distance(const partition_t *src, const partition_t *dst) const
    {
        return (src==dst) ? 0 : 1;
    }

    cost_t occupancyCost(const partition_t *part) const
    {
        cost_t d=std::abs(part->nodeCount-m_targetCount);
        cost_t acc=1;
        for(int i=0; i<m_imbalanceExponent; i++){
            acc=acc*d;
        }
        return acc * m_imbalanceWeight;
    }

    virtual bool parseMetaData() const override
    { return true; }

    uint64_t onBeginGraphInstance(
        const GraphTypePtr &graphType,
        const std::string &graphId,
        const TypedDataPtr &graphProperties,
        rapidjson::Document &&metadata
    ) override
    {
        std::cerr<<"onBeginGraphINnstance\n";
        m_graphType=graphType;
        m_graphId=graphId;
        m_graphProperties=graphProperties;
        m_metadata=std::move(metadata);
        
        return 1;
    }   

    virtual uint64_t onDeviceInstance(
        uint64_t gId,
        const DeviceTypePtr &dt,
        const std::string &id,
        const TypedDataPtr &properties,
        rapidjson::Document &&metadata
    ) override
    {
        //std::cerr<<"Device : "<<id<<"\n";

        partition_t *partition=/*m_partitions.front().get(); */m_partitions[m_urng()%m_partitions.size()].get();

        auto index=m_nodes.size();
        std::vector<edge_ptr_t> empty;
        node_ptr_t n(new node_t);
        n->id=id;
        n->index=index;
        n->deviceType=dt;
        n->cost=0;
        n->partition=partition;
        n->properties=properties;
        n->metadata=std::move(metadata);
        m_nodes.push_back(std::move(n));
        partition->nodeCount++;
        return index;
    }

    virtual void onEndGraphInstance(uint64_t gId) override
    {
        m_targetCount=m_nodes.size() / m_partitions.size();
        for(unsigned i=0;i<m_partitions.size();i++){
            m_globalCost += occupancyCost(m_partitions[i].get());
        }

    }

    cost_t calcEdgeWeight(const MessageTypePtr &e, const TypedDataPtr &properties)
    {
        return m_crossingWeight;
    }

    cost_t recalcCost()
    {
        cost_t acc=0;
        for(const auto &e : m_edges){
            acc += distance(e->src->partition, e->dst->partition) * e->weight;
        }
        for(const auto &p : m_partitions){
            acc += occupancyCost(p.get());
        }
        return acc;
    }

    virtual void onEdgeInstance(
        uint64_t gId,
        uint64_t dstDevInst, const DeviceTypePtr &dstDevType, const InputPinPtr &dstPin,
        uint64_t srcDevInst,  const DeviceTypePtr &srcDevType, const OutputPinPtr &srcPin,
        const TypedDataPtr &properties,
        rapidjson::Document &&metadata
    ) override
    {
        //std::cerr<<"Edge : "<<srcDevInst<<" -> "<<dstDevInst<<"\n";

        auto weight=calcEdgeWeight(dstPin->getMessageType(), properties);

        node_t *src=m_nodes.at(srcDevInst).get();
        node_t *dst=m_nodes.at(dstDevInst).get();

        if(src==dst)
            return;

        m_edges.push_back(std::make_shared<edge_t>(edge_t{weight,src,dst,srcPin,dstPin,properties,std::move(metadata)}));
        auto e=m_edges.back();
        src->outputs.push_back(e);
        dst->inputs.push_back(e);

        cost_t delta=distance(src->partition,dst->partition) * weight;
        src->cost += delta;
        m_globalCost += delta;

        //std::cerr<<"  delta="<<delta<<", cost="<<m_globalCost<<"\n";
    }

    cost_t move(node_t *node, partition_t *newPartition)
    {
        cost_t acc=0;

        partition_t *oldPartition=node->partition;

        if(newPartition!=oldPartition){
            // Outgoing nodes
            cost_t localDelta=0;
            for(const auto e : node->outputs){
                auto dstPartition=e->dst->partition;
                auto delta = (distance(newPartition,dstPartition)-distance(oldPartition,dstPartition)) * e->weight;
                localDelta+=delta;
                //std::cerr<<"  out:("<<e->src->id<<":"<<e->src->partition->index<<","<<e->dst->id<<":"<<e->dst->partition->index<<") : "<<delta<<"\n";
            }
            node->cost += localDelta;
            acc += localDelta;

            // Incoming nodes
            for(const auto e : node->inputs){
                auto srcPartition=e->src->partition;
                cost_t remote = (distance(srcPartition, newPartition)-distance(srcPartition,oldPartition)) * e->weight;
                e->src->cost += remote;
                acc += remote;
                //std::cerr<<"  in:("<<e->src->id<<":"<<e->src->partition->index<<","<<e->dst->id<<":"<<e->dst->partition->index<<") : "<<remote<<"\n";
            }

            // Update partitions
            acc -= occupancyCost(newPartition);
            newPartition->nodeCount++;
            acc += occupancyCost(newPartition);

            acc -= occupancyCost(oldPartition);
            oldPartition->nodeCount--;
            acc += occupancyCost(oldPartition);

            node->partition=newPartition;
        }

        m_globalCost += acc;

        return acc; // Total change for this node and all other nodes
    }



public:
    void setImbalanceWeight(int v)
    { m_imbalanceWeight=v; }

    void setImbalanceExponent(int v)
    { m_imbalanceExponent=v; }

    void setCrossingWeight(int v)
    { m_crossingWeight=v; }

    void greedy(unsigned n, double prop=1.0)
    {
        unsigned nn=(unsigned)ceil(n*prop);

        for(unsigned i=0; i<nn; i++){
            node_t *node=m_nodes.at( m_urng()%m_nodes.size() ).get();
            partition_t *newPartition=m_partitions.at( m_urng()%m_partitions.size() ).get();
            partition_t *oldPartition=node->partition;

            //std::cerr<<i<<" : node "<<node->id<<", "<<oldPartition->index<<" -> "<<newPartition->index<<"\n";

            auto old=m_globalCost;

            cost_t delta=move(node, newPartition) + int(m_urng()%5)-2;
            //std::cerr<<"  "<<i<<" : "<<old<<" -> "<<m_globalCost<<", delta="<<delta<<"\n";

            if(delta >= 0){
                move(node, oldPartition);
                //std::cerr<<"  revert\n\n";
            }else{
                //std::cerr<<"  accept\n\n";
            }

        }
    }

    void anneal(unsigned n, double prop=0.1)
    {
        std::uniform_real_distribution<> ureal;

        double T0=10.0;
        double Tn=00.1;
        double alpha=std::pow(Tn/T0, 1.0/n);

        double T=T0;

        unsigned nn=(unsigned)ceil(n*prop);

        for(unsigned i=0; i<nn; i++){
            node_t *node=m_nodes.at( m_urng()%m_nodes.size() ).get();
            partition_t *newPartition=m_partitions.at( m_urng()%m_partitions.size() ).get();

            partition_t *oldPartition=node->partition;

            //std::cerr<<"  "<<node->id<<" : "<<oldPartition->index<<" -> "<<newPartition->index<<"\n";

            auto oldCost=m_globalCost;
            cost_t delta=move(node, newPartition);
            auto newCost=m_globalCost;

            double thresh=(delta <= 0) ? 1.0 : exp( -delta/T );

            bool accept = ureal(m_urng) < thresh;

            if(0==(i%100000)){
                std::cerr.precision(6);
                std::cerr<<"  "<<i<<" : T="<<T<<", cost="<<m_globalCost<<", delta="<<delta<<", thresh="<<thresh<<", accept="<<accept<<"\n";
            //std::cerr<<"  recalc="<<recalcCost()<<"\n";
            }

            if(!accept){
                move(node, oldPartition);
            }

            T=T*alpha;
        }
    }

    void annealk(unsigned n, unsigned k)
    {
        std::uniform_real_distribution<> ureal;

        double T0=100.0;
        double Tn=00.1;
        double alpha=std::pow(Tn/T0, 1.0/n);

        double T=T0;

        std::vector<std::pair<node_t*,partition_t*> > moves;
        moves.reserve(k);

        auto forward=[&](){
            for(unsigned i=0; i<k; i++){
                node_t *node=m_nodes.at( m_urng()%m_nodes.size() ).get();
                partition_t *newPartition=m_partitions.at( m_urng()%m_partitions.size() ).get();

                moves.push_back(std::make_pair(node,node->partition));
                move(node, newPartition);

            }
        };

        auto backward=[&](){
            while(!moves.empty()){
                move(moves.back().first, moves.back().second);
                moves.pop_back();
            }
        };

        for(unsigned i=0; i<n; i++){
            auto oldCost=m_globalCost;
            forward();
            auto newCost=m_globalCost;
            auto delta=newCost-oldCost;

            double thresh=(delta <= 0) ? 1.0 : exp( -delta/T );

            bool accept = ureal(m_urng) < thresh;

            if(0==(i%10000)){
                std::cerr.precision(6);
                std::cerr<<"  "<<i<<" : T="<<T<<", cost="<<m_globalCost<<", delta="<<delta<<", thresh="<<thresh<<", accept="<<accept<<"\n";
            }

            if(!accept){
                backward();
            }else{
                moves.clear();
            }

            T=T*alpha;
        }
    }
    
    template<class TAlloc>
    rapidjson::Value &GetOrAddMemberAsObject(rapidjson::Value &parent, const std::string &name, TAlloc &allocator)
    {
        if(!parent.HasMember(name.c_str())){
            parent.AddMember(std::move(rapidjson::Value(name.c_str(),allocator)), std::move(rapidjson::Value(rapidjson::kObjectType)), allocator);
        }
        auto &x=parent[name.c_str()];
        if(!x.IsObject())
            throw std::runtime_error("Expected an object.");
        return x;
    }
    
    std::string createKey(rapidjson::Document &graphMeta)
    {
        auto &allocator =graphMeta.GetAllocator();
        
        if(!graphMeta.HasMember("device.keys")){
            graphMeta.AddMember("device.keys", std::move(rapidjson::Value(rapidjson::kObjectType)), allocator);
        }
        rapidjson::Value &keys=graphMeta["device.keys"];
        if(!keys.IsObject())
            throw std::runtime_error("Corrupt device.keys metadata.");
        
        std::string key;
        unsigned i=0;
        while(true){
            ++i;
            key="p"+std::to_string(i);
            if(!keys.HasMember(key.c_str()))
                break;
        }
        
        rapidjson::Value vKey(key.c_str(), allocator);
        rapidjson::Value vValue("device.keys", allocator);
        keys.AddMember(std::move(vKey), std::move(vValue), allocator);
        
        rapidjson::Value &dt10Partitions=GetOrAddMemberAsObject(graphMeta, "dt10.partitions", allocator);
        rapidjson::Value &N=GetOrAddMemberAsObject(graphMeta, std::to_string(m_partitions.size()), allocator);
        
        rapidjson::Value counts(rapidjson::kArrayType);
        for(auto p : m_partitions){
            counts.PushBack( p->nodeCount , allocator);
        }
        N.AddMember("counts", std::move(counts), allocator);
        
        N.AddMember("key", std::move(rapidjson::Value(key.c_str(),allocator)), allocator);
        
        return key;
    }
    
    void write(GraphLoadEvents *dst)
    {
        std::string key=createKey(m_metadata);
        
        dst->onGraphType(m_graphType);
        
        auto gId=dst->onBeginGraphInstance(m_graphType, m_graphId, m_graphProperties, std::move(m_metadata));
        
        std::vector<uint64_t> nHandles;
        nHandles.resize(m_nodes.size());
        dst->onBeginDeviceInstances(gId);
        for(auto n : m_nodes){
            rapidjson::Value vKey(key.c_str(), n->metadata.GetAllocator());
            rapidjson::Value vVal(n->partition->index);
            n->metadata.AddMember(std::move(vKey), std::move(vVal), n->metadata.GetAllocator());
            
            
            nHandles[n->index]=dst->onDeviceInstance(gId, n->deviceType, n->id, n->properties, std::move(n->metadata));
        }
        dst->onEndDeviceInstances(gId);
        dst->onBeginEdgeInstances(gId);
        for(auto e : m_edges){
            dst->onEdgeInstance(gId, 
                nHandles[e->dst->index], e->dst->deviceType, e->dstPin,
                nHandles[e->src->index], e->src->deviceType, e->srcPin,
                e->properties,
                std::move(e->metadata)
            );
        }
        dst->onEndEdgeInstances(gId);
        dst->onEndGraphInstance(gId);
    }

    void dump_dot(std::ostream &dst, bool showClusters=true)
    {
        std::array<const char*,8> colours{"blue","green","gray","yellow","blue4","red4","red","green4"};

        std::vector<cost_t> costs;
        for(const auto &n : m_nodes){
            auto cost=n->cost;
            if(cost >= costs.size()){
                costs.resize(cost+1, 0);
            }
            costs.at(cost)++;
        }
        for(unsigned i=0; i<costs.size(); i++){
            if(costs[i]>0){
                std::cerr<<"  cost "<<i<<", count = "<<costs[i]<<"\n";
            }
        }

        auto nodePrint = [&](node_t *n, const std::string &fillColour)
        {
            dst<<" \""<<n->id<<"\" [";
            if(fillColour.size()){
                dst<<" style=\"filled\" fillcolor=\""<<fillColour<<"\"";
            }
            dst<<"];\n";
        };

        dst<<"digraph partitioned{\n";
        //std::cout<<"overlap=false;\n";
        //std::cout<<"spline=true;\n";

        if(!showClusters){
            for(const auto &n : m_nodes){
                nodePrint(n.get(), colours.at(n->partition->index));
            }
        }else{
            std::map<partition_t*,std::vector<node_t*> > partitions;
            for(const auto &n : m_nodes){
                partitions[n->partition].push_back(n.get());
            }

            for(const auto &p : partitions){
                dst<<" subgraph cluster_"<<p.first->index<<" {\n";

                for(const auto &n : p.second){
                   nodePrint(n, n->cost > 0 ? "red" : "");
                }

                dst<<"}\n";
            }
        }
        for(const auto &e : m_edges){
            dst<<" \""<<e->src->id<<"\" -> \""<<e->dst->id<<"\" [";
            if(e->src->partition!=e->dst->partition){
                dst<<" color=\"red\"";
            }else{
                dst<<" color=\"#00000040\"";
            }
            dst<<"];\n";
        }

        dst<<"}\n";
    }
};

#endif
