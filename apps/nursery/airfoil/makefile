CPPFLAGS += -I/usr/include/hdf5/serial -std=c++14
CPPFLAGS += -W -Wall -Wno-unused-variable -Wno-unused-parameter

LDFLAGS += -L/usr/lib/x86_64-linux-gnu
LDLIBS += -lhdf5_serial -lhdf5_cpp

CPPFLAGS += -DNDEBUG=1 -O3

mock_extra : LDLIBS += -ltbb
