CPPFLAGS += -I ../../../include

LIBXML_PKG_CONFIG_CPPFLAGS := $(shell pkg-config --cflags libxml++-2.6)
LIBXML_PKG_CONFIG_LDLIBS := $(shell pkg-config --libs-only-l libxml++-2.6)
LIBXML_PKG_CONFIG_LDFLAGS := $(shell pkg-config --libs-only-L --libs-only-other libxml++-2.6)

CPPFLAGS += -I include -W -Wall -Wno-unused-parameter -Wno-unused-variable
CPPFLAGS += $(LIBXML_PKG_CONFIG_CPPFLAGS)
CPPFLAGS += -Wno-unused-local-typedefs
CPPFLAGS += -g

LDLIBS += $(LIBXML_PKG_CONFIG_LDLIBS)
LDFLAGS += $(LIBXML_PKG_CONFIG_LDFLAGS)
