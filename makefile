.PRECIOUS : output/%.checked

SHELL=/bin/bash

CPPFLAGS += -I include
CPPFLAGS += $(shell pkg-config --cflags libxml++-2.6)

LDFLAGS += $(shell pkg-config --libs libxml++-2.6)

ifeq ($(OS),Windows_NT)
SO_CPPFLAGS += -shared
else
SO_CPPFLAGS += -dynamiclib -fPIC
endif

CPPFLAGS += -std=c++11 -g

CPPFLAGS += -DNDEBUG=1 -O2

TRANG = external/trang-20091111/trang.jar
JING = external/jing-20081028/bin/jing.jar

# TODO : OS X specific
PYTHON = python3.4

$(TRANG) : external/trang-20091111.zip
	(cd external && unzip -o trang-20091111.zip)
	touch $@

$(JING) : external/jing-20081028.zip
	(cd external && unzip -o jing-20081028.zip)
	touch $@

rapidjson : external/rapidjson-master.zip
	(cd external && unzip -o rapidjson-master)

CPPFLAGS += -I external/rapidjson-master/include

graph_library : $(wildcard tools/graph/*.py)

derived/%.rng derived/%.xsd : master/%.rnc $(TRANG)
	java -jar $(TRANG) -I rnc -O rng master/$*.rnc derived/$*.rng
	java -jar $(TRANG) -I rnc -O xsd master/$*.rnc  derived/$*.xsd

build-virtual-schema : derived/virtual-graph-schema.rng derived/virtual-graph-schema.xsd

regenerate-random :
	python3.4 tools/create_random_graph.py 1 > test/virtual/random1.xml
	python3.4 tools/create_random_graph.py 2 > test/virtual/random2.xml
	python3.4 tools/create_random_graph.py 4 > test/virtual/random3.xml
	python3.4 tools/create_random_graph.py 8 > test/virtual/random4.xml


output/%.checked : test/virtual/%.xml $(JING) master/virtual-graph-schema.rnc derived/virtual-graph-schema.xsd
	java -jar $(JING) -c master/virtual-graph-schema.rnc test/virtual/$*.xml
	java -jar $(JING) derived/virtual-graph-schema.xsd test/virtual/$*.xml
	$(PYTHON) tools/print_graph_properties.py < test/virtual/$*.xml
	touch $@

validate-virtual/% : output/%.checked
	echo ""

output/%.svg output/%.dot : test/%.xml tools/render_graph_as_dot.py graph_library validate-%
	mkdir -p $(dir output/$*)
	$(PYTHON) tools/render_graph_as_dot.py < test/$*.xml > output/$*.dot
	neato -Tsvg output/$*.dot > output/$*.svg

output/%.graph.cpp : test/%.xml graph_library
	mkdir -p $(dir output/$*)
	$(PYTHON) tools/render_graph_as_cpp.py < test/$*.xml > output/$*.graph.cpp

output/%.graph.cpp : apps/%.xml graph_library
	mkdir -p $(dir output/$*)
	$(PYTHON) tools/render_graph_as_cpp.py < apps/$*.xml > output/$*.graph.cpp

output/%.graph.so : output/%.graph.cpp
	g++ $(CPPFLAGS) $(SO_CPPFLAGS) $< -o $@ $(LDFLAGS)

bin/print_graph_properties : tools/print_graph_properties.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

bin/epoch_sim : tools/epoch_sim.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

all_tools : bin/print_graph_properties bin/epoch_sim


VIRTUAL_ALL_TESTS := $(patsubst test/virtual/%.xml,%,$(wildcard test/virtual/*.xml))

tt :
	echo $(VIRTUAL_ALL_TESTS)

validate-virtual : $(foreach t,$(VIRTUAL_ALL_TESTS),validate-virtual/$(t) output/virtual/$(t).svg output/virtual/$(t).graph.cpp output/virtual/$(t).graph.so)

clean :
	-find . -iname '*~' -exec rm {} ';'  # Get rid of emacs temporaries
	-rm output/virtual/*.dot output/virtual/*.svg

