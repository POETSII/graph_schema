SHELL=/usr/bin/env bash

.PRECIOUS : %.xml.gz %.elf %.build.csv %.execute.csv

GS_ROOT=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/..
TS_ROOT=$(abspath $(GS_ROOT)/../toy_softswitch)


%.elf %.build.csv : %.xml.gz
	$(TS_ROOT)/tools/build_tinsel_elf.sh <( gunzip -c $*.xml.gz ) -o $*.elf --measure=$*.build.csv --release

%.execute.csv : %.elf
	$(TS_ROOT)/tools/execute_tinsel_elf.sh $*.elf --measure=$*.execute.csv




clock_tree_deep_32k.xml.gz :
	$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance.py 14 2 800 | gzip > $@

FILES += clock_tree_deep_32k.xml.gz

clock_tree_broad_32k.xml.gz :
	$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance.py 3 25 1000 | gzip > $@

FILES += clock_tree_broad_32k.xml.gz

storm_global_2k.xml.gz :
	$(GS_ROOT)/apps/storm/create_storm_instance.py 2048 16 2048 | gzip > $@

FILES += storm_global_2k.xml.gz

storm_local_2k.xml.gz :
	$(GS_ROOT)/apps/storm/create_storm_instance.py 2048 16 32 | gzip > $@

FILES += storm_local_2k.xml.gz

clocked_izhikevich_fix_sparse_10k.xml.gz :
	$(GS_ROOT)/apps/clocked_izhikevich_fix/create_clocked_izhikevich_fix_instance.py 8000 2000 20 1000 32 32 | gzip > $@

FILES += clocked_izhikevich_fix_sparse_10k.xml.gz

clocked_izhikevich_fix_dense_1k.xml.gz :
	$(GS_ROOT)/apps/clocked_izhikevich_fix/create_clocked_izhikevich_fix_instance.py 800 200 500 1000 1000 1000 | gzip > $@

FILES += clocked_izhikevich_fix_dense_1k.xml.gz

####################################################

gals_heat_fix_regular_1k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 32 4000 4095 | gzip > $@

FILES += gals_heat_fix_regular_1k.xml.gz

gals_heat_fix_regular_4k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 64 1000 1023 | gzip > $@

FILES += gals_heat_fix_regular_4k.xml.gz

gals_heat_fix_regular_16k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 128 250 255 | gzip > $@

FILES += gals_heat_fix_regular_16k.xml.gz

gals_heat_fix_regular_64k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 256 250 255 | gzip > $@

FILES += gals_heat_fix_regular_64k.xml.gz

##################################

# 20 sec
gals_heat_fix_noedge_regular_1k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 32 4000 4095 | gzip > $@

FILES += gals_heat_fix_noedge_regular_1k.xml.gz

gals_heat_fix_noedge_regular_4k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 64 1000 1023 | gzip > $@

FILES += gals_heat_fix_noedge_regular_4k.xml.gz

gals_heat_fix_noedge_regular_16k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 128 250 255 | gzip > $@

FILES += gals_heat_fix_noedge_regular_16k.xml.gz

gals_heat_fix_noedge_regular_64k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 256 125 255 | gzip > $@

FILES += gals_heat_fix_noedge_regular_64k.xml.gz


########################

# 40 seconds (?)
gals_heat_irregular_12k.xml.gz : 
	(cd $(GS_ROOT)/apps/gals_heat/create_mesh/ && octave octave_svg_to_geo.m)
	(cd $(GS_ROOT)/apps/gals_heat/create_mesh/ && octave octave_write_csv_delauney.m poets.svg_8) > poets.8.csv
	$(GS_ROOT)/apps/gals_heat/create_mesh/csv_mesh_to_graph_fix.py poets.8.csv | gzip > $@

FILES += gals_heat_irregular_12k.xml.gz


############################

# exec: 73 sec
apsp_sparse_4k.xml.gz :
	$(GS_ROOT)/apps/apsp/create_apsp_instance.py 4096 8 | gzip > $@

FILES += apsp_sparse_4k.xml.gz

# exec: 100 sec
apsp_dense_1k.xml.gz :
	$(GS_ROOT)/apps/apsp/create_apsp_instance.py 1024 256 | gzip > $@

FILES += apsp_dense_1k.xml.gz

###################################

# 8 sec
ising_spin_fix_4k.xml.gz :
	$(GS_ROOT)/apps/ising_spin_fix/create_ising_spin_fix_instance.py 64 1 4000 | gzip > $@

FILES += ising_spin_fix_4k.xml.gz

# 12 sec
ising_spin_fix_16k.xml.gz :
	$(GS_ROOT)/apps/ising_spin_fix/create_ising_spin_fix_instance.py 128 1 1000 | gzip > $@

FILES += ising_spin_fix_16k.xml.gz

# 12 sec
ising_spin_fix_64k.xml.gz :
	$(GS_ROOT)/apps/ising_spin_fix/create_ising_spin_fix_instance.py 256 1 250 | gzip > $@

FILES += ising_spin_fix_64k.xml.gz

################################################

all_xml : $(FILES)

all_elf : $(subst .xml.gz,.elf,$(FILES))

all_exec : $(subst .xml.gz,.execute.csv,$(FILES))

.DELETE_ON_ERROR : 
