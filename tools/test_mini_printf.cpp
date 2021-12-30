#include "mini_printf.hpp"

#include <iostream>
#include <random>
#include <cassert>
#include <sstream>

std::string make_random_string(std::mt19937_64 &rng)
{
    std::string res;
    while(1){
        if(rng()&1){
            break;
        }
        char ch=(char)rng();
        while(ch=='%' || !(isprint(ch) || iswspace(ch))){
            ch=(char)rng();
        }
        res.push_back(ch);
    }
    return res;
}

std::pair<std::string,std::string> make_format_string(std::mt19937_64 &rng, const std::vector<std::pair<std::string,std::string>> &codes_and_reps)
{
    std::string fmt;
    std::string exp;

    std::string part=make_random_string(rng);
    fmt += part;
    exp += part;
    for(const auto &cr : codes_and_reps){
        fmt += "%"+cr.first;
        exp += cr.second;
        part=make_random_string(rng);
        fmt += part;
        exp += part;
    }

    return {fmt,exp};
}

void test1(std::mt19937_64 &rng)
{
    char buffer[256];

    for(unsigned i=0; i<10000; i++){
        auto rr=make_format_string(rng,{});
        mini_sprintf(buffer, rr.first.c_str());
        if(buffer!=rr.second){
            std::cerr<<"i="<<i<<", fmt="<<rr.first<<", got="<<buffer<<"\n";
        }
        assert(buffer==rr.second);
    }
}

void test2(std::mt19937_64 &rng)
{
    char buffer[256];

    for(unsigned i=0; i<10000; i++){
        uint32_t x=rng();
        x=x>>(rng()%32);
        std::string ref=std::to_string(x);

        auto rr=make_format_string(rng,{{"u", ref}});
        mini_sprintf(buffer, rr.first.c_str(), x);
        if(buffer!=rr.second){
            std::cerr<<"i="<<i<<", fmt="<<rr.first<<", x="<<x<<", got="<<buffer<<"\n";
        }
        assert(buffer==rr.second);
    }
}


void test3(std::mt19937_64 &rng)
{
    char buffer[256];

    for(unsigned i=0; i<10000; i++){
        int32_t x=static_cast<int32_t>(uint32_t(rng()));
        x=x>>(rng()%31);
        std::string ref=std::to_string(x);

        auto rr=make_format_string(rng,{{"d", ref}});
        mini_sprintf(buffer, rr.first.c_str(), x);
        if(buffer!=rr.second){
            std::cerr<<"i="<<i<<", fmt="<<rr.first<<", x="<<x<<", got="<<buffer<<"\n";
        }
        assert(buffer==rr.second);
    }
}

void test4(std::mt19937_64 &rng)
{
    char buffer[256];

    for(unsigned i=0; i<10000; i++){
        uint32_t x=(uint32_t(rng()));
        x=x>>(rng()%32);

        std::stringstream ss;
        ss<<std::hex<<x;
        std::string ref=ss.str();

        auto rr=make_format_string(rng,{{"x", ref}});
        mini_sprintf(buffer, rr.first.c_str(), x);
        if(buffer!=rr.second){
            std::cerr<<"i="<<i<<", fmt="<<rr.first<<", x="<<x<<", got="<<buffer<<", exp="<<rr.second<<"\n";
        }
        assert(buffer==rr.second);
    }
}

void test5(std::mt19937_64 &rng)
{
    char buffer[256];

    for(unsigned i=0; i<10000; i++){
        std::string x=make_random_string(rng);

        auto rr=make_format_string(rng,{{"s", x}});
        mini_sprintf(buffer, rr.first.c_str(), x.c_str());
        if(buffer!=rr.second){
            std::cerr<<"i="<<i<<", fmt="<<rr.first<<", x="<<x<<", got="<<buffer<<", exp="<<rr.second<<"\n";
        }
        assert(buffer==rr.second);
    }
}

void test6(std::mt19937_64 &rng)
{
    char buffer[1024];

    auto hex=[=](uint32_t x)
    {
        std::stringstream s;
        s<<std::hex<<x;
        return s.str();  
    };

    for(unsigned i=0; i<10000; i++){
        std::vector<std::pair<std::string,std::string>> args;
        std::string x0=make_random_string(rng);
        args.push_back({"s",x0});
        uint32_t x1=rng();
        args.push_back({"u",std::to_string(x1)});
        int32_t x2=static_cast<int32_t>(uint32_t(rng()));
        args.push_back({"d",std::to_string(x2)});
        uint32_t x3=rng();
        args.push_back({"x",hex(x3)});
        std::string x4=make_random_string(rng);
        args.push_back({"s",x4});

        auto rr=make_format_string(rng,args);
        mini_sprintf(buffer, rr.first.c_str(), x0.c_str(), x1, x2, x3, x4.c_str());
        if(buffer!=rr.second){
            std::cerr<<"i="<<i<<", fmt="<<rr.first<<",got="<<buffer<<", exp="<<rr.second<<"\n";
        }
        assert(buffer==rr.second);
    }
}




int main()
{
    std::mt19937_64 rng;

    test1(rng);
    test2(rng);
    test3(rng);
    test4(rng);
    test5(rng);
    test6(rng);
}