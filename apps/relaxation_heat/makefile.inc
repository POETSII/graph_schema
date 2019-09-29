

$(eval $(call provider_rules_template,relaxation_heat,apps/relaxation_heat/relaxation_heat_graph_type.xml))
$(eval $(call provider_rules_template,relaxation_heat_noterm,apps/relaxation_heat/relaxation_heat_noterm_graph_type.xml))


define relaxation_heat_instance_template
# $1 = params
# $2 = delta
# $3 = maxsteps
# $4 = filters
# $5 = use_field (should be "" or "yes")
# $6 = demo_only (should be "" or "yes")

demos/relaxation_heat/relaxation_heat_$1.xml : apps/relaxation_heat/create_relaxation_heat_instance.py apps/relaxation_heat/relaxation_heat_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/relaxation_heat/create_relaxation_heat_instance.py  $$(subst _, ,$1) > $$@

ALL_TEST_XML += demos/relaxation_heat/relaxation_heat_$1.xml

demos/relaxation_heat/relaxation_heat_$1.snap : demos/relaxation_heat/relaxation_heat_$1.xml bin/epoch_sim providers/relaxation_heat.graph.so demos/relaxation_heat/relaxation_heat_$1.checked
	bin/epoch_sim  --log-level 0 --max-steps $3 --snapshots $2 $$@ --expect-idle-exit  demos/relaxation_heat/relaxation_heat_$1.xml

ifeq (,$6)
ALL_relaxation_heat_TESTS += demos/relaxation_heat/relaxation_heat_$1.snap
endif

ifeq (,$5)

demos/relaxation_heat/relaxation_heat_$1.mp4 : demos/relaxation_heat/relaxation_heat_$1.xml demos/relaxation_heat/relaxation_heat_$1.snap
	mkdir -p demos/relaxation_heat/relaxation_heat_$1.tmp
	-(cd demos/relaxation_heat/relaxation_heat_$1.tmp && rm *.dot)
	-(cd demos/relaxation_heat/relaxation_heat_$1.tmp && rm *.dot.png)
	-rm demo/relaxation_heat/relaxation_heat_$1.mp4
	$(PYTHON) tools/render_graph_as_dot.py demos/relaxation_heat/relaxation_heat_$1.xml --snapshots demos/relaxation_heat/relaxation_heat_$1.snap --output demos/relaxation_heat/relaxation_heat_$1.tmp/out  \
		$4
	(cd demos/relaxation_heat/relaxation_heat_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	#(cd demos/relaxation_heat/relaxation_heat_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../relaxation_heat_$1.gif)
	(cd demos/relaxation_heat/relaxation_heat_$1.tmp/  && $(FFMPEG) -y -r 10 -i out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ../relaxation_heat_$1.mp4)

else

demos/relaxation_heat/relaxation_heat_$1.mp4 : demos/relaxation_heat/relaxation_heat_$1.xml demos/relaxation_heat/relaxation_heat_$1.snap
	apps/relaxation_heat/render_heat_as_video_field.sh demos/relaxation_heat/relaxation_heat_$1.xml demos/relaxation_heat/relaxation_heat_$1.snap
	mv  demos/relaxation_heat/relaxation_heat_$1.xml.field.mp4 demos/relaxation_heat/relaxation_heat_$1.mp4

endif


ALL_relaxation_heat_DEMOS += demos/relaxation_heat/relaxation_heat_$1.mp4

endef


relaxation_heat_FILTERS_TIME = \
  --device-type-filter cell --bind-dev "cell" "state" "currVersion" "color" "blend_colors( (255,255,0), (255,0,255), 0, 10, (value%10))"

relaxation_heat_FILTERS_VALUE = \
  --device-type-filter cell --bind-dev "cell" "state" "currHeat" "color" "heat(-127, +127, value)"

ifeq ($(NO_APP_TESTS),)


$(eval $(call relaxation_heat_instance_template,9,1,1000,$(relaxation_heat_FILTERS_VALUE)))
$(eval $(call relaxation_heat_instance_template,16,1,1000,$(relaxation_heat_FILTERS_TIME)))

#$(eval $(call relaxation_heat_instance_template,32,10,50000,$(relaxation_heat_FILTERS_VALUE),yes,yes))

#$(eval $(call relaxation_heat_instance_template,128,10,50000,$(relaxation_heat_FILTERS_VALUE),yes,yes))

endif


relaxation_heat_tests : $(ALL_relaxation_heat_TESTS)

relaxation_heat_demos : $(ALL_relaxation_heat_DEMOS)

ALL_TESTS += relaxation_heat_tests

ALL_DEMOS += relaxation_heat_demos



