#include "routing_file_channel_endpoint.hpp"

#include <iostream>
#include <random>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>

double now()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec+1e-9*ts.tv_nsec;
}

const int L=64;

void worker(unsigned K, Endpoint &ep, unsigned n)
{
    endpoint_address_t self=ep.self_address();

    std::vector<endpoint_address_t> addresses;
    for(int i=0; i<K; i++){
        if(i!=ep.self_address()){
            addresses.push_back(i);
        }
    }

    std::mt19937 rng((size_t)&ep);

    unsigned received=0, sent=0;
    double sumLat=0;
    double sumLatSqr=0;

    double t_now;

    auto on_recv = [&](const message_t &m)->bool{
        received++;
        double sent=*(const double*)m.payload;
        double latency=t_now-sent;
        sumLat+=latency;
        sumLatSqr+=latency*latency;
        return false;
    };

    while(sent < n){
        auto ready=ep.wait(RECV|SEND);
        if(ready&SEND){
            t_now=now();
            MessageList togo;
            do{
                Message *m=ep.get_send_buffer();
                if(!m){
                    break;
                }
                unsigned len=64;
                *(double*)m->payload=t_now;
                m->set_header(addresses.size(), &addresses[0], 19937, len);
                sent++;
                togo.push_back(m);
            }while(sent < n);
            ep.send(std::move(togo));
        }
        if(ready&RECV){
            t_now=now();
            ep.receive(on_recv);
        }
    }
    
    while(received != (K-1)*n ) {
        ep.wait(RECV|TIMEOUT, 1e-2);
        t_now=now();
        ep.receive(on_recv);
    }

    ep.wait(FLUSHED, 0);

    double meanLat=sumLat/n;
    double stddevLat=sqrt(sumLatSqr/n - meanLat*meanLat);
    fprintf(stderr, "latency, mean=%lf, stddev=%lf\n", meanLat, stddevLat);
}

int main()
{
    const int K=4;

    int mesh[K][K];
    for(int i=0; i<K;i++){
        mesh[i][i]=-1;
    }
    for(int i=0; i<K-1; i++){
        for(int j=i+1; j<K; j++){
            int pipes[2];
            socketpair(AF_UNIX, SOCK_STREAM, 0, pipes);
            mesh[i][j]=pipes[0];
            mesh[j][i]=pipes[1];
        }
    }

    std::vector<RoutingFileChannelEndpoint> eps;

    for(int i=0; i<K; i++){
        eps.emplace_back(i, K, mesh[i]);
    }

    unsigned to_send=10000000;

    double start=now();

    std::vector<std::thread> workers;

    for(unsigned i=0; i<K; i++){
        int ii=i;
        workers.emplace_back([&,ii](){
            worker(K, eps[ii], to_send);
        });
    }

    for(unsigned i=0; i<K; i++){
        workers[i].join();
    }

    double t=now()-start;

    size_t total_sent=to_send*(K-1)*(K-1);
    fprintf(stderr, "n=%u, t=%g, d=%u, msg/sec=%g, bytes/sec=%g\n",
        2*to_send, t, total_sent, total_sent/t, total_sent*L/t);

}