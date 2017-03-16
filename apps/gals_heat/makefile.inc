

$(eval $(call provider_rules_template,gals_heat))

define gals_heat_instance_template
# $1 = params
# $2 = delta
# $3 = maxsteps
# $4 = filters

demos/gals_heat/gals_heat_$1.xml : apps/gals_heat/create_gals_heat_instance.py apps/gals_heat/gals_heat_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/gals_heat/create_gals_heat_instance.py  $$(subst _, ,$1) > $$@

demos/gals_heat/gals_heat_$1.snap : demos/gals_heat/gals_heat_$1.xml bin/epoch_sim providers/gals_heat.graph.so demos/gals_heat/gals_heat_$1.checked
	bin/epoch_sim --max-steps $3 --snapshots $2 $$@  demos/gals_heat/gals_heat_$1.xml

ALL_GALS_HEAT_TESTS += demos/gals_heat/gals_heat_$1.snap

demos/gals_heat/gals_heat_$1.mp4 : demos/gals_heat/gals_heat_$1.xml demos/gals_heat/gals_heat_$1.snap
	mkdir -p demos/gals_heat/gals_heat_$1.tmp
	-(cd demos/gals_heat/gals_heat_$1.tmp && rm *.dot)
	-(cd demos/gals_heat/gals_heat_$1.tmp && rm *.dot.png)
	-rm demo/gals_heat/gals_heat_$1.mp4
	$(PYTHON) tools/render_graph_as_dot.py demos/gals_heat/gals_heat_$1.xml --snapshots demos/gals_heat/gals_heat_$1.snap --output demos/gals_heat/gals_heat_$1.tmp/out  \
		$4
	(cd demos/gals_heat/gals_heat_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	#(cd demos/gals_heat/gals_heat_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../gals_heat_$1.gif)
	(cd demos/gals_heat/gals_heat_$1.tmp/  && $(FFMPEG) -r 10 -i out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ../gals_heat_$1.mp4)


ALL_GALS_HEAT_DEMOS += demos/gals_heat/gals_heat_$1.mp4

endef


GALS_HEAT_FILTERS_TIME = \
  --bind-dev "*" "state" "timeNow" "color" "blend_colors( (255,255,0), (255,0,255), 0, 10, (value%10))"

GALS_HEAT_FILTERS_VALUE = \
  --bind-dev "*" "state" "value" "color" "heat(-1, +1, value)"


$(eval $(call gals_heat_instance_template,9,1,100,$(GALS_HEAT_FILTERS_VALUE)))
$(eval $(call gals_heat_instance_template,16,1,100,$(GALS_HEAT_FILTERS_TIME)))
$(eval $(call gals_heat_instance_template,32,10,10000,$(GALS_HEAT_FILTERS_VALUE)))

# Needs to be manually rendered with graph_to_field
#$(eval $(call gals_heat_instance_template,128,200,50000,$(GALS_HEAT_FILTERS_VALUE)))



gals_heat_tests : $(ALL_GALS_HEAT_TESTS)

gals_heat_demos : $(ALL_GALS_HEAT_DEMOS)

ALL_TESTS += gals_heat_tests

ALL_DEMOS += gals_heat_demos



