SHELL=/usr/bin/env bash

.PRECIOUS : %.xml.gz %.elf %.build.csv %.execute.csv

GS_ROOT=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../..

##########################################

VARIANTS = cluster random furthest_order nearest_order random_order



%.simulate.out : %.xml.gz
	POETS_PROVIDER_PATH=$(GS_ROOT)/providers $(GS_ROOT)/bin/epoch_sim --log-level 0 --stats-delta 1000 $< 2>&1 | tee $@

%.no-assert.xml.gz : %.xml.gz
	gzip -d -c $< | sed -e 's|\s*assert[(].*| // removed assert for orchestrator|g' | gzip -9 > $@



###########################################


clock_tree_deep.xml.gz :
	$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance.py 6 2 100 | gzip > $@

FILES += clock_tree_deep.xml.gz

clock_tree_broad.xml.gz :
	$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance.py 3 7 1000 | gzip > $@

FILES += clock_tree_broad.xml.gz

########################################################

storm_global_64.xml.gz :
	$(GS_ROOT)/apps/storm/create_storm_instance.py 64 16 64 | gzip > $@

FILES += storm_global_64.xml.gz

storm_local_64.xml.gz :
	$(GS_ROOT)/apps/storm/create_storm_instance.py 64 16 32 | gzip > $@

FILES += storm_local_64.xml.gz

#####################################################

clocked_izhikevich_fix_sparse_10k.xml.gz :
	$(GS_ROOT)/apps/clocked_izhikevich_fix/create_clocked_izhikevich_fix_instance.py 8000 2000 20 1000 32 32 | gzip > $@

#FILES += clocked_izhikevich_fix_sparse_10k.xml.gz

clocked_izhikevich_fix_dense_1k.xml.gz :
	$(GS_ROOT)/apps/clocked_izhikevich_fix/create_clocked_izhikevich_fix_instance.py 800 200 500 1000 1000 1000 | gzip > $@

#FILES += clocked_izhikevich_fix_dense_1k.xml.gz

####################################################

gals_heat_fix_regular_1k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 32 64000 65535 | gzip > $@

#FILES += gals_heat_fix_regular_1k.xml.gz

gals_heat_fix_regular_4k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 64 16000 16383 | gzip > $@

#FILES += gals_heat_fix_regular_4k.xml.gz

gals_heat_fix_regular_16k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 128 4000 4095 | gzip > $@

#FILES += gals_heat_fix_regular_16k.xml.gz

gals_heat_fix_regular_64k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix/create_gals_heat_fix_instance.py 256 500 511 | gzip > $@

#FILES += gals_heat_fix_regular_64k.xml.gz

##################################


gals_heat_fix_noedge_regular_1k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 32 64000 1048575 | gzip > $@

#FILES += gals_heat_fix_noedge_regular_1k.xml.gz

gals_heat_fix_noedge_regular_4k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 64 16000 16383 | gzip > $@

#FILES += gals_heat_fix_noedge_regular_4k.xml.gz

gals_heat_fix_noedge_regular_16k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 128 4000 4095 | gzip > $@

#FILES += gals_heat_fix_noedge_regular_16k.xml.gz

gals_heat_fix_noedge_regular_64k.xml.gz :
	 $(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance.py 256 500 511 | gzip > $@

#FILES += gals_heat_fix_noedge_regular_64k.xml.gz


########################

gals_heat_fix_irregular_12k.xml.gz : 
	(cd $(GS_ROOT)/apps/gals_heat/create_mesh/ && octave octave_svg_to_geo.m)
	(cd $(GS_ROOT)/apps/gals_heat/create_mesh/ && octave octave_write_csv_delauney.m poets.svg_8) > poets.8.csv
	$(GS_ROOT)/apps/gals_heat/create_mesh/csv_mesh_to_graph_fix.py poets.8.csv 6000 | gzip > $@
	rm poets.8.csv

#FILES += gals_heat_fix_irregular_12k.xml.gz

gals_heat_fix_irregular_32k.xml.gz : 
	(cd $(GS_ROOT)/apps/gals_heat/create_mesh/ && octave octave_svg_to_geo.m)
	(cd $(GS_ROOT)/apps/gals_heat/create_mesh/ && octave octave_write_csv_delauney.m poets.svg_16) > poets.16.csv
	$(GS_ROOT)/apps/gals_heat/create_mesh/csv_mesh_to_graph_fix.py  poets.16.csv 3000 | gzip > $@
	rm poets.16.csv

#FILES += gals_heat_fix_irregular_32k.xml.gz


############################

# exec: 73 sec
apsp_sparse_64.xml.gz :
	$(GS_ROOT)/apps/apsp/create_apsp_instance.py 64 8 | gzip > $@

FILES += apsp_sparse_64.xml.gz

# exec: 100 sec
apsp_dense_64.xml.gz :
	$(GS_ROOT)/apps/apsp/create_apsp_instance.py 64 32 | gzip > $@

FILES += apsp_dense_64.xml.gz

###################################

# 8 sec
ising_spin_fix_4k.xml.gz :
	$(GS_ROOT)/apps/ising_spin_fix/create_ising_spin_fix_instance.py 64 1 4000 | gzip > $@

#FILES += ising_spin_fix_4k.xml.gz

# 12 sec
ising_spin_fix_16k.xml.gz :
	$(GS_ROOT)/apps/ising_spin_fix/create_ising_spin_fix_instance.py 128 1 1000 | gzip > $@

#FILES += ising_spin_fix_16k.xml.gz

# 12 sec
ising_spin_fix_64k.xml.gz :
	$(GS_ROOT)/apps/ising_spin_fix/create_ising_spin_fix_instance.py 256 1 250 | gzip > $@

#FILES += ising_spin_fix_64k.xml.gz

################################################

all_xml : $(FILES)

all_xml_no_assert :$(subst .xml.gz,.no-assert.xml.gz,$(FILES))

all_elf : $(subst .xml.gz,.elf,$(FILES))

all_exec : $(subst .xml.gz,.execute.csv,$(FILES))

all_simulate : $(subst .xml.gz,.simulate.out,$(FILES))

all.csv :
	-rm $@
	for x in *.build.csv ; do \
		BN="$(basename $$x .build.csv)"; \
		while read y ; do \
			( echo "$$BN, $$y" >> $@ ) ; \
		done < $$x ; \
	done
	for x in *.execute.csv ; do \
		BN="$(basename $$x .execute.csv)"; \
		while read y ; do \
			( echo "$$BN, $$y" >> $@ ) ; \
		done < $$x ; \
	done

all_variants_elf : $(foreach v,$(VARIANTS),$(subst .xml.gz,.$(v).elf,gals_heat_fix_noedge_regular_1k.xml.gz))

all_variants_exec : $(foreach v,$(VARIANTS),$(subst .xml.gz,.$(v).execute.csv,gals_heat_fix_noedge_regular_1k.xml.gz))

.DELETE_ON_ERROR : 
