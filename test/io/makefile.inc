

$(eval $(call provider_rules_template,test_io,test/io/test_io_graph_type.xml))

test_io_tests : $(ALL_test_io_TESTS)

test_io_demos : $(ALL_test_io_DEMOS)

ALL_TESTS += test_io_tests

ALL_DEMOS += test_io_demos



