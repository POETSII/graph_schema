

$(eval $(call provider_rules_template,apsp))

define apsp_instance_template
# $1 = params
# $2 = snapshotdelta
# $3 = maxsteps

demos/apsp/apsp_$1.xml : apps/apsp/create_apsp_instance.py apps/apsp/apsp_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/apsp/create_apsp_instance.py  $$(subst _, ,$1) > $$@

demos/apsp/apsp_$1.snap : demos/apsp/apsp_$1.xml bin/epoch_sim providers/apsp.graph.so demos/apsp/apsp_$1.checked
	bin/epoch_sim --stats-delta 1000 --log-level 0 --max-steps $3 --snapshots $2 $$@  demos/apsp/apsp_$1.xml

ALL_apsp_TESTS += demos/apsp/apsp_$1.snap

endef


$(eval $(call apsp_instance_template,2_2,100,10000))
$(eval $(call apsp_instance_template,8_2,100,10000))
$(eval $(call apsp_instance_template,7_7,100,10000))
$(eval $(call apsp_instance_template,11_3,100,10000))
$(eval $(call apsp_instance_template,100_10,100,10000))
$(eval $(call apsp_instance_template,200_30,100,10000))



apsp_tests : $(ALL_apsp_TESTS)

ALL_TESTS += apsp_tests




