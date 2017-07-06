

$(eval $(call provider_rules_template,clocked_izhikevich_fix))

define clocked_izhikevich_fix_instance_template	
# $1 = params
# $2 = maxsteps
# $5 = filters

demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml : apps/clocked_izhikevich_fix/create_clocked_izhikevich_fix_instance.py apps/clocked_izhikevich_fix/clocked_izhikevich_fix_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/clocked_izhikevich_fix/create_clocked_izhikevich_fix_instance.py  $$(subst _, ,$1) > $$@

demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.snap : demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml bin/epoch_sim providers/clocked_izhikevich_fix.graph.so demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.checked
	bin/epoch_sim --stats-delta 1000 --max-steps $2 --log-level 0 --snapshots 1000 $$@ --key-value  demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.kv  demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml

ALL_clocked_izhikevich_fix_TESTS += demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.snap

demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.gif : demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.snap
	mkdir -p demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml --snapshots demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.snap --output demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.tmp/out  \
		$3
	(cd demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../clocked_izhikevich_fix_$1.gif)

$(eval $(call softswitch_instance_template,clocked_izhikevich_fix_$1,demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml,1))
$(eval $(call softswitch_instance_template,clocked_izhikevich_fix_$1,demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml,4))
$(eval $(call softswitch_instance_template,clocked_izhikevich_fix_$1,demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml,16))
$(eval $(call softswitch_instance_template,clocked_izhikevich_fix_$1,demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml,64))
$(eval $(call softswitch_instance_template,clocked_izhikevich_fix_$1,demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml,256))
$(eval $(call softswitch_instance_template,clocked_izhikevich_fix_$1,demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.xml,1024))
	
ALL_clocked_izhikevich_fix_DEMOS += demos/clocked_izhikevich_fix/clocked_izhikevich_fix_$1.gif
	
endef


clocked_izhikevich_fix_FILTERS = \
  --bind-dev "neuron" "state" "v" "color" "blend_colors( (255,255,0), (255,0,255), -3932160, -3276800, value)"

#   --bind-edge "pulse" "firings" "relative" "color" "blend_colors( (255,0,0,0), (255,0,0,255), 0, 65536, value)" 

$(eval $(call clocked_izhikevich_fix_instance_template,8_2_10,100000,$(clocked_izhikevich_fix_FILTERS)))

$(eval $(call clocked_izhikevich_fix_instance_template,50_10_60_100,10000,$(clocked_izhikevich_fix_FILTERS)))
$(eval $(call clocked_izhikevich_fix_instance_template,80_20_100_100,10000,$(clocked_izhikevich_fix_FILTERS)))
$(eval $(call clocked_izhikevich_fix_instance_template,200_50_50_100,10000,$(clocked_izhikevich_fix_FILTERS)))


clocked_izhikevich_fix_tests : $(ALL_clocked_izhikevich_fix_TESTS)

clocked_izhikevich_fix_demos : $(ALL_clocked_izhikevich_fix_DEMOS)

ALL_TESTS += clocked_izhikevich_fix_tests

ALL_DEMOS += clocked_izhikevich_fix_demos



