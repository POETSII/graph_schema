#include "base85_codec.hpp"

#include <algorithm>
#include <vector>
#include <random>
#include <iostream>

#include <time.h>

double now()
{
  timespec ts;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return ts.tv_nsec*1e-9 + ts.tv_sec;
}

int main()
{
  Base85Codec codec;

  std::mt19937_64 rng;

  std::vector<uint8_t> input;
  for(int i=0;i<1000000000; i++){
    input.push_back(rng());
  }
  std::vector<char> output;
  output.reserve(codec.get_max_encoded_size(input.size()));

  std::cerr<<"Encode 1000M\n";
  double t=now();
  codec.encode_bytes(input.size(), &input[0], output);
  double dt=now()-t;
  std::cerr<<"  t="<<dt<<", = "<<input.size()/dt/1000000<<" MB/sec\n";

  t=now();
  std::cerr<<"Decode 1000M\n";
  {
    const char *begin=&output[0], *end=&output[output.size()];
    codec.decode_bytes(begin, end,input.size(), &input[0] );
  }
  dt=now()-t;
  std::cerr<<"  t="<<dt<<", = "<<input.size()/dt/1000000<<" MB/sec\n";

  for(int w=0; w<=64; w++){
    std::cerr<<"Width="<<w<<"\n";
    uint64_t mask=(w==0) ? uint64_t(0) : ((~uint64_t(0))>>(64-w));

    std::vector<char> encoded;
    for(int j=0; j< 1000000; j++){
      uint64_t val=rng()&mask;

      encoded.clear();
      codec.encode_bits(w, val, encoded);

      const char *begin=encoded.data(), *end=encoded.empty() ? nullptr : encoded.data()+encoded.size();
      auto got=codec.decode_bits(begin, end, w);
      assert(begin==end);
      assert(got==val);

      if(w>1){
        int shift=rng()%(w-1);
        val=val>>shift;

        encoded.clear();
        codec.encode_bits(w, val, encoded);

        const char *begin=encoded.data(), *end=encoded.empty() ? nullptr : encoded.data()+encoded.size();
        auto got=codec.decode_bits(begin, end, w);
        assert(begin==end);
        assert(got==val);
      }
    }
  }

  for(int i=0; i<16; i++){
    std::cerr<<"Len="<<i<<"\n";

    double sumSizes=0;
    std::vector<uint8_t> input(i);
    std::vector<char> encoded(2*i);
    std::vector<uint8_t> output(i);

    unsigned N=100000;
    for(int j=0; j<N; j++){
      std::generate(input.begin(), input.end(), [&](){
        auto tmp=rng();
        if(tmp&1){
          tmp>>=1;
        }else{
          tmp=0;
        }
        return tmp;
      });

      encoded.clear();
      codec.encode_bytes(input.size(), input.empty() ? nullptr : input.data(), encoded);  
      sumSizes += encoded.size();      

      {
        const char *begin=encoded.empty() ? nullptr : encoded.data();
        const char *end=encoded.empty() ? nullptr : encoded.data()+encoded.size();
        codec.decode_bytes(begin, end, input.size(), input.empty() ? nullptr : &output[0]);
      }

      assert(input==output);
    }
    std::cerr<<"Expansion = "<<sumSizes/(i*N)<<"\n";
  }
}