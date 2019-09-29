$(eval $(call provider_rules_template,test_external_ping_pong,apps/tests/externals/test_external_ping_pong_graph_type.xml))

$(info bleh)

providers/test_external_ping_pong.external.o : apps/tests/externals/test_external_ping_pong_graph_type.external.cpp
	$(CXX) -c $(CPPFLAGS) $< -o $@ -g $(SO_CPPFLAGS)

providers/test_external_ping_pong.external.so : providers/test_external_ping_pong.external.o
	$(CXX) $(CPPFLAGS) $< -o $@ -g $(SO_CPPFLAGS) $(LDFLAGS) $(LDLIBS)

test_external_ping_pong_provider : providers/test_external_ping_pong.external.so

all_externals : providers/test_external_ping_pong.external.so
