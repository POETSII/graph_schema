#include <vector>
#include <memory>
#include <unordered_set>
#include <random>
#include <cstdint>
#include <algorithm>

#include "graph_persist.hpp"
#include "graph_persist_dom_reader.hpp"
#include "graph_persist_sax_writer.hpp"

#pragma pack(push,1)
struct neuron_props_t
{
    uint32_t seed;
    uint32_t fanin;
    float a;
    float b;
    float c;
    float d;
    float Ir;
};

struct graph_props_t
{
    uint32_t max_time;
    uint32_t n;
    uint32_t k;
    uint32_t c;
    uint32_t s;
};

struct repeater_s_in_edge_props_t
{
    uint8_t offset; // Identifies which offset within repeater it is
};

struct neuron_s_in_edge_props_t
{
    float weight;
};
#pragma pack(pop)

struct Neuron
{
    unsigned index;
    neuron_props_t properties;
    std::vector<std::pair<unsigned,float> > synapses;
};  

struct NeuralNetwork
{
    std::vector<Neuron> neurons;
};

void dump_network_as_dot(const NeuralNetwork &nn, std::ostream &dst)
{
    dst<<"digraph network {\n";
    for(unsigned i=0; i<nn.neurons.size(); i++){
        for(auto s : nn.neurons[i].synapses){
            dst<<" n"<<s.first<<"->n"<<i<<";\n";
        }
    }
    dst<<"}\n";
}

void create_random_neuron_properties(neuron_props_t &properties, std::mt19937 &rng, bool ex)
{
    static std::uniform_real_distribution<float> udist;
    float r=udist(rng);
    if(ex){
        properties.a=0.02;
        properties.b=0.2;
        properties.c=-65+15*r*r;
        properties.d=8-6*r*r;
        properties.Ir=5;
    }else{
        properties.a=0.02+0.08*r;
        properties.b=0.25-0.05*r;
        properties.c=-65;
        properties.d=2;
        properties.Ir=2;
    }
    properties.seed=rng();
}

NeuralNetwork create_random_network(std::mt19937 &rng, unsigned n, unsigned m, int nEx)
{
    static std::uniform_real_distribution<float> udist;

    NeuralNetwork res;

    float probEx=nEx/float(n);

    fprintf(stderr, "Creating neurons\n");
    res.neurons.resize(n);
    for(unsigned i=0; i<n; i++){
        bool ex=udist(rng) < probEx;
        create_random_neuron_properties(res.neurons[i].properties, rng, ex);
        res.neurons[i].synapses.clear();
        res.neurons[i].synapses.reserve(m);
    }

    std::uniform_int_distribution<unsigned> ndist(0, n-1);

    fprintf(stderr, "Creating connections\n");
    // We really want to avoid doing this in O(n^2) time given n~=1M
    std::vector<bool> used;
    for(unsigned i=0; i<n; i++){
        if((i%1000)==0){
            fprintf(stderr, "   Neuron %u of %u\n", i, n);
        }
        used.assign(n, false);
        auto &dest=res.neurons[i].synapses;
        while(dest.size()<m){
            unsigned cand=ndist(rng);
            if(!used[cand]){
                used[cand]=true;
                float w=udist(rng);
                if(cand < nEx){
                    w=0.5*w;
                }else{
                    w=-w;
                }
                dest.push_back(std::make_pair(cand,w));
            }
        }

        res.neurons[i].properties.fanin=res.neurons[i].synapses.size();
    }

    return res;
}

void write_graph_to_sink(
    GraphTypePtr graphType,
    const NeuralNetwork &network,
    const std::string &graph_id,
    unsigned max_time,
    unsigned c,
    unsigned k,
    GraphLoadEvents &sink
){
    if(k>32){
        throw std::runtime_error("Cannot support k>32 due to dubious design decisions by dt10.");
    }

    unsigned n=network.neurons.size();
    unsigned s=(n+c-1)/c;

    DeviceTypePtr neuronType=graphType->getDeviceType("neuron");
    DeviceTypePtr repeaterType=graphType->getDeviceType("repeater");

    // The bindings returned by the sink
    uint64_t g_id;
    std::vector<uint64_t> n_ids;
    std::vector<std::vector<uint64_t> > r_ids;

    TypedDataPtr graphPropsGeneric=graphType->getPropertiesSpec()->create();
    if(sizeof(graph_props_t)!=graphPropsGeneric.payloadSize()){
        throw std::runtime_error("Graph properties size mismatch.");
    };
    
    graph_props_t *graphProperties=(graph_props_t*)graphPropsGeneric.payloadPtr();
    graphProperties->max_time=max_time;
    graphProperties->c=c;
    graphProperties->k=k;
    graphProperties->s=s;
    graphProperties->n=n;
    
    sink.onGraphType(graphType);

    g_id=sink.onBeginGraphInstance(graphType, graph_id, graphPropsGeneric, rapidjson::Document());

    fprintf(stderr, "  Writing devices\n");
    sink.onBeginDeviceInstances(g_id);

    TypedDataPtr defaultNeuronProps=neuronType->getPropertiesSpec()->create();
    
    fprintf(stderr, "    Writing Neurons\n");
    n_ids.reserve(n);
    for(unsigned i=0; i<n; i++){
        unsigned cluster=i/s;
        char buffer[64]={0};
        snprintf(buffer, sizeof(buffer)-1, "c%u_n%u", cluster, i);
        auto properties=defaultNeuronProps.clone();
        memcpy(properties.payloadPtr(), &network.neurons[i].properties, sizeof(neuron_props_t));
        auto state=neuronType->getStateSpec()->create();
        n_ids.push_back(sink.onDeviceInstance(g_id, neuronType, buffer, properties, state, rapidjson::Document()));
    }

    fprintf(stderr, "    Writing Repeaters\n");
    r_ids.resize(c);
    for(unsigned cluster=0; cluster<c; cluster++){
        auto &r_ids_curr=r_ids[cluster];
        r_ids_curr.resize((n+k-1)/k);
        for (unsigned r=0; r<r_ids_curr.size(); r++){
            char buffer[64]={0};
            snprintf(buffer, sizeof(buffer)-1, "c%u_r%u", cluster, r);
            auto properties=TypedDataPtr();
            auto state=repeaterType->getStateSpec()->create();
            auto r_id=sink.onDeviceInstance(g_id, repeaterType, buffer, properties, state, rapidjson::Document());
            r_ids_curr[r]=r_id;
        }
    }
    sink.onEndDeviceInstances(g_id);

    fprintf(stderr, "  Writing edges\n");
    sink.onBeginEdgeInstances(g_id);
    unsigned numEdges=0;

    InputPinPtr repeaterInPin=repeaterType->getInput("s_in");
    std::vector<OutputPinPtr> repeaterOutPins;
    for(unsigned i=0; i<k; i++){
        repeaterOutPins.push_back(repeaterType->getOutput("s_out_"+std::to_string(i)));
        if(!repeaterOutPins.back()){
            throw std::runtime_error("This graph doesn't have enough repeater s_out pins.");
        }
    }
    InputPinPtr neuronInPin=neuronType->getInput("s_in");
    OutputPinPtr neuronOutPin=neuronType->getOutput("s_out");
    
    TypedDataPtr defaultRepeaterEdgeProps=repeaterInPin->getPropertiesSpec()->create();
    TypedDataPtr defaultNeuronEdgeProps=neuronInPin->getPropertiesSpec()->create();

    if(sizeof(repeater_s_in_edge_props_t)!=defaultRepeaterEdgeProps.payloadSize()){
        throw std::runtime_error("Repeater edge properties size mismatch.");
    };
    if(sizeof(neuron_s_in_edge_props_t)!=defaultNeuronEdgeProps.payloadSize()){
        throw std::runtime_error("Neuron edge properties size mismatch.");
    };

    fprintf(stderr, "    repeater <- neuron edges\n");
    // Generate global cluster connections
    for(unsigned i=0; i<n; i++){
        if( (i%1000)==0 ){
            fprintf(stderr, "      neuron %u of %u\n", i, n);
        }
        unsigned cluster_src=i/s;
        unsigned src_n_id = n_ids[i]; 
        for(unsigned cluster_dst=0; cluster_dst<c; cluster_dst++){
            unsigned dst_r_id = r_ids[cluster_dst][i/k]; // "c{cluster_dst}_r{floor(i/k)}:s_in"
            
            TypedDataPtr ep=defaultRepeaterEdgeProps.clone();
            TypedDataPtr es;
            ((repeater_s_in_edge_props_t*)ep.payloadPtr())->offset=i%k;
            sink.onEdgeInstance(g_id,
                dst_r_id, repeaterType, repeaterInPin,
                src_n_id, neuronType, neuronOutPin,
                -1, ep, es
            );
            numEdges++;
        }
    }
    
    fprintf(stderr, "    neuron <- repeater edges\n");
    // Generate local fanout
    for(unsigned dst=0; dst<n; dst++){
        if( (dst%1000)==0 ){
            fprintf(stderr, "      neuron %u of %u\n", dst, n);
        }
        unsigned dst_cluster=dst/s;
        for(const auto &e : network.neurons[dst].synapses){
            unsigned src=e.first;
            float w=e.second;
            unsigned src_cluster=src/s;
            uint64_t dst_n_id=n_ids[dst]; //"c{cluster}_n{dst}:s_in"
            uint64_t src_r_id=r_ids[src_cluster][src/k]; //  <- "c{cluster}_r{floor(src/k)}:s_out{src%k}"
            TypedDataPtr ep=defaultNeuronEdgeProps.clone();
            TypedDataPtr es;
            ((neuron_s_in_edge_props_t*)ep.payloadPtr())->weight=w;
            sink.onEdgeInstance(g_id,
                dst_n_id, neuronType, neuronInPin,
                src_r_id, repeaterType, repeaterOutPins.at(src%k),
                -1, ep, es
            );
            numEdges++;
        }
    }
    fprintf(stderr, "    Added %u edges\n", numEdges);

    sink.onEndEdgeInstances(g_id);

    sink.onEndGraphInstance(g_id);
}

int main(int argc, char *argv[])
{
    std::mt19937 rng;

    int n=1000;
    if(argc>1){
        n=atoi(argv[1]);
    }
    int m=std::min(1000.0, n*0.1);
    if(argc>2){
        m=std::min(n, atoi(argv[2]));
    }

    fprintf(stderr, "Building network\n");
    NeuralNetwork network=create_random_network(rng, n, m, n*0.8);

    //dump_network_as_dot(network, std::cout);

    
    std::string path="wibble.xml.gz";
    sax_writer_options options;
    // Sanity checks add O(n m) memory and time cost 
    options.sanity= n*m<10000;
    auto pSink=createSAXWriterOnFile(path, options);

    std::string id="wibble";
    int max_time=1000;
    int k=8;
    int c=25;

    fprintf(stderr, "Writing network\n");
    GraphTypePtr graphType=loadGraphType(filepath("barrier_izhikevich_clustered_graph_type.xml"), "barrier_izhikevich_clustered");

    write_graph_to_sink(
        graphType, network, id, max_time, c, k, *pSink 
    );

    pSink.reset();

    return 0;
}