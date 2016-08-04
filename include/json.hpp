#ifndef json_hpp
#define json_hpp

#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <ctype.h>

/* WARNING : Worlds crappiest JSON SAX parser!. This should be replaced
   with rapidjson. */

class JSONEvents
{
public:
  virtual ~JSONEvents()
  {}

  virtual void onScalar(const char *name, const char *value) =0;

  virtual void beginObject(const char *name);
  virtual void endObject(const char *name);
};

class JSONEventsWriter
  : public JSONEvents
{
private:
  enum bind_type
    {
      bind_float,
      bind_bool,
      bind_int32,
      bind_uint32
    };
  
  struct binding_pair
  {
    bind_type type;
    void *dst;
  };

  std::unordered_map<std::string,binding_pair> m_bindings;

  int m_depth=0;
  std::string m_accName;
public:
  JSONEventsWriter()
  {}
  
  void bind(const char *name, float &val)
  { m_bindings.insert(std::make_pair(std::string(name),binding_pair{bind_float,&val})); }

  void bind(const char *name, bool &val)
  { m_bindings.insert(std::make_pair(std::string(name),binding_pair{bind_bool,&val})); }

  void bind(const char *name, int32_t &val)
  { m_bindings.insert(std::make_pair(std::string(name),binding_pair{bind_int32,&val})); }

  void bind(const char *name, uint32_t &val)
  { m_bindings.insert(std::make_pair(std::string(name),binding_pair{bind_uint32,&val})); }

  void onScalar(const char *name, const char *value)
  {
    auto it=m_bindings.find(name);
    if(it==m_bindings.end())
      throw std::runtime_error(std::string("Unknown element ")+name);
    auto &binding=*it;

    std::stringstream acc(value);
    if(binding.second.type==bind_float){
      acc >> *(float*)binding.second.dst;
    }else if(binding.second.type==bind_bool){
      acc >> *(bool*)binding.second.dst;
    }else if(binding.second.type==bind_int32){
      acc >> *(int32_t*)binding.second.dst;
    }else if(binding.second.type==bind_uint32){
      acc >> *(uint32_t*)binding.second.dst;
    }else{
      throw std::runtime_error("Unknown binding type.");
    }
  }

  void beginObject(const char *name)
  {
    if(m_depth==0){
      m_accName=name;
    }else{
      m_accName=m_accName+"."+name;
    }
    m_depth++;
  }

  void endObject(const char *name)
  {
    assert(m_depth>0);
    m_depth--;
    if(m_depth==0){
      m_accName.clear();
    }else{
      m_accName=m_accName.substr(0, m_accName.size()-strlen(name)-1);
    }
  }
};

std::string JSONParseKey(const char *&str)
{
  fprintf(stderr, "JSONParseKey(%s)\n", str);
  
  while(std::isspace(*str))
    str++;
  
  if(*str!='"')
    throw std::runtime_error("Malformed key");
  str++;

  const char *keyStart=str;

  while(*str && *str!='"')
    str++;

  if(!*str)
    throw std::runtime_error("Malformed key");

  const char *keyEnd=str;
  str++;

  return std::string(keyStart,keyEnd);
}

void JSONParsePair(const char *&str, JSONEvents &events)
{
  std::string key=JSONParseKey(str);

  fprintf(stderr, "JSONParsePair/post(%s)\n", str);

  while(*str && isspace(*str))
    str++;
  if(!*str || *str!=':')
    throw std::runtime_error("Malformed pair");
  str++;

  while(*str && isspace(*str))
    str++;
  if(!*str)
    throw std::runtime_error("Missing value");

  if(*str=='[')
    throw std::runtime_error("Arrays not supported.");
  if(*str=='{')
    throw std::runtime_error("Sub-tuples not supported.");
  str++;

  const char *valueStart=str;
  while(*str && *str!=',' && !isspace(*str))
    str++;
  const char *valueEnd=str;

  std::string value(valueStart, valueEnd);

  events.onScalar(key.c_str(), value.c_str());

  fprintf(stderr, "Post\n");
}

void JSONParser(const char *str, JSONEvents &events)
{
  fprintf(stderr, "JSONParser(%s)\n", str);
  
  while(1){
    while(*str && isspace(*str))
      ++str;

    if(!*str){
      fprintf(stderr, "Done\n");
      return;
    }
    
    JSONParsePair(str, events);

    while(*str && isspace(*str))
      ++str;
    if(!*str)
      break;
    if(*str!=',')
      throw std::runtime_error("Expected comma");
    ++str;
  }
}


#endif
