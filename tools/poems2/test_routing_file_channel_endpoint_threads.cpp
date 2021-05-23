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

void worker(Endpoint &ep, unsigned n, unsigned &bytes_sent, unsigned &bytes_received)
{
    endpoint_address_t self=ep.self_address();

    std::mt19937 rng((size_t)&ep);

    unsigned received=0, sent=0;
    double sumLat=0;
    double sumLatSqr=0;

    double t_now;

    auto on_recv = [&](const message_t &m)->bool{
        received++;
        bytes_received+=m.payload_length();
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
                unsigned len=64;//(8+(rng()%(MAX_MESSAGE_SIZE-8)));
                *(double*)m->payload=t_now;
                m->set_header(1-self, 19937, len);
                sent++;
                bytes_sent+=len;
                togo.push_back(m);
            }while(sent < n);
            ep.send(std::move(togo));
        }
        if(ready&RECV){
            t_now=now();
            ep.receive(on_recv);
        }
    }
    
    while(received != n ) {
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
    int pipes[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, pipes);

    socklen_t len=sizeof(int), val;
    getsockopt (pipes[0], SOL_SOCKET, SO_SNDBUF, (char *) &val, &len);
    std::cerr<<"SO_SNDBUF = "<<val<<std::endl;
    //setsockopt (pipes[0], SOL_SOCKET, SO_SNDBUF, (char *) &val, sizeof (int));

    int fds0[2]={-1, pipes[0]};
    RoutingFileChannelEndpoint ep0(0, 2, fds0);

    int fds1[2]={pipes[1], -1};
    RoutingFileChannelEndpoint ep1(1, 2, fds1);

    unsigned sent0_bytes=0;
    unsigned sent1_bytes=0;
    unsigned recv0_bytes=0;
    unsigned recv1_bytes=0;

    unsigned to_send=10000000;

    double start=now();

    std::thread t0([&](){
        worker(ep0, to_send, sent1_bytes, recv0_bytes);
    });
    std::thread t1([&](){
        worker(ep1, to_send, sent0_bytes, recv1_bytes);
    });

    t0.join();
    t1.join();

    double t=now()-start;

    fprintf(stderr, "n=%u, t=%g, d=%u, msg/sec=%g, bytes/sec=%g\n",
        2*to_send, t, recv0_bytes+sent0_bytes, (to_send*2)/t, (recv0_bytes+sent0_bytes)/t);

    assert(sent1_bytes == recv1_bytes);
    assert(sent0_bytes == recv0_bytes);
}