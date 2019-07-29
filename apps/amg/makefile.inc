

$(eval $(call provider_rules_template,amg))

define amg_poisson_instance_template

demos/amg/amg_poisson_$1.xml : apps/amg/make_poisson_graph_instance.py apps/amg/amg_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/amg/make_poisson_graph_instance.py  $$(subst _, ,$1) > $$@

ALL_TEST_XML += demos/amg/amg_poisson_$1.xml

demos/amg/amg_poisson_$1.snap : demos/amg/amg_poisson_$1.xml bin/epoch_sim providers/amg.graph.so demos/amg/amg_poisson_$1.checked
	bin/epoch_sim  --log-level 0 --snapshots 100 $$@ --stats-delta 100 demos/amg/amg_poisson_$1.xml

ALL_amg_TESTS += demos/amg/amg_poisson_$1.snap

endef

ifeq ($(NO_APP_TESTS),)

$(eval $(call amg_poisson_instance_template,2_2,$(amg_FILTERS)))
$(eval $(call amg_poisson_instance_template,3_3,$(amg_FILTERS)))
$(eval $(call amg_poisson_instance_template,4_4,$(amg_FILTERS)))
$(eval $(call amg_poisson_instance_template,3_4,$(amg_FILTERS)))
$(eval $(call amg_poisson_instance_template,4_3,$(amg_FILTERS)))
$(eval $(call amg_poisson_instance_template,8_8,$(amg_FILTERS)))
#$(eval $(call amg_poisson_instance_template,16_16,$(amg_FILTERS)))
$(eval $(call amg_poisson_instance_template,32_32,$(amg_FILTERS)))

endif

amg_tests : $(ALL_amg_TESTS)

amg_demos : $(ALL_amg_DEMOS)

ALL_DEMOS += amg_demos

ALL_TESTS += amg_tests
