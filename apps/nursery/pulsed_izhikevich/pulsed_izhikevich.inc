

$(eval $(call provider_rules_template,pulsed_izhikevich,apps/nursery/pulsed_izhikevich/pulsed_izhikevich_graph_type.xml,no-default))

define pulsed_izhikevich_instance_template	
# $1 = params
# $2 = maxsteps
# $5 = filters

demos/pulsed_izhikevich/pulsed_izhikevich_$1.xml : apps/nursery/pulsed_izhikevich/create_sparse_instance.py apps/nursery/pulsed_izhikevich/pulsed_izhikevich_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/nursery/pulsed_izhikevich/create_sparse_instance.py  $$(subst _, ,$1) > $$@

ALL_TEST_XML += demos/pulsed_izhikevich/pulsed_izhikevich_$1.xml

demos/pulsed_izhikevich/pulsed_izhikevich_$1.snap : demos/pulsed_izhikevich/pulsed_izhikevich_$1.xml bin/epoch_sim providers/pulsed_izhikevich.graph.so demos/pulsed_izhikevich/pulsed_izhikevich_$1.checked
	bin/epoch_sim --stats-delta 1000 --max-steps $2 --log-level 0 --snapshots 1000 $$@  demos/pulsed_izhikevich/pulsed_izhikevich_$1.xml

ALL_pulsed_izhikevich_TESTS += demos/pulsed_izhikevich/pulsed_izhikevich_$1.snap

demos/pulsed_izhikevich/pulsed_izhikevich_$1.gif : demos/pulsed_izhikevich/pulsed_izhikevich_$1.xml demos/pulsed_izhikevich/pulsed_izhikevich_$1.snap
	mkdir -p demos/pulsed_izhikevich/pulsed_izhikevich_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/pulsed_izhikevich/pulsed_izhikevich_$1.xml --snapshots demos/pulsed_izhikevich/pulsed_izhikevich_$1.snap --output demos/pulsed_izhikevich/pulsed_izhikevich_$1.tmp/out  \
		$3
	(cd demos/pulsed_izhikevich/pulsed_izhikevich_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/pulsed_izhikevich/pulsed_izhikevich_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../pulsed_izhikevich_$1.gif)
	
ALL_pulsed_izhikevich_DEMOS += demos/pulsed_izhikevich/pulsed_izhikevich_$1.gif
	
endef


pulsed_izhikevich_FILTERS = \
  --bind-dev "neuron" "state" "v" "color" "blend_colors( (255,255,0), (255,0,255), -70, -50, value)"

ifeq ($(NO_APP_TESTS),)


$(eval $(call pulsed_izhikevich_instance_template,8_2_10_10000,100000,$(pulsed_izhikevich_FILTERS)))

$(eval $(call pulsed_izhikevich_instance_template,50_10_60_10000,100000,$(pulsed_izhikevich_FILTERS)))
$(eval $(call pulsed_izhikevich_instance_template,80_20_100_10000,100000,$(pulsed_izhikevich_FILTERS)))
$(eval $(call pulsed_izhikevich_instance_template,200_50_50_10000,100000,$(pulsed_izhikevich_FILTERS)))

endif

pulsed_izhikevich_tests : $(ALL_pulsed_izhikevich_TESTS)

pulsed_izhikevich_demos : $(ALL_pulsed_izhikevich_DEMOS)

ALL_TESTS += pulsed_izhikevich_tests

ALL_DEMOS += pulsed_izhikevich_demos



