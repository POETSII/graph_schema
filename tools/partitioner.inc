
demos/partitioner/gals32.xml : apps/gals_heat/create_gals_heat_instance.py
	mkdir -p $(dir $@)
	python3.4 $< 32 > $@

demos/partitioner/gals32.gif : bin/partitioner providers/gals_heat.graph.so demos/partitioner/gals32.xml
	X=$$(seq -w 0 99); \
	for i in $$X; do \
		bin/partitioner demos/partitioner/gals32.xml --prop-steps 0.$$i > demos/partitioner/gals32_$$i.dot ; \
		neato -Epenwidth="5" -Nlabel="" -Gsize=5,5\! -Gdpi=100 demos/partitioner/gals32_$$i.dot -Tpng > demos/partitioner/gals32_$$i.png; \
	done
	# Make width+height multiple of 2 : http://stackoverflow.com/a/20848224/4141520
	(cd demos/partitioner && $(FFMPEG) -r 10 -start_number 1 -i gals32_%02d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 gals32.mp4)

ALL_DEMOS += demos/partitioner/gals32.gif

demos/partitioner/gals64.xml : apps/gals_heat/create_gals_heat_instance.py
	mkdir -p $(dir $@)
	python3.4 $< 64 > $@

demos/partitioner/gals64.gif : bin/partitioner providers/gals_heat.graph.so demos/partitioner/gals64.xml
	X=$$(seq -w 0 99); \
	for i in $$X; do \
		bin/partitioner demos/partitioner/gals64.xml --prop-steps 0.$$i > demos/partitioner/gals64_$$i.dot ; \
		neato -Epenwidth="5" -Nlabel="" -Gsize=8,8\! -Gdpi=100 demos/partitioner/gals64_$$i.dot -Tpng > demos/partitioner/gals64_$$i.png; \
	done
	# Make width+height multiple of 2 : http://stackoverflow.com/a/20848224/4141520
	(cd demos/partitioner && $(FFMPEG) -r 10 -start_number 1 -i gals64_%02d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 gals64.mp4)

demos/partitioner/gals64_p4.gif : bin/partitioner providers/gals_heat.graph.so demos/partitioner/gals64.xml
	X=$$(seq -w 0 99); \
	for i in $$X; do \
		bin/partitioner demos/partitioner/gals64.xml --steps 10000000 --partitions 4 --prop-steps 0.$$i > demos/partitioner/gals64_p4_$$i.dot ; \
		neato -Epenwidth="5" -Nlabel="" -Gsize=8,8\! -Gdpi=100 demos/partitioner/gals64_p4_$$i.dot -Tpng > demos/partitioner/gals64_p4_$$i.png; \
	done
	# Make width+height multiple of 2 : http://stackoverflow.com/a/20848224/4141520
	(cd demos/partitioner && $(FFMPEG) -r 10 -start_number 1 -i gals64_p4_%02d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 gals64_p4.mp4)


demos/partitioner/ising32.xml : apps/ising_spin/create_ising_spin_instance.py
	mkdir -p $(dir $@)
	python3.4 $< apps/ising_spin/ising_spin_graph_type.xml 32 > $@

demos/partitioner/ising32_p4.gif : bin/partitioner providers/ising_spin.graph.so demos/partitioner/ising32.xml
	X=$$(seq -w 0 99); \
	for i in $$X; do \
		bin/partitioner demos/partitioner/ising32.xml --steps 10000000 --partitions 4 --prop-steps 0.$$i > demos/partitioner/ising32_p4_$$i.dot ; \
		neato -n1 -Epenwidth="5" -Nlabel="" -Gsize=8,8\! -Gdpi=100 demos/partitioner/ising32_p4_$$i.dot -Tpng > demos/partitioner/ising32_p4_$$i.png; \
	done
	# Make width+height multiple of 2 : http://stackoverflow.com/a/20848224/4141520
	(cd demos/partitioner && $(FFMPEG) -r 10 -start_number 1 -i ising32_p4_%02d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 ising32_p4.mp4)

ALL_DEMOS += demos/partitioner/ising32_p4.gif

demos/partitioner/net100.xml : apps/gals_izhikevich/create_sparse_instance.py
	mkdir -p $(dir $@)
	python3.4 $< apps/gals_izhikevich/gals_izhikevich_graph_type.xml 320 80 3 > $@

demos/partitioner/net100_p4.gif : bin/partitioner providers/ising_spin.graph.so demos/partitioner/net100.xml
	X=$$(seq -w 0 29); \
	for i in $$X; do \
		bin/partitioner demos/partitioner/net100.xml --steps 100000000 --partitions 4 --prop-steps 0.$$i > demos/partitioner/net100_p4_$$i.dot ; \
		neato -Goverlap=false -Epenwidth="5" -Nlabel="" -Gsize=8,8\! -Gdpi=100 demos/partitioner/net100_p4_$$i.dot -Tpng > demos/partitioner/net100_p4_$$i.png; \
	done
	# Make width+height multiple of 2 : http://stackoverflow.com/a/20848224/4141520
	(cd demos/partitioner && $(FFMPEG) -r 10 -start_number 1 -i net100_p4_%02d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -crf 18 net100_p4.mp4)

