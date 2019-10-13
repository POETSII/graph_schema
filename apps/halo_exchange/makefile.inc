

$(eval $(call provider_rules_template,halo_exchange))

define halo_exchange_instance_template
# $1 = params
# $2 = delta
# $3 = maxsteps
# $4 = filters

demos/halo_exchange/halo_exchange_$1.xml : apps/halo_exchange/create_halo_exchange_instance.py apps/halo_exchange/halo_exchange_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/halo_exchange/create_halo_exchange_instance.py  $$(subst _, ,$1) > $$@

demos/halo_exchange/halo_exchange_$1.snap  : demos/halo_exchange/halo_exchange_$1.xml bin/epoch_sim providers/halo_exchange.graph.so demos/halo_exchange/halo_exchange_$1.checked
	bin/epoch_sim  --log-level 0 --max-steps $3 --snapshots $2 demos/halo_exchange/halo_exchange_$1.snap   demos/halo_exchange/halo_exchange_$1.xml

ALL_halo_exchange_TESTS += demos/halo_exchange/halo_exchange_$1.snap

demos/halo_exchange/halo_exchange_$1.mp4 : demos/halo_exchange/halo_exchange_$1.xml demos/halo_exchange/halo_exchange_$1.snap
	mkdir -p demos/halo_exchange/halo_exchange_$1.tmp
	-(cd demos/halo_exchange/halo_exchange_$1.tmp && rm *.dot)
	-(cd demos/halo_exchange/halo_exchange_$1.tmp && rm *.dot.png)
	-rm demo/halo_exchange/halo_exchange_$1.mp4
	$(PYTHON) tools/render_graph_as_dot.py demos/halo_exchange/halo_exchange_$1.xml --snapshots demos/halo_exchange/halo_exchange_$1.snap --output demos/halo_exchange/halo_exchange_$1.tmp/out  \
		$4
	(cd demos/halo_exchange/halo_exchange_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	#(cd demos/halo_exchange/halo_exchange_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../halo_exchange_$1.gif)
	(cd demos/halo_exchange/halo_exchange_$1.tmp/  && $(FFMPEG) -r 10 -i out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ../halo_exchange_$1.mp4)


ALL_halo_exchange_DEMOS += demos/halo_exchange/halo_exchange_$1.mp4


$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,1))
$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,4))
$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,16))
$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,32))
$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,64))
$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,128))
$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,256))
$(eval $(call softswitch_instance_template,halo_exchange_$1,demos/halo_exchange/halo_exchange_$1.xml,512))


endef


halo_exchange_FILTERS_TIME = \
  --bind-dev "*" "state" "timeNow" "color" "blend_colors( (255,255,0), (255,0,255), 0, 10, (value%10))"

halo_exchange_FILTERS_VALUE = \
  --bind-dev "cell" "state" "v" "color" "heat(-20000000, +20000000, value)"

ifeq ($(NO_APP_TESTS),)


$(eval $(call halo_exchange_instance_template,4,1,100,$(halo_exchange_FILTERS_VALUE)))
$(eval $(call halo_exchange_instance_template,9,1,100,$(halo_exchange_FILTERS_VALUE)))
$(eval $(call halo_exchange_instance_template,16,1,100,$(halo_exchange_FILTERS_TIME)))
$(eval $(call halo_exchange_instance_template,32,10,10000,$(halo_exchange_FILTERS_VALUE)))

# Needs to be manually rendered with graph_to_field
#$(eval $(call halo_exchange_instance_template,128,200,50000,$(halo_exchange_FILTERS_VALUE)))

endif

halo_exchange_tests : $(ALL_halo_exchange_TESTS)

halo_exchange_demos : $(ALL_halo_exchange_DEMOS)

ALL_TESTS += halo_exchange_tests

ALL_DEMOS += halo_exchange_demos



