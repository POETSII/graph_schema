

$(eval $(call provider_rules_template,clock_tree))

define clock_tree_instance_template
# $1 = D_B  (depth and branching)
# $2 = max-steps
# $3 = filters

demos/clock_tree/clock_tree_$1.xml : apps/clock_tree/create_clock_tree_instance.py
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/clock_tree/create_clock_tree_instance.py  $$(subst _, ,$1) > $$@

demos/clock_tree/clock_tree_$1.snap : demos/clock_tree/clock_tree_$1.xml bin/epoch_sim clock_tree_provider demos/clock_tree/clock_tree_$1.checked
	bin/epoch_sim  --log-level 0 --max-steps $2 --snapshots 1 $$@  demos/clock_tree/clock_tree_$1.xml

ALL_CLOCK_TREE_TESTS += demos/clock_tree/clock_tree_$1.snap

demos/clock_tree/clock_tree_$1.mp4 : demos/clock_tree/clock_tree_$1.xml demos/clock_tree/clock_tree_$1.snap clock_tree_provider
	mkdir -p demos/clock_tree/clock_tree_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/clock_tree/clock_tree_$1.xml --snapshots demos/clock_tree/clock_tree_$1.snap --output demos/clock_tree/clock_tree_$1.tmp/out  \
		$3
	(cd demos/clock_tree/clock_tree_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Nlabel="" -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/clock_tree/clock_tree_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../../clock_tree_$1.gif)
	(cd demos/clock_tree/clock_tree_$1.tmp/  && $(FFMPEG) -r 10 -i out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ../clock_$1.mp4)


ALL_CLOCK_TREE_DEMOS +=  demos/clock_tree/clock_tree_$1.mp4


$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,1))
$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,4))
$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,16))
$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,32))
$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,64))
$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,128))
$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,256))
$(eval $(call softswitch_instance_template,clock_tree_$1,demos/clock_tree/clock_tree_$1.xml,512))

endef

CLOCK_TREE_FILTERS = \
	--bind-dev "branch" "state" "status" "color" "'gray' if value==0 else 'green' if value==1 else 'red'" \
	--bind-dev "leaf" "rts" "-" "color" "'red' if value>0 else 'green'" \

$(eval $(call clock_tree_instance_template,8_3,50,$(CLOCK_TREE_FILTERS)))
$(eval $(call clock_tree_instance_template,8_2,50,$(CLOCK_TREE_FILTERS)))
$(eval $(call clock_tree_instance_template,6_3,50,$(CLOCK_TREE_FILTERS)))
$(eval $(call clock_tree_instance_template,4_4,50,$(CLOCK_TREE_FILTERS)))

clock_tree_tests : $(ALL_CLOCK_TREE_TESTS)

clock_tree_demos : $(ALL_CLOCK_TREE_DEMOS)

ALL_DEMOS += clock_tree_demos

ALL_TESTS += clock_tree_tests