

$(eval $(call provider_rules_template,relaxation_heat,apps/nursery/relaxation_heat/relaxation_heat_graph_type.xml))
$(eval $(call provider_rules_template,relaxation_heat_noterm,apps/nursery/relaxation_heat/relaxation_heat_noterm_graph_type.xml))


define relaxation_heat_instance_template
# $1 = params
# $2 = delta
# $3 = maxsteps
# $4 = filters

demos/relaxation_heat/relaxation_heat_$1.xml : apps/nursery/relaxation_heat/create_relaxation_heat_instance.py apps/nursery/relaxation_heat/relaxation_heat_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/nursery/relaxation_heat/create_relaxation_heat_instance.py  $$(subst _, ,$1) > $$@

demos/relaxation_heat/relaxation_heat_$1.snap : demos/relaxation_heat/relaxation_heat_$1.xml bin/epoch_sim providers/relaxation_heat.graph.so demos/relaxation_heat/relaxation_heat_$1.checked
	bin/epoch_sim  --log-level 0 --max-steps $3 --snapshots $2 $$@  demos/relaxation_heat/relaxation_heat_$1.xml

ALL_relaxation_heat_TESTS += demos/relaxation_heat/relaxation_heat_$1.snap

demos/relaxation_heat/relaxation_heat_$1.mp4 : demos/relaxation_heat/relaxation_heat_$1.xml demos/relaxation_heat/relaxation_heat_$1.snap
	mkdir -p demos/relaxation_heat/relaxation_heat_$1.tmp
	-(cd demos/relaxation_heat/relaxation_heat_$1.tmp && rm *.dot)
	-(cd demos/relaxation_heat/relaxation_heat_$1.tmp && rm *.dot.png)
	-rm demo/relaxation_heat/relaxation_heat_$1.mp4
	$(PYTHON) tools/render_graph_as_dot.py demos/relaxation_heat/relaxation_heat_$1.xml --snapshots demos/relaxation_heat/relaxation_heat_$1.snap --output demos/relaxation_heat/relaxation_heat_$1.tmp/out  \
		$4
	(cd demos/relaxation_heat/relaxation_heat_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	#(cd demos/relaxation_heat/relaxation_heat_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../relaxation_heat_$1.gif)
	(cd demos/relaxation_heat/relaxation_heat_$1.tmp/  && $(FFMPEG) -r 10 -i out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ../relaxation_heat_$1.mp4)


ALL_relaxation_heat_DEMOS += demos/relaxation_heat/relaxation_heat_$1.mp4

endef


relaxation_heat_FILTERS_TIME = \
  --bind-dev "*" "state" "timeNow" "color" "blend_colors( (255,255,0), (255,0,255), 0, 10, (value%10))"

relaxation_heat_FILTERS_VALUE = \
  --bind-dev "*" "state" "value" "color" "heat(-1, +1, value)"


$(eval $(call relaxation_heat_instance_template,9,1,100,$(relaxation_heat_FILTERS_VALUE)))
$(eval $(call relaxation_heat_instance_template,16,1,100,$(relaxation_heat_FILTERS_TIME)))
$(eval $(call relaxation_heat_instance_template,32,10,10000,$(relaxation_heat_FILTERS_VALUE)))

# Needs to be manually rendered with graph_to_field
#$(eval $(call relaxation_heat_instance_template,128,200,50000,$(relaxation_heat_FILTERS_VALUE)))



relaxation_heat_tests : $(ALL_relaxation_heat_TESTS)

relaxation_heat_demos : $(ALL_relaxation_heat_DEMOS)

ALL_TESTS += relaxation_heat_tests

ALL_DEMOS += relaxation_heat_demos



