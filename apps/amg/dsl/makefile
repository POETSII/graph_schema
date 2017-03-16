CXXFLAGS += -std=gnu++11 -g

device_dsl_parser.tab.cpp device_dsl_parser.tab.hpp : device_dsl_parser.ypp
	bison $<

device_dsl_parser.yy.cpp : device_dsl_parser.flex
	flex -o $@ $<

device_dsl : device_dsl_parser.yy.cpp device_dsl_parser.tab.cpp device_dsl_driver.cpp enum_states.cpp
