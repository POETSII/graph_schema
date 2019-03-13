

$(eval $(call provider_rules_template,betweeness_centrality))


define betweeness_centrality_instance_template
# $1 = params
# $2 = delta
# $3 = maxsteps

demos/betweeness_centrality/betweeness_centrality_$1.xml : apps/betweeness_centrality/create_betweeness_centrality_instance.py apps/betweeness_centrality/betweeness_centrality_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/betweeness_centrality/create_betweeness_centrality_instance.py  $$(subst _, ,$1) > $$@

demos/betweeness_centrality/betweeness_centrality_$1.snap : demos/betweeness_centrality/betweeness_centrality_$1.xml bin/epoch_sim providers/betweeness_centrality.graph.so demos/betweeness_centrality/betweeness_centrality_$1.checked
	bin/epoch_sim  --log-level 0 --max-steps $3 --snapshots $2 $$@  demos/betweeness_centrality/betweeness_centrality_$1.xml

ALL_betweeness_centrality_TESTS += demos/betweeness_centrality/betweeness_centrality_$1.snap

endef


$(eval $(call betweeness_centrality_instance_template,16_16_2_2,1,1000))
$(eval $(call betweeness_centrality_instance_template,16_16_20_20,1,1000))
$(eval $(call betweeness_centrality_instance_template,100_40_4_4,10,1000))
$(eval $(call betweeness_centrality_instance_template,1000_40_100_1,10,3000))



betweeness_centrality_tests : $(ALL_betweeness_centrality_TESTS)

betweeness_centrality_demos : $(ALL_betweeness_centrality_DEMOS)

ALL_TESTS += betweeness_centrality_tests

ALL_DEMOS += betweeness_centrality_demos



