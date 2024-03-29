.PRECIOUS : output/%.checked

export PYTHONPATH = tools

POETS_EXTERNAL_INTERFACE_SPEC ?= $(wildcard ../external_interface_spec)
POETS_EXTERNAL_INTERFACE_SPEC ?= $(wildcard /POETS/external_interface_spec)

ifeq ($(POETS_EXTERNAL_INTERFACE_SPEC),)
	HAVE_POETS_EXTERNAL_INTERFACE_SPEC=0
	CPPFLAGS += -I include/include_cache
else
	HAVE_POETS_EXTERNAL_INTERFACE_SPEC=1
	CPPFLAGS+= -I $(POETS_EXTERNAL_INTERFACE_SPEC)/include
endif

SHELL=/bin/bash

# For some reason pkg-config stopped working on byron...

$(info $(shell pkg-config libxml++-2.6 ; echo $$?))
ifeq ($(shell pkg-config libxml++-2.6 ; echo $$?),0)
LIBXML_PKG_CONFIG_CPPFLAGS := $(shell pkg-config --cflags libxml++-2.6)
LIBXML_PKG_CONFIG_LDLIBS := $(shell pkg-config --libs-only-l libxml++-2.6)
LIBXML_PKG_CONFIG_LDFLAGS := $(shell pkg-config --libs-only-L --libs-only-other libxml++-2.6)
else
$(info "nooo")
LIBXML_PKG_CONFIG_CPPFLAGS := -I/usr/include/libxml++-2.6 -I/usr/lib/x86_64-linux-gnu/libxml++-2.6/include -I/usr/include/libxml2 -I/usr/include/glibmm-2.4 -I/usr/lib/x86_64-linux-gnu/glibmm-2.4/include -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/sigc++-2.0 -I/usr/lib/x86_64-linux-gnu/sigc++-2.0/include
LIBXML_PKG_CONFIG_LDLIBS := -lxml++-2.6 -lxml2 -lglibmm-2.4 -lgobject-2.0 -lglib-2.0 -lsigc-2.0
LIBXML_PKG_CONFIG_LDFLAGS := 
endif

CPPFLAGS += -I include -W -Wall -Wno-unused-parameter -Wno-unused-variable
CPPFLAGS += $(LIBXML_PKG_CONFIG_CPPFLAGS)
CPPFLAGS += -Wno-unused-local-typedefs
CPPFLAGS += -I providers
CPPFLAGS += -I external/robin_hood

LDLIBS += $(LIBXML_PKG_CONFIG_LDLIBS)
LDFLAGS += $(LIBXML_PKG_CONFIG_LDFLAGS)

#LDLIBS += -lboost_filesystem -lboost_system

ifeq ($(OS),Windows_NT)
SO_CPPFLAGS += -shared
else
# http://stackoverflow.com/a/12099167
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
# libxml++ uses auto_ptr, which is deprecated in c++17. This brings it back for clang on OSX.
CPPFLAGS += -D_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR=1
CPPFLAGS += -I /opt/homebrew/include
SO_CPPFLAGS += -dynamiclib -fPIC
# These are correct warnings, but ....
CPPFLAGS += -Wno-inconsistent-missing-override -Wno-deprecated-declarations
else
SO_CPPFLAGS += -shared -fPIC
LDFLAGS += -pthread
LDLIBS += -ldl -fPIC
endif
endif

# Default is moderately optimised with asserts
# dwarf-4 sometimes produces better debug info (?)
CPPFLAGS += -std=c++17 -O1 -gdwarf-4 

# Try to help libubsan merge RTTI on imported classes
# https://stackoverflow.com/a/57379835
CPPFLAGS += -rdynamic

ifeq ($(UNAME_S),Darwin)
# ? Anything needed here to control denormals?
else
# In some cases we want to try to achieve reproducible floating-point,
# including with no denormals. This (hopefully) convinces gcc not to
# optimise past those things. 
# Using SSE is generally a good things anyway
CPPFLAGS+= -mfpmath=sse -msse2
endif
# These may inhibit optimisations
CPPFLAGS+= -frounding-math -fsignaling-nans

# Used to indicate that functions should be linked in as
# seperate objects.
CPPFLAGS += -DPOETS_GRAPH_SCHEMA_SEPERATE_COMPILATION=1

# Last optimisation flag overrides
CPPFLAGS_DEBUG = $(CPPFLAGS) -O0 -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address

# Release is max optimised with no asserts
CPPFLAGS_RELEASE = $(CPPFLAGS) -O3 -DNDEBUG=1



TRANG = external/trang-20091111/trang.jar
JING = external/jing-20081028/bin/jing.jar
RNG_SVG = external/rng-svg/build.xml

FFMPEG := $(shell which ffmpeg)
ifeq ($(FFMPEG),)
FFMPEG := $(shell which avconv)
endif

ff :
	echo $(FFMPEG)

FFMPEG := $(shell which ffmpeg)
ifeq ($(FFMPEG),)
 FFMPEG := $(shell which avconv)
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

jing : $(JING)

$(RNG_SVG) : external/rng-svg-latest.zip
	mkdir external/rng-svg
	(cd external/rng-svg && unzip -o ../rng-svg-latest)
	touch $@



graph_library : $(wildcard tools/graph/*.py)

derived/%.rng derived/%.xsd : master/%.rnc $(TRANG) $(JING) $(wildcard master/%-example*.xml)
	# Check the claimed examples in order to make sure that
	# they validate
	for i in master/$*-example*.xml master/$*-exemplar*.xml ; do \
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

build-virtual-schema-v3 : derived/virtual-graph-schema-v3.rng derived/virtual-graph-schema-v3.xsd

regenerate-random :
	python3.4 tools/create_random_graph.py 1 > test/virtual/random1.xml
	python3.4 tools/create_random_graph.py 2 > test/virtual/random2.xml
	python3.4 tools/create_random_graph.py 4 > test/virtual/random3.xml
	python3.4 tools/create_random_graph.py 8 > test/virtual/random4.xml


%.checked : %.xml $(JING) master/virtual-graph-schema-v3.rnc derived/virtual-graph-schema-v3.xsd
	java -jar $(JING) -c master/virtual-graph-schema-v3.rnc $*.xml
	java -jar $(JING) derived/virtual-graph-schema-v3.xsd $*.xml
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


bin/create_gals_heat_instance : apps/gals_heat/create_gals_heat_instance.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)


# Things extracted into objects to speed up compilation and make ccache more effective
STANDARD_IMPL_SRCS = \
  graph_persist_sax_writer_impl \
  graph_persist_dom_reader_impl \
  xml_pull_parser_impl

STANDARD_IMPL_OBJS = $(foreach s, $(STANDARD_IMPL_SRCS), obj/$s.o )
STANDARD_IMPL_OBJS_DEBUG = $(foreach s, $(STANDARD_IMPL_SRCS), obj/$s.debug.o )
STANDARD_IMPL_OBJS_RELEASE = $(foreach s, $(STANDARD_IMPL_SRCS), obj/$s.release.o )

obj/%.o : src/%.cpp
	mkdir -p obj
	$(CXX) -c $(CPPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

obj/%.debug.o : src/%.cpp
	mkdir -p obj
	$(CXX) -c $(CPPFLAGS_DEBUG) $< -o $@ $(LDFLAGS) $(LDLIBS)

obj/%.release.o : src/%.cpp
	mkdir -p obj
	$(CXX) -c $(CPPFLAGS_RELEASE) $< -o $@ $(LDFLAGS) $(LDLIBS)


###############
## Rules for the actual binaries

bin/%.o : tools/%.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $(CPPFLAGS) $< -o $@

bin/% : bin/%.o $(STANDARD_IMPL_OBJS)	
	$(CXX) $(CPPFLAGS) $(STANDARD_IMPL_OBJS) $@.o  -o $@ $(LDFLAGS) $(LDLIBS)


bin/%.debug.o : tools/%.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $(CPPFLAGS_DEBUG) $< -o $@

bin/%.debug : bin/%.debug.o $(STANDARD_IMPL_OBJS_DEBUG)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS_DEBUG)  $(STANDARD_IMPL_OBJS_DEBUG) $@.o -o $@ $(LDFLAGS) $(LDLIBS)


bin/%.release.o : tools/%.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $(CPPFLAGS_RELEASE) $< -o $@

bin/%.release : bin/%.release.o $(STANDARD_IMPL_OBJS_RELEASE)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS_RELEASE)  $(STANDARD_IMPL_OBJS_RELEASE) $@.o -o $@ $(LDFLAGS) $(LDLIBS)


define provider_rules_template
# $1 : name
# $2 : optional full xml path (e.g. for things in nursery)
# $3 : no default. If not empty, don't add to default build

ifeq ($2,)
$1_src_xml=apps/$1/$1_graph_type.xml
else
$1_src_xml=$2
endif

providers/$1.graph.so : $$($1_src_xml)
	tools/compile_graph_as_provider.sh $$< --working-dir providers -o $$@

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
include apps/gals_heat_fix_noedge/makefile.inc
include apps/storm/makefile.inc

include apps/amg/makefile.inc
include apps/apsp/makefile.inc
include apps/betweeness_centrality/makefile.inc
include apps/relaxation_heat/makefile.inc

include apps/firefly_sync/makefile.inc
#include apps/firefly_nosync/makefile.inc

include apps/gals_heat_float/makefile.inc
include apps/gals_heat_protocol_only/makefile.inc

include apps/tests/externals/tests_externals.inc
include apps/tests/supervisors/tests_supervisors.inc


# Non-default
include apps/nursery/airfoil/airfoil.inc
include apps/nursery/nested_arrays/makefile.inc
include apps/nursery/apsp_vec_barrier/apsp_vec_barrier.inc
include apps/nursery/barrier_izhikevich_clustered/barrier_izhikevich_clustered.inc
include apps/nursery/pulsed_izhikevich/pulsed_izhikevich.inc
include apps/nursery/self_clocked_izhikevich/self_clocked_izhikevich.inc
include apps/nursery/ising_spin_fix_ext/ising_spin_fix_ext.inc
include apps/nursery/pressure_sync/pressure_sync.inc

#TODO : Defunct?
include tools/partitioner.inc

# DPD ONLY

#include ../POETS/dpd-xml/makefile.inc

demos : $(ALL_DEMOS)

all_tools : bin/print_graph_properties bin/epoch_sim bin/graph_sim bin/hash_sim2 bin/structurally_compare_graph_types \
	bin/convert_graph_to_v4 bin/convert_graph_to_base85 bin/convert_graph_to_v3 \
	bin/topologically_compare_graph_instances bin/topologically_diff_graph_instances

#############################
# Most testing of graphs is done with epoch_sim. Give graph_sim some exercise here

%.xml.graph_sim : %.xml
	bin/graph_sim $< && touch $@

test_graph_sim : $(foreach p,$(ALL_TEST_XML), $(p).graph_sim )

ALL_TESTS += test_graph_sim

##

VIRTUAL_ALL_TESTS := $(patsubst test/virtual/%.xml,%,$(wildcard test/virtual/*.xml))

tt :
	echo $(VIRTUAL_ALL_TESTS)

validate-virtual : $(foreach t,$(VIRTUAL_ALL_TESTS),validate-virtual/$(t) output/virtual/$(t).svg output/virtual/$(t).graph.cpp output/virtual/$(t).graph.so)

.PHONY : test

test_no_clean :
	@>&2 echo "# Building tools"
	@make -j 4 -k all_tools 2>/dev/null > /dev/null
	@>&2 echo "# Building providers"
	@make -j 4 -k all_providers 2>/dev/null > /dev/null
	@>&2 echo "# Running bats"
	@bats  -t -r .

test :
	@>&2 echo "# Cleaning directory"
	@make clean 2> /dev/null > /dev/null
	@>&2 echo "# Building tools"
	@make -j 4 -k all_tools 2>/dev/null > /dev/null
	@>&2 echo "# Building providers"
	@make -j 4 -k all_providers 2>/dev/null > /dev/null
	@>&2 echo "# Running bats"
	@bats  -t -r .

test_parallel :
	@>&2 echo "# Cleaning directory"
	@make clean 2> /dev/null > /dev/null
	@>&2 echo "# Building tools"
	@make -j 4 -k all_tools 2>/dev/null > /dev/null
	@>&2 echo "# Building providers"
	@make -j 4 -k all_providers 2>/dev/null > /dev/null
	@>&2 echo "# Running bats"
	@bats -j 4 -t -r .

softswitch : $(ALL_SOFTSWITCH)
	echo $(ALL_SOFTSWITCH)

demo : $(ALL_DEMOS)

clean :
	-find . -iname '*~' -exec rm {} ';'  # Get rid of emacs temporaries
	-rm -rf output/virtual/*.dot output/virtual/*.svg
	-rm -rf bin/*
	-rm -rf demos/*
	-rm -rf providers/*
	-rm -f src/*.o
	-rm -rf external/trang-20091111
	-rm -rf external/jing-20081028
	-rm -rf derived/*
