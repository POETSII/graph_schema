

$(eval $(call provider_rules_template,self_clocked_izhikevich,apps/nursery/self_clocked_izhikevich/self_clocked_izhikevich_graph_type.xml,no-default))

define self_clocked_izhikevich_instance_template	
# $1 = params
# $2 = maxsteps
# $5 = filters

demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.xml : apps/nursery/self_clocked_izhikevich/create_sparse_instance.py apps/nursery/self_clocked_izhikevich/self_clocked_izhikevich_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/nursery/self_clocked_izhikevich/create_sparse_instance.py  $$(subst _, ,$1) > $$@

ALL_TEST_XML += demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.xml

demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.snap : demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.xml bin/epoch_sim providers/self_clocked_izhikevich.graph.so demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.checked
	bin/epoch_sim --stats-delta 1000 --max-steps $2 --log-level 0 --snapshots 1000 $$@  demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.xml

ALL_self_clocked_izhikevich_TESTS += demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.snap

demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.gif : demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.xml demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.snap
	mkdir -p demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.xml --snapshots demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.snap --output demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.tmp/out  \
		$3
	(cd demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../self_clocked_izhikevich_$1.gif)
	
ALL_self_clocked_izhikevich_DEMOS += demos/self_clocked_izhikevich/self_clocked_izhikevich_$1.gif
	
endef


self_clocked_izhikevich_FILTERS = \
  --bind-dev "neuron" "state" "v" "color" "blend_colors( (255,255,0), (255,0,255), -70, -50, value)"

ifeq ($(NO_APP_TESTS),)


$(eval $(call self_clocked_izhikevich_instance_template,8_2_10_10000,100000,$(self_clocked_izhikevich_FILTERS)))

$(eval $(call self_clocked_izhikevich_instance_template,50_10_60_10000,100000,$(self_clocked_izhikevich_FILTERS)))
$(eval $(call self_clocked_izhikevich_instance_template,80_20_100_10000,100000,$(self_clocked_izhikevich_FILTERS)))
$(eval $(call self_clocked_izhikevich_instance_template,200_50_50_10000,100000,$(self_clocked_izhikevich_FILTERS)))

endif

self_clocked_izhikevich_tests : $(ALL_self_clocked_izhikevich_TESTS)

self_clocked_izhikevich_demos : $(ALL_self_clocked_izhikevich_DEMOS)

ALL_TESTS += self_clocked_izhikevich_tests

ALL_DEMOS += self_clocked_izhikevich_demos



