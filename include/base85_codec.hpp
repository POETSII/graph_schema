#ifndef base85_codec_hpp
#define base85_codec_hpp

#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <cassert>
#include <string>
#include <array>
#include <vector>

struct Base85Codec
{
  static const unsigned BASE=85;
  static const char ZERO='_';
  const char C_ID_TERMINATOR='@';

  char forwards[BASE];
  uint8_t backwards[256];

  std::array<unsigned,64> width_to_digits;

  Base85Codec()
  {
    static const char *chars=
    "abcdefghijklmnopqrstuvwxyz" // 26
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26
    "0123456789"   // 10
    "!\"#$%'()*+" // 10
    ",-./:;<=>?"  // 10
    "@\\^"; // b

    if(strlen(chars)!=85){
      throw std::logic_error("expected exactly BASE chars.");
    }
    if(strchr(chars,ZERO)!=0){
      throw std::logic_error("chars contained zero char.");
    }
    if(strchr(chars,']')!=0){
      throw std::logic_error("chars contains ] character, so could break out of CDATA.");
    }

    std::fill(forwards, forwards+BASE, 0);
    std::fill(backwards, backwards+256, 0xFF);

    for(unsigned i=0; i<BASE; i++){
        if(!isprint(chars[i])){
            throw std::runtime_error("Char is not printable.");
        }
      forwards[i]=chars[i];
      if(backwards[(unsigned)chars[i]]!=0xFF){
          std::string tmp(1,chars[i]);
        throw std::logic_error("Repeated char : "+tmp);
      }
      backwards[(unsigned)chars[i]]=i;
    }

    // Build a table saying how many digits are needed for each width of integer
    int digits=0;
    __int128_t digits_max=1;
    int bits=0;
    __int128_t bits_max=1;
    while(bits < 64){
        if(bits_max < digits_max){
            width_to_digits[bits]=digits;
            bits++;
            bits_max <<= 1;
        }else{
            digits++;
            digits_max *= BASE;
        }
    }
  }

  size_t get_max_encoded_size(size_t nBytes) const
  {
    uint32_t full=nBytes/4;
    uint32_t partial=nBytes%4;

    const uint32_t partial_size[4]={0, 2, 3, 4 };
    return full*5 + partial_size[partial];
  }

  char *encode_bytes(size_t nBytes, const uint8_t *src, char *dst) const
  {
    const uint8_t *end=src+nBytes;

    unsigned full=nBytes/4;
    unsigned partial=nBytes%4;

    for(unsigned j=0; j<full; j++){
        uint_fast32_t acc=*(const uint32_t*)src;
        for(unsigned i=0; i<5; i++){
            if(acc==0){
                *dst++=ZERO;
                break;
            }
            uint_fast32_t digit=acc % BASE;
            acc=acc/BASE;
            *dst++ = forwards[digit];
        }
      src+=4;
    }

    if(partial){
        unsigned partial_digits=partial+1;
        uint32_t acc=0;
        memcpy(&acc, src, partial);
        for(unsigned i=0; i<partial_digits; i++){
            if(acc==0){
                *dst++=ZERO;
                break;
            }
            uint_fast32_t digit=acc % BASE;
            acc=acc/BASE;
            *dst++ = forwards[digit];
        }
    }

    return dst;
  }

  void encode_bytes(size_t nBytes, const uint8_t *data, std::vector<char> &dst) const
  {
    if(nBytes!=0){
        size_t orig_size=dst.size();
        dst.resize(orig_size + get_max_encoded_size(nBytes));
        
        auto pos=&dst[orig_size];
        pos=encode_bytes(nBytes, data, pos);
        size_t n=pos-&dst[0];
        assert(n <= dst.size());
        dst.resize(n);
    }
  }

  void encode_digit(unsigned value, std::vector<char> &dst) const
  {
      assert(value < BASE);
      dst.push_back(forwards[value]);
  }

  // It can be slight more efficient if you know exactly how many bits you
  // have. For example, encoding 19 bits requires 3 digits, while encoding
  // it as 3 bytes requires 4 bytes.
  void encode_bits(unsigned width, uint64_t bits, std::vector<char> &dst) const
  {
    if(width==0){
        return;
    }
    assert(width<=64);
    unsigned digits=width_to_digits[width];
    assert(digits>0);

    size_t orig_len=dst.size();
    dst.resize(orig_len+digits);

    char *pout=dst.data()+orig_len;

    for(unsigned i=0; i<digits; i++){
        if(bits==0){
            *pout++=ZERO;
            break;
        }
        uint_fast32_t digit=bits % BASE;
        bits=bits/BASE;
        *pout++ = forwards[digit];
    }
    assert(dst.data()+orig_len < pout);
    assert(pout <= dst.data()+dst.size());

    dst.resize(pout-dst.data());
  }

    // We just encode as the chars, followed by C_ID_TERMINATOR
    // Encoding the length always takes at least one byte anyway
  void encode_c_identifier(const std::string &s, std::vector<char> &dst) const
  {
      dst.insert(dst.end(), dst.begin(), dst.end());
      dst.push_back(C_ID_TERMINATOR);
  }

  void decode_bytes(const char *&begin, const char *end, size_t nBytes, uint8_t *dst) const
  {
    unsigned full=nBytes/4;
    unsigned partial=nBytes%4;

    uint8_t cross_or=0; // bitwise or across all decoded digits. If it is 0xFF then data was invalid.

    const unsigned char *usrc=(const unsigned char*)begin;
    const unsigned char *uend=(const unsigned char*)end;

    for(unsigned j=0; j<full; j++){
      uint_fast32_t acc=0;
      uint_fast32_t scale=1;
      for(int i=0; i<5; i++){
        if(usrc==uend){
          throw std::runtime_error("Out of chars.");
        }
        auto uch=*usrc++;
        if(uch==(unsigned char)ZERO){
            break;
        }
        auto digit=backwards[uch];
        cross_or |= digit;
        acc += scale * digit;
        scale *= BASE;
      }
      *(uint32_t*)dst=acc;
      dst+=4;
    }

    if(partial>0){
        assert(partial < 5);
        uint_fast32_t acc=0;
        uint_fast32_t scale=1;
        for(unsigned i=0; i<(partial+1); i++){
          if(usrc==uend){
            throw std::runtime_error("Out of chars.");
          }
          auto uch=*usrc++;
          if(uch==(unsigned char)ZERO){
              break;
          }
          auto digit=backwards[uch];
          cross_or |= digit;
          acc += scale * digit;
          scale *= BASE;
        }
        memcpy(dst, &acc, partial);
    }

    if(cross_or==0xFF){
        throw std::runtime_error("Invalid digits in base85 string.");
    }
    
    begin=(const char *)usrc;
  }

  unsigned decode_digit(const char *&begin, const char *end) const
  {
    if(begin>=end){
      throw std::runtime_error("Not enough chars.");
    }
    auto digit=backwards[(unsigned)*begin++];
    if(digit==0xFF){
      throw std::runtime_error("Invalid char.");
    }
    return digit;
  }

  uint64_t decode_bits(const char *&begin, const char *end, unsigned width) const
  {
    if(width==0){
       return 0;
    }
    assert(width<=64);
    unsigned digits=width_to_digits[width];
    assert(digits>0);

    uint8_t cross_or=0;

    uint64_t acc=0;
    uint64_t scale=1;
    for(unsigned i=0; i<digits; i++){
        if(begin==end){
          throw std::runtime_error("Out of characters.");
        }
        char c=*begin++;
        if(c==ZERO){
            break;
        }
        auto digit=backwards[(unsigned char)c];
        cross_or |= digit;
        acc += scale * digit;
        scale *= BASE;
    }

    if(cross_or==0xFF){
        throw std::runtime_error("Illegal base85 characters.");
    }

    return acc;
  }

  void decode_c_identifier(const char *&src, const char *end, std::string &dst)
  {
    assert(src < end);
    dst.clear();
    
    while(1){
      if(src==end){
        throw std::runtime_error("Corrupt C identifier.");
      }
      char ch=*src++;
      if(ch==C_ID_TERMINATOR){
        break;
      }else{
        // TODO : unsafe for untrusted input?
        assert(isprint(ch));
        dst.push_back(ch);
      }
    }
  }
};

#endif
