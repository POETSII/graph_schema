SHELL=/bin/bash

CPPFLAGS += -I include
CPPFLAGS += $(shell pkg-config --cflags libxml++-2.6)

LDFLAGS += $(shell pkg-config --libs libxml++-2.6)

CPPFLAGS += -std=c++11

TRANG = external/trang-20091111/trang.jar
JING = external/jing-20081028/bin/jing.jar

$(TRANG) : external/trang-20091111.zip
	(cd external && unzip trang-20091111.zip)
	touch $@

$(JING) : external/jing-20081028.zip
	(cd external && unzip jing-20081028.zip)
	touch $@

derived/%.rng derived/%.xsd : master/%.rnc
	java -jar $(TRANG) -I rnc -O rng $< master/$*.rng
	java -jar $(TRANG) -I rnc -O xsd $< master/$*.xsd

virtual-graph-schema-build : derived/virtual-graph-schema.rng derived/virtual-graph-schema.xsd


virtual-validate-% : master/virtual-graph-schema.rnc test/virtual/%.xml
	java -jar $(JING) -c master/virtual-graph-schema.rnc test/virtual/$*.xml

VIRTUAL_ALL_TESTS := $(patsubst test/virtual/%.xml,%,$(wildcard test/virtual/*.xml))

tt :
	echo $(VIRTUAL_ALL_TESTS)

virtual-validate : $(foreach t,$(VIRTUAL_ALL_TESTS),virtual-validate-$(t))