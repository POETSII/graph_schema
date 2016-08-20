.PRECIOUS : output/%.checked

export PYTHONPATH = tools

SHELL=/bin/bash

CPPFLAGS += -I include -W -Wall -Wno-unused-parameter
CPPFLAGS += $(shell pkg-config --cflags libxml++-2.6)

LDFLAGS += $(shell pkg-config --libs libxml++-2.6)

ifeq ($(OS),Windows_NT)
SO_CPPFLAGS += -shared
else
SO_CPPFLAGS += -dynamiclib -fPIC
endif

CPPFLAGS += -std=c++11 -g
#CPPFLAGS += -O0

CPPFLAGS += -O3 -DNDEBUG=1

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


%.checked : %.xml $(JING) master/virtual-graph-schema.rnc derived/virtual-graph-schema.xsd
	java -jar $(JING) -c master/virtual-graph-schema.rnc $*.xml
	java -jar $(JING) derived/virtual-graph-schema.xsd $*.xml
	$(PYTHON) tools/print_graph_properties.py < $*.xml
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

bin/epoch_sim.s : tools/epoch_sim.cpp
	mkdir -p bin
	$(CXX) -S $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

bin/queue_sim : tools/queue_sim.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)


define provider_rules_template

providers/$1.graph.cpp : apps/$1/$1_graph_type.xml
	mkdir -p providers
	$$(PYTHON) tools/render_graph_as_cpp.py < apps/$1/$1_graph_type.xml > providers/$1.graph.cpp

providers/$1.graph.so : providers/$1.graph.cpp
	g++ $$(CPPFLAGS) $$(SO_CPPFLAGS) $$< -o $$@ $$(LDFLAGS)

$1_provider : providers/$1.graph.so

all_providers : clock_tree_provider

endef

include apps/clock_tree/makefile.inc
include apps/ising_spin/makefile.inc
include apps/clocked_izhikevich/makefile.inc
include apps/gals_izhikevich/makefile.inc
include apps/gals_heat/makefile.inc

demos : $(ALL_DEMOS)


all_tools : bin/print_graph_properties bin/epoch_sim


VIRTUAL_ALL_TESTS := $(patsubst test/virtual/%.xml,%,$(wildcard test/virtual/*.xml))

tt :
	echo $(VIRTUAL_ALL_TESTS)

validate-virtual : $(foreach t,$(VIRTUAL_ALL_TESTS),validate-virtual/$(t) output/virtual/$(t).svg output/virtual/$(t).graph.cpp output/virtual/$(t).graph.so)

test : $(ALL_TESTS)

demo : $(ALL_DEMOS)

clean :
	-find . -iname '*~' -exec rm {} ';'  # Get rid of emacs temporaries
	-rm -rf output/virtual/*.dot output/virtual/*.svg
	-rm -rf demos/*
	-rm -rf providers/*
	-rm -rf external/trang-20091111
	-rm -rf external/jing-20081028
	-rm -rf rapidjson-master


