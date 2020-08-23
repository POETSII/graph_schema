GRAPH_SCHEMA ?= ../../..

POETS_EXTERNAL_INTERFACE_SPEC ?= $(wildcard ../../../../external_interface_spec)
POETS_EXTERNAL_INTERFACE_SPEC ?= $(wildcard /POETS/external_interface_spec)

ifeq ($(POETS_EXTERNAL_INTERFACE_SPEC),)
	HAVE_POETS_EXTERNAL_INTERFACE_SPEC=0
	CPPFLAGS += -I ${GRAPH_SCHEMA}/include/include_cache
else
	HAVE_POETS_EXTERNAL_INTERFACE_SPEC=1
	CPPFLAGS += -I $(POETS_EXTERNAL_INTERFACE_SPEC)/include
endif


CPPFLAGS += -std=c++17 -msse3 -mfpmath=sse -ffp-contract=off -fno-fast-math
CPPFLAGS += -I $(GRAPH_SCHEMA)/external/robin_hood
CPPFLAGS += -I $(GRAPH_SCHEMA)/include

CPPFLAGS += -gsplit-dwarf -gz

CPPFLAGS += -fmax-errors=5

CPPFLAGS += -fsanitize=undefined
CPPFLAGS += -fsanitize=address
CPPFLAGS += -fsanitize-undefined-trap-on-error

LIBXML_PKG_CONFIG_CPPFLAGS := $(shell pkg-config --cflags libxml++-2.6)
LIBXML_PKG_CONFIG_LDLIBS := $(shell pkg-config --libs-only-l libxml++-2.6)
LIBXML_PKG_CONFIG_LDFLAGS := $(shell pkg-config --libs-only-L --libs-only-other libxml++-2.6)

CPPFLAGS += -I include -W -Wall -Wno-unused-parameter -Wno-unused-variable
CPPFLAGS += $(LIBXML_PKG_CONFIG_CPPFLAGS)

LDLIBS += $(LIBXML_PKG_CONFIG_LDLIBS)
LDFLAGS += $(LIBXML_PKG_CONFIG_LDFLAGS)
LDFLAGS += -fuse-ld=gold -Wl,--gdb-index

PROVIDER_FLAGS=
ifneq ($(RELEASE_WITH_ASSERTS),)
CPPFLAGS += -O3
PROVIDER_FLAGS += --release-with-asserts
else ifneq ($(RELEASE_NO_ASSERTS),)
CPPFLAGS += -O3 -DNDEBUG=1
PROVIDER_FLAGS += --release-no-asserts
endif

#CPPFLAGS += -O3 -DNDEBUG=1

NEURON_TYPES=CUBA izhikevich poisson
ALGORITHM_TYPES=HwIdle GALSExact CountExact

define algorithm_neuron_template

providers/SNN_$(1)_$(2)_graph_type.xml : bin/create_graph_type graphs/SNN_$(1)_template.xml
	mkdir -p providers
	bin/create_graph_type $(2) < graphs/SNN_$(1)_template.xml > $$@

providers/SNN_$(1)_$(2)_graph_type.external.cpp : bin/create_graph_type graphs/SNN_$(1)_template.external.cpp
	mkdir -p providers
	bin/create_graph_type $(2) < graphs/SNN_$(1)_template.external.cpp > $$@

providers/SNN_$(1)_$(2).graph.so : providers/SNN_$(1)_$(2)_graph_type.xml providers/SNN_$(1)_$(2)_graph_type.external.cpp
	${GRAPH_SCHEMA}/tools/compile_graph_as_provider.sh $(PROVIDER_FLAGS) -I include/neuron_models/ --working-dir providers $$< -o $$@

ALL_GRAPH_TYPES += providers/SNN_$(1)_$(2)_graph_type.xml 
ALL_GRAPH_PROVIDERS += providers/SNN_$(1)_$(2).graph.so
endef

$(foreach nt,$(NEURON_TYPES),$(foreach at,$(ALGORITHM_TYPES),$(eval $(call algorithm_neuron_template,$(at),$(nt)))))

src/%.o : src/%.cpp
	$(CXX) -c $(CPPFLAGS) -o $@ $<

bin/% : src/%.o src/neuron_factory.o
	mkdir -p bin
	$(CXX) $(CPPFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

all_generators : bin/generate_izhikevich_sparse bin/generate_CUBA bin/generate_poisson

all_tools : bin/reference_simulator bin/create_graph_type bin/create_graph_instance_v2

all_tests : bin/test_dumb_snn_files bin/test_sink_snn bin/test_source_snn bin/test_source_sink_inmem

all_graph_types : $(ALL_GRAPH_TYPES)
all_graph_providers : $(ALL_GRAPH_PROVIDERS)

all_programs : all_generators all_tools all_graph_providers

clean :
	-rm -f bin/*
	-rm -f providers/*
	-rm -f src/*.o