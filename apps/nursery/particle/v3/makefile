LIB_TINSEL_EMU_ROOT ?= /POETS/lib_tinsel_emu

LIB_TINSEL_EMU_ROOT := $(realpath $(LIB_TINSEL_EMU_ROOT))

CPPFLAGS += -g -W -Wall -std=c++11
CPPFLAGS += -Iinclude
CPPFLAGS += -I$(LIB_TINSEL_EMU_ROOT)/include
CPPFLAGS += -pthread

#CPPFLAGS += -O3 -DNDEBUG=1

LDFLAGS += -L/POETS/lib_tinsel_emu/lib

# Make executables export their own function (softswitchMain)
LDFLAGS += -Wl,--export-dynamic

TINSEL_UNIX_LDLIBS = $(LIB_TINSEL_EMU_ROOT)/lib/lib_tinsel_emu_unix.a -ldl
TINSEL_SEQ_LDLIBS = /POETS/lib_tinsel_emu/lib/lib_tinsel_emu_seq.a -ldl
TINSEL_TINSEL0P2_LDLIBS = /POETS/lib_tinsel_emu/lib/lib_tinsel_emu_tinsel0p2.a


bin/simple : src/simple/simple.cpp src/shared/particle.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

define add_impl

bin/$1_unix : src/$1/$1.cpp $(wildcard src/$1/$1_*.cpp) src/shared/particle.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $$^ -o $$@ $(LDFLAGS) $(LDLIBS) $(TINSEL_UNIX_LDLIBS)

bin/$1_seq : src/$1/$1.cpp $(wildcard src/$1/$1_*.cpp) src/shared/particle.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $$^ -o $$@ $(LDFLAGS) $(LDLIBS) $(TINSEL_SEQ_LDLIBS)


test_$1_unix : bin/$1_unix
	mkdir -p output
	bin/$1_unix --tinsel-hard-log-level 0 --tinsel-soft-log-level 0 -N 1 > output/$1_particles.csv
	python3 scripts/plot_particles.py output/$1_particles.csv output/$1_particles.mp4

bin/$1_tinsel0p2 : src/$1/$1.cpp src/shared/particle.cpp
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $$^ -o $$@ $(LDFLAGS) $(LDLIBS) $(TINSEL_TINSEL0P2_LDLIBS)

endef

IMPLS = cell_1x1 cell_1x2 cell_2x2 cell_4x4 cell_generic

$(foreach i,$(IMPLS),$(eval $(call add_impl,$(i))))
