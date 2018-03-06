CPPFLAGS += -g -W -Wall -std=c++11
CPPFLAGS += -I /POETS/lib_tinsel_emu/include
CPPFLAGS += -pthread

#CPPFLAGS += -O3 -DNDEBUG=1

LDFLAGS += -L /POETS/lib_tinsel_emu/lib

# Make executables export their own function (softswitchMain)
LDFLAGS += -Wl,--export-dynamic

simple : particle.cpp

cell_1x1 : particle.cpp

cell_1x1 : LDLIBS += /POETS/lib_tinsel_emu/lib/lib_tinsel_emu_unix.a -ldl

cell_1x2 : particle.cpp

cell_1x2 : LDLIBS += /POETS/lib_tinsel_emu/lib/lib_tinsel_emu_unix.a -ldl
