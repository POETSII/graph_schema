

$(eval $(call provider_rules_template,ising_spin))

define ising_spin_instance_template
# $1 = n
# $2 = T
# $3 = filters
# $4 = render demo

demos/ising_spin/ising_spin_$1.xml : apps/ising_spin/create_ising_spin_instance.py apps/ising_spin/ising_spin_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/ising_spin/create_ising_spin_instance.py  $$(subst _, ,$1) > $$@

ALL_TEST_XML += demos/ising_spin/ising_spin_$1.xml

demos/ising_spin/ising_spin_$1.snap : demos/ising_spin/ising_spin_$1.xml bin/epoch_sim providers/ising_spin.graph.so demos/ising_spin/ising_spin_$1.checked
	bin/epoch_sim --max-steps $2 --snapshots 1 $$@ --expect-idle-exit  demos/ising_spin/ising_spin_$1.xml

ALL_ISING_SPIN_TESTS += demos/ising_spin/ising_spin_$1.snap

ifneq ($4,)

demos/ising_spin/ising_spin_$1.gif : demos/ising_spin/ising_spin_$1.xml demos/ising_spin/ising_spin_$1.snap
	mkdir -p demos/ising_spin/ising_spin_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/ising_spin/ising_spin_$1.xml --snapshots demos/ising_spin/ising_spin_$1.snap --output demos/ising_spin/ising_spin_$1.tmp/out  \
		$3
	(cd demos/ising_spin/ising_spin_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/ising_spin/ising_spin_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../ising_spin_$1.gif)
	(cd demos/ising_spin/ising_spin_$1.tmp/  && $(FFMPEG) -r 10 -i out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ../ising_spin_$1.mp4)

ALL_ISING_SPIN_DEMOS += demos/ising_spin/ising_spin_$1.gif

endif

endef


ISING_SPIN_FILTERS = \
	--bind-dev "cell" "state" "spin" "color" "blend_colors( (255,255,0), (255,0,255), -1, +1, value)"

ifeq ($(NO_APP_TESTS),)


$(eval $(call ising_spin_instance_template,3_1,50,$(ISING_SPIN_FILTERS)))
$(eval $(call ising_spin_instance_template,16_2,50,$(ISING_SPIN_FILTERS),1))
$(eval $(call ising_spin_instance_template,32_1,50,$(ISING_SPIN_FILTERS)))
$(eval $(call ising_spin_instance_template,40_0.5,50,$(ISING_SPIN_FILTERS)))

# Too big to render as a graph. Render manually using render_graph_to_field
#$(eval $(call ising_spin_instance_template,256_0.5,50,$(ISING_SPIN_FILTERS)))

endif

ising_spin_tests : $(ALL_ISING_SPIN_TESTS)

ising_spin_demos : $(ALL_ISING_SPIN_DEMOS)

ALL_TESTS += ising_spin_tests

ALL_DEMOS += ising_spin_demos

