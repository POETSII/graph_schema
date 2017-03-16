#include <iostream>
#include "device_dsl_driver.hpp"

extern void enum_states(DeviceTypePtr device);


int main (int argc, char *argv[])
{
  int res = 0;
  device_dsl_driver driver;
  for (int i = 1; i < argc; ++i)
    if (argv[i] == std::string ("-p"))
      driver.trace_parsing = true;
    else if (argv[i] == std::string ("-s"))
      driver.trace_scanning = true;
    else if (!driver.parse (argv[i])){
      for(auto d : driver.graph->getDevices()){
        enum_states(d);
      }
    }
    else
      res = 1;
  return res;
}
