#include "file_channel_endpoint.hpp"

#include <iostream>
#include <random>

int main()
{
    int pipes_A[2];
    int pipes_B[2];

    pipe(pipes_A);
    pipe(pipes_B);

    FileChannelEndpoint ep1(pipes_A[0], pipes_B[1], 0);
    FileChannelEndpoint ep0(pipes_B[0], pipes_A[1], 1);

    unsigned sent0=0;
    unsigned sent1=0;
    unsigned recv0=0;
    unsigned recv1=0;
    unsigned sent0_bytes=0;
    unsigned sent1_bytes=0;
    unsigned recv0_bytes=0;
    unsigned recv1_bytes=0;

    auto recv_on_0 = [&](const message_t &m)->bool{
        recv0++;
        recv0_bytes+=m.payload_length();
        return false;
    };
    auto recv_on_1 = [&](const message_t &m)->bool{
        recv1++;
        recv1_bytes+=m.payload_length();
        return false;
    };

    std::mt19937 rng;

    for(int i=0; i<10000; i++){
        switch(rng()%8){
            case 0:
            case 1:
            case 2:
            {
                Message *m=ep1.get_send_buffer();
                if(m){
                    unsigned len=rng()%MAX_MESSAGE_SIZE;
                    m->set_header(0, 19937, len);
                    ep1.send(m);
                    sent0++;
                    sent0_bytes+=len;
                }
                break;
            }
            case 3:
            case 4:
            case 5:
            {
                Message *m=ep0.get_send_buffer();
                if(m){
                    unsigned len=rng()%MAX_MESSAGE_SIZE;
                    m->set_header(0, 19937, len);
                    ep0.send(m);
                    sent1++;
                    sent1_bytes+=len;
                }
                break;
            }
            case 6:
                ep0.receive(recv_on_0);
                break;
            case 7:
                ep1.receive(recv_on_1);
                break;
            }
        }

    while(sent1 != recv1 || sent0 != recv0 ) {
        ep0.wait(RECV|TIMEOUT, 1e-2);
        ep1.wait(RECV|TIMEOUT, 1e-2);
        ep0.receive(recv_on_0);
        ep1.receive(recv_on_1);
    }

    assert(sent1_bytes == recv1_bytes);
    assert(sent0_bytes == recv0_bytes);
}