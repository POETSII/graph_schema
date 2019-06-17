$(eval $(call provider_rules_template,hello_world))


ALL_hello_world_TESTS += 

hello_world_tests : $(ALL_hello_world_TESTS)

hello_world_demos : $(ALL_hello_world_DEMOS)

ALL_TESTS += hello_world_tests

ALL_DEMOS += hello_world_demos



