SHELL=/usr/bin/env bash

.PRECIOUS : %.xml.gz %.elf

GS_ROOT=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../..

PYTHONPATH ?= $(GS_ROOT)/tools
export PYTHONPATH

###########################################

define def_clock_tree_deep

clock_tree_deep/clock_tree_deep_s$1.xml.gz :
	mkdir -p clock_tree_deep
	$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance_stream.py $1 2 100 | gzip > $$@

clock_tree_deep_all : clock_tree_deep/clock_tree_deep_s$1.xml.gz

endef

$(foreach i,2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19, $(eval $(call def_clock_tree_deep,$(i))))

###########################################

define def_clock_tree_broad

clock_tree_broad/clock_tree_broad_s$1.xml.gz :
	mkdir -p clock_tree_broad
	$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance_stream.py 4 $1 100 | gzip > $$@

clock_tree_broad_all : clock_tree_broad/clock_tree_broad_s$1.xml.gz

endef

$(foreach i,2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 32, $(eval $(call def_clock_tree_broad,$(i))))

###########################################

define def_gals_heat_fix_noedge_square

gals_heat_fix_noedge_square/gals_heat_fix_noedge_square_s$1.xml.gz :
	mkdir -p gals_heat_fix_noedge_square
	$(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance_stream.py $1 1000 4095 | gzip > $$@

gals_heat_fix_noedge_square_all : gals_heat_fix_noedge_square/gals_heat_fix_noedge_square_s$1.xml.gz

endef

$(foreach i,4 6 8 11 16 23 32 45 64 91 128 181 256 362 512 724 1024, $(eval $(call def_gals_heat_fix_noedge_square,$(i))))

###########################################

define def_gals_heat_fix_noedge_cube

gals_heat_fix_noedge_cube/gals_heat_fix_noedge_cube_s$1.xml.gz :
	mkdir -p gals_heat_fix_noedge_cube
	$(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance_cube.py $1 1000 4095 | gzip > $$@

gals_heat_fix_noedge_cube_all : gals_heat_fix_noedge_cube/gals_heat_fix_noedge_cube_s$1.xml.gz

endef

$(foreach i,2 3 4 5 6 7 9 10 12 14 16 18 22 25 29 34 40 46 54 63 74 86 100 117 136 158 185 215, $(eval $(call def_gals_heat_fix_noedge_cube,$(i))))

###########################################


.DELETE_ON_ERROR : 
