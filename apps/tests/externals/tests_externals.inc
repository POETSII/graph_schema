%.external.so : %.external.cpp
	echo ${POETS_EXTERNAL_INTERFACE_SPEC}
	$(CXX) $(CPPFLAGS) $< -o $@ $(SO_CPPFLAGS) $(LDFLAGS) $(LDLIBS)

all_externals : apps/tests/externals/test_external_ping_pong_graph_inst_1.external.so
