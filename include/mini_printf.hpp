#ifndef graph_schema_mini_printf_hpp
#define graph_schema_mini_printf_hpp

/////////////////////////////////////
// Begin workaround for orchestrator handler_log to allow values to be printed
// See graph_schema/include/mini_printf.hpp

#include <stdint.h>
#include <stdarg.h>

namespace mini_printf
{

    inline void tinsel_div_mod_10(uint32_t x, uint32_t &div, uint32_t &rem)
    {
        const uint64_t m=3435973837ul;
        uint32_t mx=(uint64_t(x)*m)>>32;
        mx=mx>>3;
        div=mx;
        rem = x - mx*10u;
    }

    inline char *format_arg_str(char *dst, const char *s)
    {
        while(1){
            char ch=*s++;
            if(!ch){
                return dst;
            }
            *dst++=ch;
        }
    }

    inline char *format_arg_hex(char *dst, uint32_t x)
    {
        if(x==0){
            *dst++='0';
        }else{
            char buffer[8];
            char *pos=buffer;
            while(x){
                uint32_t digit=x&0xF;
                char bias=( (digit < 10) ? '0' : ('a'-10) );
                *pos++ = digit + bias;
                x=x>>4;
            }

            while(pos!=buffer){
                *dst++ = *--pos;
            }
        }
        return dst;
    }

    inline char *format_arg_dec(char *dst, char code, uint32_t x)
    {
        if(code=='d' && (x>>31)){
            *dst++ = '-';
            x=0-static_cast<int32_t>(x);
        }

        if(x==0){
            *dst++ = '0';
        }else{
            char digits[10];
            char *pos=digits;

            const uint64_t m=3435973837ul;
            while(x){
                uint32_t digit;
                uint32_t div=(uint64_t(x)*m)>>(32+3);
                digit=x-div*10;
                x=div;
                *pos++ = '0' + digit;
            }

            while(pos!=digits){
                *dst++ = *--pos;
            }
        }
        return dst;
    }

}; // mini_printf

inline char *mini_vsprintf(char *dst, const char *fmt, va_list args)
{
    char ch=1;
    while(ch){
        ch=*fmt++;
        if(ch=='%'){
            char code=*fmt++;
            switch(code){
            case 'u':
            case 'd':
            case 'x':
                {
                    uint32_t x=va_arg(args, unsigned);
                    if(code=='x'){
                        dst=mini_printf::format_arg_hex(dst, x);
                    }else{
                        dst=mini_printf::format_arg_dec(dst, code, x);
                    }
                    break;
                }
            case 's':
                {
                    const char *p=va_arg(args, const char *);
                    dst=mini_printf::format_arg_str(dst, p);
                    break;
                }
            case '%':
                {
                    *dst++=code;
                    break;
                }
            default: // unknown/unsupported format string. Play it safe.
                {
                    dst=mini_printf::format_arg_str(dst, fmt-2); // Print out string starting at %
                    ch=0; // Appear as if string has ended.
                    break;
                }
            }
        }else{
            *dst++=ch;
        }
    }

    return dst;
}

inline char *mini_sprintf(char *dst, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *res=mini_vsprintf(dst, fmt, args);
    va_end(args);
    return res;
}

// hacky way to get handler_log working in orchestrator
#if defined(handler_log) && defined(P_LOG_LEVEL)

    #define handler_log_orig handler_log

    inline char *handler_log_orch_workaround_impl(int level, uint32_t src, const char *fmt, ...)
    {
        char buffer[256]; // TODO : a bit fragile, but stack goes down...?
        va_list args;
        va_start(args, fmt);
        char *res=mini_vsprintf(buffer, fmt, args);
        const char *msg=buffer;
        softswitch_trivial_log_handler(src, msg);
        va_end(args);
        return res;
    }

    #define handler_log(level,  ...) \
        if(level <= P_LOG_LEVEL){ \
            handler_log_orch_workaround_impl(level, deviceInstance->deviceIdx, __VA_ARGS__); \
        }


#endif

// End workaround for orchestrator handler_log to allow values to be printed
/////////////////////////////////////////////////////////////

#endif
