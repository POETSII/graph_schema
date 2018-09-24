

$(eval $(call provider_rules_template,clocked_izhikevich))

define clocked_izhikevich_instance_template	
# $1 = params
# $2 = maxsteps
# $5 = filters

demos/clocked_izhikevich/clocked_izhikevich_$1.xml : apps/clocked_izhikevich/create_sparse_instance.py apps/clocked_izhikevich/clocked_izhikevich_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/clocked_izhikevich/create_sparse_instance.py  $$(subst _, ,$1) > $$@

demos/clocked_izhikevich/clocked_izhikevich_$1.snap : demos/clocked_izhikevich/clocked_izhikevich_$1.xml bin/epoch_sim providers/clocked_izhikevich.graph.so demos/clocked_izhikevich/clocked_izhikevich_$1.checked
	bin/epoch_sim --stats-delta 1000 --max-steps $2 --log-level 0 --snapshots 1000 $$@  demos/clocked_izhikevich/clocked_izhikevich_$1.xml

ALL_CLOCKED_IZHIKEVICH_TESTS += demos/clocked_izhikevich/clocked_izhikevich_$1.snap

demos/clocked_izhikevich/clocked_izhikevich_$1.gif : demos/clocked_izhikevich/clocked_izhikevich_$1.xml demos/clocked_izhikevich/clocked_izhikevich_$1.snap
	mkdir -p demos/clocked_izhikevich/clocked_izhikevich_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/clocked_izhikevich/clocked_izhikevich_$1.xml --snapshots demos/clocked_izhikevich/clocked_izhikevich_$1.snap --output demos/clocked_izhikevich/clocked_izhikevich_$1.tmp/out  \
		$3
	(cd demos/clocked_izhikevich/clocked_izhikevich_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/clocked_izhikevich/clocked_izhikevich_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../clocked_izhikevich_$1.gif)
	
ALL_CLOCKED_IZHIKEVICH_DEMOS += demos/clocked_izhikevich/clocked_izhikevich_$1.gif
	
endef


CLOCKED_IZHIKEVICH_FILTERS = \
  --bind-dev "neuron" "state" "v" "color" "blend_colors( (255,255,0), (255,0,255), -70, -50, value)"

#   --bind-edge "pulse" "firings" "relative" "color" "blend_colors( (255,0,0,0), (255,0,0,255), 0, 1, value)" 

$(eval $(call clocked_izhikevich_instance_template,8_2_10,100000,$(CLOCKED_IZHIKEVICH_FILTERS)))

$(eval $(call clocked_izhikevich_instance_template,30_10_6,100000,$(CLOCKED_IZHIKEVICH_FILTERS)))

$(eval $(call clocked_izhikevich_instance_template,50_10_60,10000,$(CLOCKED_IZHIKEVICH_FILTERS)))
$(eval $(call clocked_izhikevich_instance_template,80_20_100,10000,$(CLOCKED_IZHIKEVICH_FILTERS)))
$(eval $(call clocked_izhikevich_instance_template,200_50_50,10000,$(CLOCKED_IZHIKEVICH_FILTERS)))


clocked_izhikevich_tests : $(ALL_CLOCKED_IZHIKEVICH_TESTS)

clocked_izhikevich_demos : $(ALL_CLOCKED_IZHIKEVICH_DEMOS)

ALL_TESTS += clocked_izhikevich_tests

ALL_DEMOS += clocked_izhikevich_demos



