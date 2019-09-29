

$(eval $(call provider_rules_template,ising_spin_fix))

define ising_spin_fix_instance_template
# $1 = n
# $2 = T
# $3 = filters
# $4 render demo

demos/ising_spin_fix/ising_spin_fix_$1.xml : apps/ising_spin_fix/create_ising_spin_fix_instance.py apps/ising_spin_fix/ising_spin_fix_graph_type.xml
	mkdir -p $$(dir $$@)
	$$(PYTHON) apps/ising_spin_fix/create_ising_spin_fix_instance.py  $$(subst _, ,$1) > $$@

ALL_TEST_XML += demos/ising_spin_fix/ising_spin_fix_$1.xml

demos/ising_spin_fix/ising_spin_fix_$1.snap : demos/ising_spin_fix/ising_spin_fix_$1.xml bin/epoch_sim providers/ising_spin_fix.graph.so demos/ising_spin_fix/ising_spin_fix_$1.checked
	bin/epoch_sim --max-steps $2 --snapshots 1 $$@  --expect-idle-exit demos/ising_spin_fix/ising_spin_fix_$1.xml

ALL_ising_spin_fix_TESTS += demos/ising_spin_fix/ising_spin_fix_$1.snap

ifneq ($4,)

demos/ising_spin_fix/ising_spin_fix_$1.gif : demos/ising_spin_fix/ising_spin_fix_$1.xml demos/ising_spin_fix/ising_spin_fix_$1.snap
	mkdir -p demos/ising_spin_fix/ising_spin_fix_$1.tmp
	$(PYTHON) tools/render_graph_as_dot.py demos/ising_spin_fix/ising_spin_fix_$1.xml \
			--snapshots demos/ising_spin_fix/ising_spin_fix_$1.snap \
			--output demos/ising_spin_fix/ising_spin_fix_$1.tmp/out  \
			--device-type-filter 'cell'
			$3
	(cd demos/ising_spin_fix/ising_spin_fix_$1.tmp/  &&  for i in *.dot; do echo $$$$i; neato -Gsize=5,5\! -Gdpi=100 -Tpng $$$$i > $$$$i.png; done )
	(cd demos/ising_spin_fix/ising_spin_fix_$1.tmp/  && convert -delay 20 -loop 0 *.dot.png ../ising_spin_fix_$1.gif)
	$(FFMPEG) -y -r 10 -i demos/ising_spin_fix/ising_spin_fix_$1.tmp/out_%06d.dot.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 demos/ising_spin_fix/ising_spin_fix_$1.mp4)

ALL_ising_spin_fix_DEMOS += demos/ising_spin_fix/ising_spin_fix_$1.gif

endif
			
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,1))
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,4))
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,16))
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,32))
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,64))
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,128))
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,256))
$(eval $(call softswitch_instance_template,ising_spin_fix_$1,demos/ising_spin_fix/ising_spin_fix_$1.xml,512))
			

endef


ising_spin_fix_FILTERS = \
	--bind-dev "cell" "state" "spin" "color" "blend_colors( (255,255,0), (255,0,255), -1, +1, value)"

ifeq ($(NO_APP_TESTS),)


$(eval $(call ising_spin_fix_instance_template,3_1,50,$(ising_spin_fix_FILTERS)))
$(eval $(call ising_spin_fix_instance_template,16_2,50,$(ising_spin_fix_FILTERS)))
$(eval $(call ising_spin_fix_instance_template,32_1,50,$(ising_spin_fix_FILTERS)))
$(eval $(call ising_spin_fix_instance_template,40_0.5,50,$(ising_spin_fix_FILTERS)))

# Too big to render as a graph. Render manually using render_graph_to_field
#$(eval $(call ising_spin_fix_instance_template,256_0.5,50,$(ising_spin_fix_FILTERS)))

endif

ising_spin_fix_tests : $(ALL_ising_spin_fix_TESTS)

ising_spin_fix_demos : $(ALL_ising_spin_fix_DEMOS)

ALL_TESTS += ising_spin_fix_tests

ALL_DEMOS += ising_spin_fix_demos

