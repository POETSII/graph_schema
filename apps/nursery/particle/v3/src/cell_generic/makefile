# Tinsel root
TINSEL_ROOT ?= /POETS/tinsel
LIB_TINSEL_EMU_ROOT ?= /POETS/lib_tinsel_emu
GRAPH_SCHEMA_ROOT ?= /POETS/graph_schema
QUARTUS_ROOTDIR ?= /opt/altera/16.0/quartus

include $(TINSEL_ROOT)/globals.mk

RV_OBJCOPY = riscv32-unknown-elf-objcopy

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -Os -I $(INC) -I ../../include  -g
LDFLAGS = -melf32lriscv -G 0 

CFLAGS := $(CFLAGS) -I $(GRAPH_SCHEMA_ROOT)/apps/nursery/particle/v3/include
CFLAGS := $(CFLAGS) -I $(LIB_TINSEL_EMU_ROOT)/include

CFLAGS +=  -DNDEBUG=1 -DLIB_TINSEL_EMU_IS_TINSEL0P2_THREAD=1

.PHONY: all
all: code.v data.v

code.v: cell_generic.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text cell_generic.elf code.v

data.v: cell_generic.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents cell_generic.elf data.v

cell_generic.elf: cell_generic_softswitch.cpp ../shared/particle.cpp link.ld $(TINSEL_ROOT)/include/config.h $(TINSEL_ROOT)/include/tinsel.h entry.o  $(LIB_TINSEL_EMU_ROOT)/src/impl/tinsel0p2/tinsel0p2_thread.cpp parameters.hpp
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o tinsel0p2_thread.o $(LIB_TINSEL_EMU_ROOT)/src/impl/tinsel0p2/tinsel0p2_thread.cpp
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o cell_generic_softswitch.o cell_generic_softswitch.cpp
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o particle.o ../shared/particle.cpp
	riscv32-unknown-elf-ld $(LDFLAGS) -T link.ld -o $@ entry.o particle.o cell_generic_softswitch.o tinsel0p2_thread.o 
	#riscv32-unknown-elf-ld $(LDFLAGS) -T link.ld -o $@ entry.o cell_generic_softswitch.o tinsel0p2_thread.o 

entry.o:
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o entry.o entry.S


hostlink : Main.cpp  parameters.hpp
	$(CXX) -std=c++11 $(CPPFLAGS) -I $(TINSEL_ROOT)/include $< -o $@  \
		-ljtag_atlantic -ljtag_client \
		-L $(QUARTUS_ROOTDIR)/linux64/



$(LIB)/lib.o:
	make -C $(LIB)

link.ld: genld.sh
	./genld.sh > link.ld

$(TINSEL_ROOT)/include/config.h: $(TINSEL_ROOT)/config.py
	echo $(INC)
	echo $(TINSEL_ROOT)
	make -C $(TINSEL_ROOT)/include

TIMEOUT ?= 60

.PHONY: run-jtag
run-jtag: code.v data.v hostlink
	-killall -q -9 -u dt10 hostlink
	>&2 echo "Configuring"
	$(QUARTUS_ROOTDIR)/bin/quartus_pgm --quiet  Golden_top.cdf | ( grep -E -v "^\s*Info.*" || /bin/true)
	>&2 echo "Running hostlink"
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 ./hostlink -v -v -t $(TIMEOUT) code.v data.v 

.PHONY: run-sim
run-sim: code.v data.v
	hostlink-sim code.v data.v -c

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v
