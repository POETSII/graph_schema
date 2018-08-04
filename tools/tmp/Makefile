# Tinsel root
TINSEL_ROOT=/home/sf306/poets-ecosystem/submodules/tinsel

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v run

code.v: ro.elf
	checkelf.sh ro.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text ro.elf code.v

data.v: ro.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents ro.elf data.v

ro.elf: ro.cpp ro.h link.ld $(INC)/config.h \
          $(INC)/tinsel.h entry.o $(LIB)/lib.o
	$(RV_CPPC) $(CFLAGS) -Wall -c -DTINSEL -o ro.o ro.cpp
	$(RV_LD) $(LDFLAGS) -T link.ld -o ro.elf entry.o ro.o $(LIB)/lib.o

entry.o:
	$(RV_CPPC) $(CFLAGS) -Wall -c -o entry.o entry.S

$(LIB)/lib.o:
	make -C $(LIB)

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

run: ro_host.cpp $(HL)/*.o
	g++ -O2 -I $(INC) -I $(HL) -o run ro_host.cpp $(HL)/*.o \
	  -ljtag_atlantic -ljtag_client -L $(QUARTUS_ROOTDIR)/linux64/ \
	  -Wl,-rpath,$(QUARTUS_ROOTDIR)/linux64 -lmetis

sim: ro_host.cpp $(HL)/sim/*.o
	g++ -O2 -I $(INC) -I $(HL) -o sim ro_host.cpp $(HL)/sim/*.o \
    -lmetis

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v run sim
