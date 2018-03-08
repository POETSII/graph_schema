# Tinsel root
TINSEL_ROOT=/POETS/tinsel

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -Os -I $(INC) -I ../../include  -g
LDFLAGS = -melf32lriscv -G 0 

CFLAGS := $(CFLAGS) -I /POETS/graph_schema/apps/nursery/particle/v3/include
CFLAGS := $(CFLAGS) -I /POETS/lib_tinsel_emu/include

CFLAGS += -Os -DNDEBUG=1 -DLIB_TINSEL_EMU_IS_TINSEL0P2_THREAD=1

.PHONY: all
all: code.v data.v

code.v: hello.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text hello.elf code.v

data.v: hello.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents hello.elf data.v

cell_generic.elf: cell_generic_softswitch.cpp ../shared/particle.cpp link.ld $(INC)/config.h $(INC)/tinsel.h entry.o  /POETS/lib_tinsel_emu/src/impl/tinsel0p2/tinsel0p2_thread.cpp
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o tinsel0p2_thread.o /POETS/lib_tinsel_emu/src/impl/tinsel0p2/tinsel0p2_thread.cpp
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o cell_generic_softswitch.o cell_generic_softswitch.cpp
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o particle.o ../shared/particle.cpp
	$(RV_LD) $(LDFLAGS) -T link.ld -o $@ entry.o cell_generic_softswitch.o particle.o tinsel0p2_thread.o 

entry.o:
	riscv32-unknown-elf-g++ $(CFLAGS) -Wall -c -o entry.o entry.S

$(LIB)/lib.o:
	make -C $(LIB)

#link.ld: genld.sh
#	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: run-jtag
run-jtag: code.v data.v
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink code.v data.v -c

.PHONY: run-sim
run-sim: code.v data.v
	hostlink-sim code.v data.v -c

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v
