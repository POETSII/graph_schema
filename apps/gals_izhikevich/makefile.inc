

$(eval $(call provider_rules_template,gals_izhikevich))

define gals_izhikevich_instance_template	
# $1 = params
# $2 = snapshotdelta
# $3 = maxsteps
# $4 = filters

demos/gals_izhikevich/gals_izhikevich_$1.xml : apps/gals_izhikevich/create_sparse_instance.py apps/gals_izhikevich/gals_izhikevich_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/gals_izhikevich/create_sparse_instance.py apps/gals_izhikevich/gals_izhikevich_graph_type.xml $$(subst _, ,$1) > $$@

demos/gals_izhikevich/gals_izhikevich_$1.snap : demos/gals_izhikevich/gals_izhikevich_$1.xml bin/epoch_sim providers/gals_izhikevich.graph.so demos/gals_izhikevich/gals_izhikevich_$1.checked
	bin/epoch_sim --max-steps $3 --snapshots $2 $$@  demos/gals_izhikevich/gals_izhikevich_$1.xml

ALL_GALS_IZHIKEVICH_TESTS += demos/gals_izhikevich/gals_izhikevich_$1.snap

demos/gals_izhikevich/gals_izhikevich_$1.gif : demos/gals_izhikevich/gals_izhikevich_$1.xml demos/gals_izhikevich/gals_izhikevich_$1.snap
	mkdir -p demos/gals_izhikevich/gals_izhikevich_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/gals_izhikevich/gals_izhikevich_$1.xml --snapshots demos/gals_izhikevich/gals_izhikevich_$1.snap --output demos/gals_izhikevich/gals_izhikevich_$1.tmp/out  \
		$4
	(cd demos/gals_izhikevich/gals_izhikevich_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/gals_izhikevich/gals_izhikevich_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../gals_izhikevich_$1.gif)
	
ALL_GALS_IZHIKEVICH_DEMOS += demos/gals_izhikevich/gals_izhikevich_$1.gif
	
endef


GALS_IZHIKEVICH_FILTERS = \
  --bind-dev "neuron" "state" "v" "color" "blend_colors( (255,255,0), (255,0,255), -70, -50, value)" \
   --bind-edge "synapse" "firings" "relative" "color" "blend_colors( (255,0,0,10), (255,0,0,150), 0, 1, value)"

GALS_IZHIKEVICH_FILTERS_I = \
  --bind-dev "neuron" "state" "Icount" "color" "blend_colors( (255,255,0), (255,0,255), 0, 20, value)" \
   --bind-edge "synapse" "firings" "relative" "color" "blend_colors( (255,0,0,10), (255,0,0,150), 0, 1, value)" 



$(eval $(call gals_izhikevich_instance_template,8_2_10,10000,$(GALS_IZHIKEVICH_FILTERS)))

$(eval $(call gals_izhikevich_instance_template,50_10_20,1,100,$(GALS_IZHIKEVICH_FILTERS_I)))
$(eval $(call gals_izhikevich_instance_template,50_10_60,100,10000,$(GALS_IZHIKEVICH_FILTERS)))


$(eval $(call gals_izhikevich_instance_template,80_20_100,1000,10000,$(GALS_IZHIKEVICH_FILTERS)))
$(eval $(call gals_izhikevich_instance_template,200_50_50,1000,10000,$(GALS_IZHIKEVICH_FILTERS)))


gals_izhikevich_tests : $(ALL_GALS_IZHIKEVICH_TESTS)

gals_izhikevich_demos : $(ALL_GALS_IZHIKEVICH_DEMOS)

ALL_TESTS += gals_izhikevich_tests

ALL_DEMOS += gals_izhikevich_demos



