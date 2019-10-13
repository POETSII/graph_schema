

$(eval $(call provider_rules_template,ising_spin_fix_ext,apps/nursery/ising_spin_fix_ext/ising_spin_fix_ext_graph_type.xml,no-default))

providers/ising_spin_fix_ext_img_static_external.so : apps/nursery/ising_spin_fix_ext/ising_spin_fix_ext_img_static_external.cpp
	$(CXX) -c $(CPPFLAGS) $(SO_CPPFLAGS) $< -o providers/ising_spin_fix_ext_img_static_external.o
	$(CXX) $(SO_CPPFLAGS) providers/ising_spin_fix_ext_img_static_external.o -o $@ $(LDFLAGS) $(SO_LDFLAGS)

ising_spin_fix_ext_provider : providers/ising_spin_fix_ext_img_static_external.so


providers/ising_spin_fix_ext_img_dynamic_external.so : apps/nursery/ising_spin_fix_ext/ising_spin_fix_ext_img_dynamic_external.cpp
	$(CXX) -c $(CPPFLAGS) $(SO_CPPFLAGS) $< -o providers/ising_spin_fix_ext_img_dynamic_external.o
	$(CXX) $(SO_CPPFLAGS) providers/ising_spin_fix_ext_img_dynamic_external.o -o $@ $(LDFLAGS) $(SO_LDFLAGS)

ising_spin_fix_ext_provider : providers/ising_spin_fix_ext_img_dynamic_external.so

