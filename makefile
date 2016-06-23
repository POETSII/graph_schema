SHELL=/bin/bash

CPPFLAGS += -I include
CPPFLAGS += $(shell pkg-config --cflags libxml++-2.6)

LDFLAGS += $(shell pkg-config --libs libxml++-2.6)

CPPFLAGS += -std=c++11

TRANG = external/trang-20091111/trang.jar
JING = external/jing-20081028/bin/jing.jar

# TODO : OS X specific
PYTHON = python3.4

$(TRANG) : external/trang-20091111.zip
	(cd external && unzip trang-20091111.zip)
	touch $@

$(JING) : external/jing-20081028.zip
	(cd external && unzip jing-20081028.zip)
	touch $@

graph_library : $(wildcard tools/graph/*.py)

derived/%.rng derived/%.xsd : master/%.rnc $(TRANG)
	java -jar $(TRANG) -I rnc -O rng master/$*.rnc derived/$*.rng
	java -jar $(TRANG) -I rnc -O xsd master/$*.rnc  derived/$*.xsd

build-virtual-schema : derived/virtual-graph-schema.rng derived/virtual-graph-schema.xsd


validate-virtual/% :  test/virtual/%.xml $(JING) master/virtual-graph-schema.rnc derived/virtual-graph-schema.xsd
	java -jar $(JING) -c master/virtual-graph-schema.rnc test/virtual/$*.xml
	java -jar $(JING) derived/virtual-graph-schema.xsd test/virtual/$*.xml
	$(PYTHON) tools/print_graph_properties.py < test/virtual/$*.xml

output/%.svg output/%.dot : test/%.xml tools/render_graph_as_dot.py graph_library validate-%
	mkdir -p $(dir output/$*)
	$(PYTHON) tools/render_graph_as_dot.py < test/$*.xml > output/$*.dot
	neato -Tsvg output/$*.dot > output/$*.svg


VIRTUAL_ALL_TESTS := $(patsubst test/virtual/%.xml,%,$(wildcard test/virtual/*.xml))

tt :
	echo $(VIRTUAL_ALL_TESTS)

validate-virtual : $(foreach t,$(VIRTUAL_ALL_TESTS),validate-virtual/$(t) output/virtual/$(t).svg)

clean :
	-find . -iname '*~' -exec rm {} ';'  # Get rid of emacs temporaries
	-rm output/virtual/*.dot output/virtual/*.svg

