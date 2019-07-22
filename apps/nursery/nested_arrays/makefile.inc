$(eval $(call provider_rules_template,nested_arrays,apps/nursery/nested_arrays/nested_arrays_graph_type.xml,no-default))


ALL_nested_arrays_TESTS += 

nested_arrays_tests : $(ALL_nested_arrays_TESTS)

nested_arrays_demos : $(ALL_nested_arrays_DEMOS)

ALL_TESTS += nested_arrays_tests

ALL_DEMOS += nested_arrays_demos



