

$(eval $(call provider_rules_template,storm))



define storm_instance_template
# $1 = D_B  (depth and branching)
# $2 = filters

demos/storm/storm_$1.xml : apps/storm/create_storm_instance.py
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/storm/create_storm_instance.py  $$(subst _, ,$1) > $$@

demos/storm/storm_$1.snap : demos/storm/storm_$1.xml bin/epoch_sim providers/storm.graph.so demos/storm/storm_$1.checked
	bin/epoch_sim  --log-level 0 --snapshots 10 $$@  demos/storm/storm_$1.xml

ALL_storm_TESTS += demos/storm/storm_$1.snap

demos/storm/storm_$1.mp4 : demos/storm/storm_$1.xml demos/storm/storm_$1.snap storm_provider
	mkdir -p demos/storm/storm_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/storm/storm_$1.xml --snapshots demos/storm/storm_$1.snap --output demos/storm/storm_$1.tmp/out  \
		$2
	(cd demos/storm/storm_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Nlabel="" -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	#(cd demos/storm/storm_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../../storm_$1.gif)
	$(FFMPEG) -r 10 -i demos/storm/storm_$1.tmp/out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 demos/storm/storm_$1.mp4


ALL_storm_DEMOS +=  demos/storm/storm_$1.mp4


$(eval $(call softswitch_instance_template,storm_$1,demos/storm/storm_$1.xml,1))
$(eval $(call softswitch_instance_template,storm_$1,demos/storm/storm_$1.xml,4))

endef

#storm_FILTERS = \
#	--bind-dev "node" "state" "credit" "color" "'gray' if value==0 else 'red' if value==1 else 'orange' if value < 10 else 'yellow' if value<100 else 'green'" \

storm_FILTERS = \
	--bind-dev "node" "state" "credit" "color" "'gray' if value==0 else blend_colors([0,0,0],[255,0,0],0,math.log(0x10000000),math.log(value))" \


$(eval $(call storm_instance_template,16_4_8,$(storm_FILTERS)))
$(eval $(call storm_instance_template,256_8_256,$(storm_FILTERS)))

storm_tests : $(ALL_storm_TESTS)

storm_demos : $(ALL_storm_DEMOS)

ALL_DEMOS += storm_demos

ALL_TESTS += storm_tests
