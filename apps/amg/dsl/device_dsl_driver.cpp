#include "device_dsl_driver.hpp"
#include "device_dsl_parser.tab.hpp"

device_dsl_driver::device_dsl_driver ()
  : trace_scanning (false), trace_parsing (false)
{
}

device_dsl_driver::~device_dsl_driver ()
{
}

int
device_dsl_driver::parse (const std::string &f)
{
  file = f;
  scan_begin ();
  yy::device_dsl_parser parser (*this);
  parser.set_debug_level (trace_parsing);
  int res = parser.parse ();
  scan_end ();
  return res;
}

void
device_dsl_driver::error (const yy::location& l, const std::string& m)
{
  std::cerr << l << ": " << m << std::endl;
}

void
device_dsl_driver::error (const std::string& m)
{
  std::cerr << m << std::endl;
}
