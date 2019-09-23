#ifndef poets_protocol__types_hpp
#define poets_protocol__types_hpp

#include <cstdint>
#include <iostream>


struct halt_message_type
{
    int32_t code;
    uint32_t param1;
    uint32_t param2;
    uint32_t param3;
    char description[24];
};

struct poets_device_address_t{
    uint32_t value;
    
    poets_device_address_t()
        : value(-1)
    {}

    explicit poets_device_address_t(uint32_t _value)
        : value(_value)
    {}

    bool operator<(const poets_device_address_t &o) const
    { return value<o.value; }

    bool operator==(const poets_device_address_t o) const
    { return value==o.value; }

    bool operator!=(const poets_device_address_t o) const
    { return value!=o.value; }
};

struct poets_pin_index_t{
    uint32_t value=-1;

    poets_pin_index_t()
        : value(-1)
    {}

    explicit poets_pin_index_t(uint32_t _value)
        : value(_value)
    {}

    bool operator<(const poets_pin_index_t &o) const
    { return value<o.value; }


    bool operator==(const poets_pin_index_t o) const
    { return value==o.value; }

    bool operator!=(const poets_pin_index_t o) const
    { return value!=o.value; }

};

struct poets_endpoint_address_t{
    uint64_t value;

    poets_endpoint_address_t()
        : value(-1)
    {}

    explicit poets_endpoint_address_t(uint64_t _value)
        : value(_value)
    {}

    bool operator<(const poets_endpoint_address_t &o) const
    { return value<o.value; }

    bool operator==(const poets_endpoint_address_t o) const
    { return value==o.value; }

    bool operator!=(const poets_endpoint_address_t o) const
    { return value!=o.value; }

};

inline poets_device_address_t getEndpointDevice(poets_endpoint_address_t ep)
{ return poets_device_address_t(uint32_t(ep.value>>32)); }

inline poets_pin_index_t getEndpointPin(poets_endpoint_address_t ep)
{ return poets_pin_index_t(uint32_t(ep.value&0xFFFFFFFFul)); }

inline poets_endpoint_address_t makeEndpoint(poets_device_address_t address, poets_pin_index_t pin)
{ return poets_endpoint_address_t( (uint64_t(address.value)<<32) + pin.value); }

inline std::ostream &operator<<(std::ostream &dst, poets_endpoint_address_t a)
{           
    dst<<a.value;
    return dst;
}

inline std::ostream &operator<<(std::ostream &dst, poets_device_address_t a)
{
    dst<<a.value;
    return dst;
}

inline std::ostream &operator<<(std::ostream &dst, poets_pin_index_t a)
{
    dst<<a.value;
    return dst;
}

inline std::istream &operator>>(std::istream &dst, poets_endpoint_address_t &a)
{
    dst>>a.value;
    return dst;
}

inline std::istream &operator>>(std::istream &dst, poets_device_address_t &a)
{
    unsigned long wtf;
    dst>>wtf;
    a.value=wtf;
    return dst;
}

inline std::istream &operator>>(std::istream &dst, poets_pin_index_t &a)
{
    unsigned long grrr;
    dst>>grrr;
    a.value=grrr;
    return dst;
}

namespace std
{
    template<>
    struct hash<poets_device_address_t>
    {
        size_t operator()(poets_device_address_t x) const
        { return std::hash<decltype(x.value)>()(x.value); }
    };

    template<>
    struct hash<poets_endpoint_address_t>
    {
        size_t operator()(poets_endpoint_address_t x) const
        { return std::hash<decltype(x.value)>()(x.value); }
    };
};

inline std::string getEndpointDevice(const std::string &endpoint)
{
    auto colon=endpoint.find(':');
    if(colon==std::string::npos || colon==0 || (colon+1u==endpoint.size())){
        throw std::runtime_error("String is not a colon-seperated endpoint address.");
    }
    return endpoint.substr(0,colon);
}

inline std::string getEndpointPin(const std::string &endpoint)
{
    auto colon=endpoint.find(':');
    if(colon==std::string::npos || colon==0 || (colon+1u==endpoint.size())){
        throw std::runtime_error("String is not a colon-seperated endpoint address.");
    }
    return endpoint.substr(colon+1);
}


inline void poets_require_msg(bool cond, const char *message)
{
    if(!cond){
        throw std::runtime_error(message);
    }
}

#define poets_require(cond) poets_require_msg((cond), #cond)


#endif
