

$(eval $(call provider_rules_template,firefly_sync))

define firefly_sync_instance_template
# $1 = params
# $2 = delta
# $3 = maxsteps
# $4 = filters

demos/firefly_sync/firefly_sync_$1.xml : apps/firefly_sync/create_firefly_sync_instance.py apps/firefly_sync/firefly_sync_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/firefly_sync/create_firefly_sync_instance.py  $$(subst _, ,$1) > $$@

demos/firefly_sync/firefly_sync_$1.snap demos/firefly_sync/firefly_sync_$1.kv : demos/firefly_sync/firefly_sync_$1.xml bin/epoch_sim providers/firefly_sync.graph.so demos/firefly_sync/firefly_sync_$1.checked
	bin/epoch_sim  --log-level 0 --max-steps $3 --snapshots $2 demos/firefly_sync/firefly_sync_$1.snap --key-value demos/firefly_sync/firefly_sync_$1.kv  demos/firefly_sync/firefly_sync_$1.xml

ALL_firefly_sync_TESTS += demos/firefly_sync/firefly_sync_$1.snap demos/firefly_sync/firefly_sync_$1.kv

demos/firefly_sync/firefly_sync_$1.mp4 : demos/firefly_sync/firefly_sync_$1.xml demos/firefly_sync/firefly_sync_$1.snap
	mkdir -p demos/firefly_sync/firefly_sync_$1.tmp
	-(cd demos/firefly_sync/firefly_sync_$1.tmp && rm *.dot)
	-(cd demos/firefly_sync/firefly_sync_$1.tmp && rm *.dot.png)
	-rm demo/firefly_sync/firefly_sync_$1.mp4
	$(PYTHON) tools/render_graph_as_dot.py demos/firefly_sync/firefly_sync_$1.xml --snapshots demos/firefly_sync/firefly_sync_$1.snap --output demos/firefly_sync/firefly_sync_$1.tmp/out  \
		$4
	(cd demos/firefly_sync/firefly_sync_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	#(cd demos/firefly_sync/firefly_sync_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../firefly_sync_$1.gif)
	(cd demos/firefly_sync/firefly_sync_$1.tmp/  && $(FFMPEG) -r 10 -i out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ../firefly_sync_$1.mp4)


ALL_firefly_sync_DEMOS += demos/firefly_sync/firefly_sync_$1.mp4


$(eval $(call softswitch_instance_template,firefly_sync_$1,demos/firefly_sync/firefly_sync_$1.xml,1))
$(eval $(call softswitch_instance_template,firefly_sync_$1,demos/firefly_sync/firefly_sync_$1.xml,4))
$(eval $(call softswitch_instance_template,firefly_sync_$1,demos/firefly_sync/firefly_sync_$1.xml,16))



endef


firefly_sync_FILTERS_TIME = \
  --bind-dev "*" "state" "timeNow" "color" "blend_colors( (255,255,0), (255,0,255), 0, 10, (value%10))"

firefly_sync_FILTERS_VALUE = \
  --bind-dev "cell" "state" "v" "color" "heat(-20000000, +20000000, value)"


$(eval $(call firefly_sync_instance_template,4,1,100,$(firefly_sync_FILTERS_VALUE)))
$(eval $(call firefly_sync_instance_template,9,1,100,$(firefly_sync_FILTERS_VALUE)))
$(eval $(call firefly_sync_instance_template,16,1,100,$(firefly_sync_FILTERS_TIME)))
$(eval $(call firefly_sync_instance_template,32,10,10000,$(firefly_sync_FILTERS_VALUE)))

# Needs to be manually rendered with graph_to_field
#$(eval $(call firefly_sync_instance_template,128,200,50000,$(firefly_sync_FILTERS_VALUE)))


firefly_sync_tests : $(ALL_firefly_sync_TESTS)

firefly_sync_demos : $(ALL_firefly_sync_DEMOS)

ALL_TESTS += firefly_sync_tests

ALL_DEMOS += firefly_sync_demos



