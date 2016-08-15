

$(eval $(call provider_rules_template,gals_heat))

define gals_heat_instance_template	
# $1 = params
# $2 = maxsteps
# $5 = filters

demos/gals_heat/gals_heat_$1.xml : apps/gals_heat/create_gals_heat_instance.py apps/gals_heat/gals_heat_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/gals_heat/create_gals_heat_instance.py  $$(subst _, ,$1) > $$@

demos/gals_heat/gals_heat_$1.snap : demos/gals_heat/gals_heat_$1.xml bin/epoch_sim providers/gals_heat.graph.so
	bin/epoch_sim --max-steps $2 --snapshots 1 $$@  demos/gals_heat/gals_heat_$1.xml

ALL_GALS_HEAT_TESTS += demos/gals_heat/gals_heat_$1.snap

demos/gals_heat/gals_heat_$1.gif : demos/gals_heat/gals_heat_$1.xml demos/gals_heat/gals_heat_$1.snap
	mkdir -p demos/gals_heat/gals_heat_$1.tmp
	-(cd demos/gals_heat/gals_heat_$1.tmp && rm *.dot)
	-(cd demos/gals_heat/gals_heat_$1.tmp && rm *.dot.png)
	$(PYTHON) tools/render_graph_as_dot.py demos/gals_heat/gals_heat_$1.xml --snapshots demos/gals_heat/gals_heat_$1.snap --output demos/gals_heat/gals_heat_$1.tmp/out  \
		$3
	(cd demos/gals_heat/gals_heat_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -n -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/gals_heat/gals_heat_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../gals_heat_$1.gif)
	
ALL_GALS_HEAT_DEMOS += demos/gals_heat/gals_heat_$1.gif
	
endef


GALS_HEAT_FILTERS_TIME = \
  --bind-dev "*" "state" "timeNow" "color" "blend_color( (255,255,0), (255,0,255), 0, 10, (value%10))"

GALS_HEAT_FILTERS_VALUE = \
  --bind-dev "*" "state" "value" "color" "heat(-1, +1, value)"


$(eval $(call gals_heat_instance_template,9,100,$(GALS_HEAT_FILTERS_VALUE)))
$(eval $(call gals_heat_instance_template,32,1000,$(GALS_HEAT_FILTERS_VALUE)))



gals_heat_tests : $(ALL_GALS_HEAT_TESTS)

gals_heat_demos : $(ALL_GALS_HEAT_DEMOS)

ALL_TESTS += gals_heat_tests

ALL_DEMOS += gals_heat_demos



