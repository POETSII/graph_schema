.PRECIOUS : output/%.checked

export PYTHONPATH = tools

SHELL=/bin/bash

CPPFLAGS += -I include -W -Wall -Wno-unused-parameter -Wno-unused-variable
CPPFLAGS += $(shell pkg-config --cflags libxml++-2.6)
CPPFLAGS += -Wno-unused-local-typedefs
CPPFLAGS += -I providers

LDLIBS += $(shell pkg-config --libs-only-l libxml++-2.6)
LDFLAGS += $(shell pkg-config --libs-only-L --libs-only-other libxml++-2.6)

#LDLIBS += -lboost_filesystem -lboost_system

ifeq ($(OS),Windows_NT)
SO_CPPFLAGS += -shared
else
# http://stackoverflow.com/a/12099167
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SO_CPPFLAGS += -dynamiclib -fPIC
else
SO_CPPFLAGS += -shared -fPIC
LDFLAGS += -pthread
LDLIBS += -ldl -fPIC
endif
endif

CPPFLAGS += -std=c++11 -g
CPPFLAGS += -O0

#CPPFLAGS += -O2 -fno-omit-frame-pointer -ggdb -DNDEBUG=1



TRANG = external/trang-20091111/trang.jar
JING = external/jing-20081028/bin/jing.jar
RNG_SVG = external/rng-svg/build.xml

FFMPEG := $(shell which ffmpeg)
ifeq ($(FFMPEG),)
FFMPEG := $(shell which avconv)
endif

ff :
	echo $(FFMPEG)

FFMPEG = $(shell which ffmpeg)
ifeq ($(FFMPEG),)
 FFMPEG = $(shell which avconv)
endif

# TODO : OS X specific
PYTHON = python3

$(TRANG) : external/trang-20091111.zip
	if [[ ! -f $(TRANG) ]] ; then \
		(cd external && unzip -o trang-20091111.zip) \
	fi
	touch $@

$(JING) : external/jing-20081028.zip
	if [[ ! -f $(JING) ]] ; then \
		(cd external && unzip -o jing-20081028.zip) \
	fi
	touch $@

$(RNG_SVG) : external/rng-svg-latest.zip
	mkdir external/rng-svg
	(cd external/rng-svg && unzip -o ../rng-svg-latest)
	touch $@



graph_library : $(wildcard tools/graph/*.py)

derived/%.rng derived/%.xsd : master/%.rnc $(TRANG) $(JING) $(wildcard master/%-example*.xml)
	# Check the claimed examples in order to make sure that
	# they validate
	for i in master/$*-example*.xml; do \
		echo "Checking file $$i"; \
		java -jar $(JING) -c master/$*.rnc $$i; \
	done
	# Update the derived shemas
	mkdir -p derived
	java -jar $(TRANG) -I rnc -O rng master/$*.rnc derived/$*.rng
	java -jar $(TRANG) -I rnc -O xsd master/$*.rnc derived/$*.xsd



build-virtual-schema-v1 : derived/virtual-graph-schema-v1.rng derived/virtual-graph-schema-v1.xsd

build-virtual-schema-v2 : derived/virtual-graph-schema-v2.rng derived/virtual-graph-schema-v2.xsd

build-virtual-schema-v2.1 : derived/virtual-graph-schema-v2.1.rng derived/virtual-graph-schema-v2.1.xsd

regenerate-random :
	python3.4 tools/create_random_graph.py 1 > test/virtual/random1.xml
	python3.4 tools/create_random_graph.py 2 > test/virtual/random2.xml
	python3.4 tools/create_random_graph.py 4 > test/virtual/random3.xml
	python3.4 tools/create_random_graph.py 8 > test/virtual/random4.xml


%.checked : %.xml $(JING) master/virtual-graph-schema-v2.2.rnc derived/virtual-graph-schema-v2.2.xsd
	java -jar $(JING) -c master/virtual-graph-schema-v2.2.rnc $*.xml
	java -jar $(JING) derived/virtual-graph-schema-v2.2.xsd $*.xml
	touch $@

validate-virtual/% : output/%.checked
	echo ""

output/%.svg output/%.dot : test/%.xml tools/render_graph_as_dot.py graph_library validate-%
	mkdir -p $(dir output/$*)
	$(PYTHON) tools/render_graph_as_dot.py < test/$*.xml > output/$*.dot
	neato -Tsvg output/$*.dot > output/$*.svg

output/%.graph.cpp : test/%.xml graph_library
	mkdir -p $(dir output/$*)
	$(PYTHON) tools/render_graph_as_cpp.py test/$*.xml output/$*.graph.cpp

output/%.graph.cpp : apps/%.xml graph_library
	mkdir -p $(dir output/$*)
	$(PYTHON) tools/render_graph_as_cpp.py apps/$*.xml output/$*.graph.cpp

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

bin/create_gals_heat_instance : apps/gals_heat/create_gals_heat_instance.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

bin/% : tools/%.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)


define provider_rules_template
# $1 : name
# $2 : optional full xml path (e.g. for things in nursery)
# $3 : no default. If not empty, don't add to default build

ifeq ($2,)
$1_src_xml=apps/$1/$2$1_graph_type.xml
else
$1_src_xml=$2
endif

providers/$1.graph.cpp providers/$1.graph.hpp : $$($1_src_xml) $(JING)
	mkdir -p providers
	java -jar $(JING) -c master/virtual-graph-schema-v2.2.rnc $$($1_src_xml)
	$$(PYTHON) tools/render_graph_as_cpp.py $$($1_src_xml) providers/$1.graph.cpp
	$$(PYTHON) tools/render_graph_as_cpp.py --header < $$($1_src_xml) > providers/$1.graph.hpp

providers/$1.graph.so : providers/$1.graph.cpp
	g++ $$(CPPFLAGS) -Wno-unused-but-set-variable $$(SO_CPPFLAGS) $$< -o $$@ $$(LDFLAGS) $(LDLIBS)

$1_provider : providers/$1.graph.so

ifeq ($3,)
all_providers : $1_provider
endif

endef

SOFTSWITCH_DIR = ../toy_softswitch

ALL_SOFTSWITCH =

define softswitch_instance_template
# $1 = name
# $2 = input-path.xml
# $3 = threads

$(SOFTSWITCH_DIR)/generated/apps/$1_threads$3/present : $2
	mkdir -p $(SOFTSWITCH_DIR)/generated/apps/$1_threads$3/
	tools/render_graph_as_softswitch.py --log-level INFO $2 --threads $3 --dest $(SOFTSWITCH_DIR)/generated/apps/$1_threads$3/
	touch $$@

ALL_SOFTSWITCH := $(ALL_SOFTSWITCH) $(SOFTSWITCH_DIR)/generated/apps/$1_threads$3/present

endef

include apps/clock_tree/makefile.inc
include apps/ising_spin/makefile.inc
include apps/ising_spin_fix/makefile.inc
include apps/clocked_izhikevich/makefile.inc
include apps/clocked_izhikevich_fix/makefile.inc
include apps/gals_izhikevich/makefile.inc
include apps/gals_heat/makefile.inc
include apps/gals_heat_fix/makefile.inc
#include apps/gals_heat_fix_noedge/makefile.inc
include apps/storm/makefile.inc

include apps/amg/makefile.inc
include apps/apsp/makefile.inc

include apps/firefly_sync/makefile.inc
#include apps/firefly_nosync/makefile.inc

include apps/gals_heat_float/makefile.inc

# Non-default
include apps/nursery/airfoil/airfoil.inc
include apps/nursery/relaxation_heat/makefile.inc

include test/io/makefile.inc

#TODO : Defunct?
include tools/partitioner.inc

demos : $(ALL_DEMOS)


all_tools : bin/print_graph_properties bin/epoch_sim
#bin/queue_sim


VIRTUAL_ALL_TESTS := $(patsubst test/virtual/%.xml,%,$(wildcard test/virtual/*.xml))

tt :
	echo $(VIRTUAL_ALL_TESTS)

validate-virtual : $(foreach t,$(VIRTUAL_ALL_TESTS),validate-virtual/$(t) output/virtual/$(t).svg output/virtual/$(t).graph.cpp output/virtual/$(t).graph.so)

test_list :
	echo $(ALL_TESTS)

test : all_tools $(ALL_TESTS)

softswitch : $(ALL_SOFTSWITCH)
	echo $(ALL_SOFTSWITCH)

demo : $(ALL_DEMOS)

clean :
	-find . -iname '*~' -exec rm {} ';'  # Get rid of emacs temporaries
	-rm -rf output/virtual/*.dot output/virtual/*.svg
	-rm -rf bin/*
	-rm -rf demos/*
	-rm -rf providers/*
	-rm -rf external/trang-20091111
	-rm -rf external/jing-20081028
	-rm -rf derived/*
