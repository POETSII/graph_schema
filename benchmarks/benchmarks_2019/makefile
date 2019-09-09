SHELL=/usr/bin/env bash

.PRECIOUS : %.xml.gz %.elf

GS_ROOT=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../..

PYTHONPATH ?= $(GS_ROOT)/tools
export PYTHONPATH

POETS_PROVIDER_PATH ?= $(GS_ROOT)/providers
export POETS_PROVIDER_PATH

PIP0020_PATH ?= $(GS_ROOT)/../poets_improvement_proposals/proposed/PIP-0020

ifneq ($(wildcard $(PIP0020_PATH)),)
HAVE_PIP0020=1
endif

OUTPUT_DIR ?= output

DISTRIB_DIR ?= distrib

EPOCH_SIM_TIMEOUT ?= 240
GRAPH_SIM_TIMEOUT ?= 240
PIP0020_SIM_TIMEOUT ?= 240
POEMS_SIM_TIMEOUT ?= 240

#################################################

## General stats

$(GS_ROOT)/bin/calculate_graph_static_properties :
	cd $(GS_ROOT) && make bin/calculate_graph_static_properties

%.v3.static_properties.json : %.v3.xml.gz $(GS_ROOT)/bin/calculate_graph_static_properties %.benchmark_properties.json
	$(GS_ROOT)/bin/calculate_graph_static_properties $< $*.benchmark_properties.json > $@ 

$(DISTRIB_DIR)/% : $(OUTPUT_DIR)/%
	mkdir -p $(dir $@)
	cp $< $@

#########################################################
##########################################################



define instance_template
# $1 : class name
# $2 : scale single number
# $3 : generator command, with __S__ to represent scale. Should dump as plain xml to stdout.
# $4 : (optional) dependency(ies) that need to be satisfied before building

$(OUTPUT_DIR)/$1/$1_s$2.benchmark_properties.json :
	mkdir -p $(OUTPUT_DIR)/$1
	> $$@ echo '{ "benchmark_group":"$1", "benchmark_instance_id":"$1_s$2", "benchmark_instance_scale":$2 }'

$(OUTPUT_DIR)/$1/$1_s$2.v3.xml.gz : $4
	mkdir -p $(OUTPUT_DIR)/$1
	GRAPH_SCHEMA_DEFAULT_XML_OUTPUT_VERSION=3 $(subst __S__,$2,$3) | gzip - > $$@

$(OUTPUT_DIR)/$1/$1_s$2.v4.xml.gz : $4
	mkdir -p $1
	GRAPH_SCHEMA_DEFAULT_XML_OUTPUT_VERSION=4 $(subst __S__,$2,$3) | gzip - > $$@

#################################################
## V3 simulation

# This monstrosity is to create a dependency from smallest to largest scales. It
# allows for fast timeout.
$(eval ifeq ($($1_ALL_SCALES),)
_PREV_SCALE_EPOCH_V3_STATUS_$1_$2:=
else
_PREV_SCALE_EPOCH_V3_STATUS_$1_$2:=$(OUTPUT_DIR)/$1/$1_s$(lastword $($1_ALL_SCALES)).v3.epoch_sim.status
endif
)

$(OUTPUT_DIR)/$1/$1_s$2.v3.epoch_sim.status : $(OUTPUT_DIR)/$1/$1_s$2.v3.xml.gz $(_PREV_SCALE_EPOCH_V3_STATUS_$1_$2)
	@if [[ "$(_PREV_SCALE_EPOCH_V3_STATUS_$1_$2)" != "" ]] && grep "timeout" "$(_PREV_SCALE_EPOCH_V3_STATUS_$1_$2)" ; then \
		echo "timeout(transitive)" > $$@ ; \
	else \
		/usr/bin/timeout --foreground $(EPOCH_SIM_TIMEOUT) $(GS_ROOT)/bin/epoch_sim --stats-delta 100 --log-level 1 $$< 2> $(OUTPUT_DIR)/$1/$1_s$2.v3.epoch_sim.stderr > $(OUTPUT_DIR)/$1/$1_s$2.v3.epoch_sim.stdout ; \
		RES=$$$$? ; \
		if [[ $$$$RES -eq 124 ]] ; then \
			echo "timeout" > $$@ ; \
		elif grep _HANDLER_EXIT_FAIL_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v3.epoch_sim.stderr ; then \
			echo "fail" > $$@ ; \
		elif grep _HANDLER_EXIT_SUCCESS_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v3.epoch_sim.stderr ; then \
			echo "success" > $$@ ; \
		elif [[ $$$$RES -ne 0 ]] ; then \
			echo "error" > $$@ ; \
		else \
			echo "unknown" > $$@ ; \
		fi \
	fi

$(eval ifeq ($($1_ALL_SCALES),)
_PREV_SCALE_GRAPH_SIM_V3_STATUS_$1_$2:=
else
_PREV_SCALE_GRAPH_SIM_V3_STATUS_$1_$2:=$(OUTPUT_DIR)/$1/$1_s$(lastword $($1_ALL_SCALES)).v3.graph_sim.status
endif
)

$(OUTPUT_DIR)/$1/$1_s$2.v3.graph_sim.status : $(OUTPUT_DIR)/$1/$1_s$2.v3.xml.gz $(_PREV_SCALE_GRAPH_SIM_V3_STATUS_$1_$2)
	@if [[ "$(_PREV_SCALE_GRAPH_SIM_V3_STATUS_$1_$2)" != "" ]] && grep "timeout" "$(_PREV_SCALE_GRAPH_SIM_V3_STATUS_$1_$2)" ; then \
		echo "timeout(transitive)" > $$@ ; \
	else \
		/usr/bin/timeout --foreground $(GRAPH_SIM_TIMEOUT) $(GS_ROOT)/bin/graph_sim --stats-delta 100 --log-level 1 $$< 2> $(OUTPUT_DIR)/$1/$1_s$2.v3.graph_sim.stderr > $(OUTPUT_DIR)/$1/$1_s$2.v3.graph_sim.stdout ; \
		RES=$$$$? ; \
		if [[ $$$$RES -eq 124 ]] ; then \
			echo "timeout" > $$@ ; \
		elif grep _HANDLER_EXIT_FAIL_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v3.graph_sim.stderr ; then \
			echo "fail" > $$@ ; \
		elif grep _HANDLER_EXIT_SUCCESS_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v3.graph_sim.stderr ; then \
			echo "success" > $$@ ; \
		elif [[ $$$$RES -ne 0 ]] ; then \
			echo "error" > $$@ ; \
		else \
			echo "unknown" > $$@ ; \
		fi \
	fi


##############################################
## V4 simulation


$(eval ifeq ($($1_ALL_SCALES),)
_PREV_SCALE_PIP0020_SIM_STATUS_$1_$2:=
else
_PREV_SCALE_PIP0020_SIM_STATUS_$1_$2:=$(OUTPUT_DIR)/$1/$1_s$(lastword $($1_ALL_SCALES)).v4.pip0020_sim.status
endif
)

ifneq ($(HAVE_PIP0020),)

$(OUTPUT_DIR)/$1/$1_s$2.v4.pip0020_sim.status : $(OUTPUT_DIR)/$1/$1_s$2.v4.xml.gz $(_PREV_SCALE_PIP0020_SIM_STATUS_$1_$2)
	@if [[ "$(_PREV_SCALE_PIP0020_SIM_STATUS_$1_$2)" != "" ]] && grep "timeout" "$(_PREV_SCALE_PIP0020_SIM_STATUS_$1_$2)" ; then \
		>&2 echo "pip0020_sim : Skipping $1_$2 (transitive timeout)" \
		echo "timeout(transitive)" > $$@ ; \
	else \
		>&2 echo "pip0020_sim : Simulating $1_s$2" ; \
		gunzip -c $$< | /usr/bin/timeout $(PIP0020_SIM_TIMEOUT) $(PIP0020_PATH)/tools/pyparser/simulate.sh /dev/stdin --app-log-level 1 2> $(OUTPUT_DIR)/$1/$1_s$2.v4.pip0020_sim.stderr > $(OUTPUT_DIR)/$1/$1_s$2.v4.pip0020_sim.stdout ; \
		RES=$$$$? ; \
		if grep _HANDLER_EXIT_SUCCESS_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.pip0020_sim.stdout ; then \
			STATUS="success"; \
		elif [[ $$$$RES -eq 124 ]] ; then \
			STATUS="timeout"; \
		elif grep _HANDLER_EXIT_FAIL_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.pip0020_sim.stdout ; then \
			STATUS="fail"; \
		elif [[ $$$$RES -ne 0 ]] ; then \
			STATUS="error"; \
		else \
			STATUS="unknown"; \
		fi ; \
		>&2 echo "pip0020_sim : Finished $1_$2 - $$$$STATUS" ; \
		echo $$$$STATUS > $$@ ; \
	fi

endif

$(eval ifeq ($($1_ALL_SCALES),)
_PREV_SCALE_EPOCH_V4_STATUS_$1_$2:=
else
_PREV_SCALE_EPOCH_V4_STATUS_$1_$2:=$(OUTPUT_DIR)/$1/$1_s$(lastword $($1_ALL_SCALES)).v4.epoch_sim.status
endif
)

$(OUTPUT_DIR)/$1/$1_s$2.v4.epoch_sim.status : $(OUTPUT_DIR)/$1/$1_s$2.v4.xml.gz $(_PREV_SCALE_EPOCH_V4_STATUS_$1_$2)
	@if [[ "$(_PREV_SCALE_EPOCH_V4_STATUS_$1_$2)" != "" ]] && grep "timeout" "$(_PREV_SCALE_EPOCH_V4_STATUS_$1_$2)" ; then \
		echo "timeout(transitive)" > $$@ ; \
	else \
		/usr/bin/timeout --foreground $(EPOCH_SIM_TIMEOUT) $(GS_ROOT)/bin/epoch_sim --stats-delta 100 --log-level 1 $$< 2> $(OUTPUT_DIR)/$1/$1_s$2.v4.epoch_sim.stderr > $(OUTPUT_DIR)/$1/$1_s$2.v4.epoch_sim.stdout ; \
		RES=$$$$? ; \
		if [[ $$$$RES -eq 124 ]] ; then \
			echo "timeout" > $$@ ; \
		elif grep _HANDLER_EXIT_FAIL_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.epoch_sim.stderr ; then \
			echo "fail" > $$@ ; \
		elif grep _HANDLER_EXIT_SUCCESS_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.epoch_sim.stderr ; then \
			echo "success" > $$@ ; \
		elif [[ $$$$RES -ne 0 ]] ; then \
			echo "error" > $$@ ; \
		else \
			echo "unknown" > $$@ ; \
		fi \
	fi
	@>&2 echo "Epoch sim v4 $1_$2 status : $$$$(cat $$@)"


$(eval ifeq ($($1_ALL_SCALES),)
_PREV_SCALE_GRAPH_SIM_V4_STATUS_$1_$2:=
else
_PREV_SCALE_GRAPH_SIM_V4_STATUS_$1_$2:=$(OUTPUT_DIR)/$1/$1_s$(lastword $($1_ALL_SCALES)).v4.graph_sim.status
endif
)

$(OUTPUT_DIR)/$1/$1_s$2.v4.graph_sim.status : $(OUTPUT_DIR)/$1/$1_s$2.v4.xml.gz $(_PREV_SCALE_GRAPH_SIM_V4_STATUS_$1_$2)
	@if [[ "$(_PREV_SCALE_GRAPH_SIM_V4_STATUS_$1_$2)" != "" ]] && grep "timeout" "$(_PREV_SCALE_GRAPH_SIM_V4_STATUS_$1_$2)" &> /dev/null ; then \
		echo "timeout(transitive)" > $$@ ; \
	else \
		/usr/bin/timeout --foreground $(GRAPH_SIM_TIMEOUT) $(GS_ROOT)/bin/graph_sim --stats-delta 100 --log-level 1 $$< 2> $(OUTPUT_DIR)/$1/$1_s$2.v4.graph_sim.stderr > $(OUTPUT_DIR)/$1/$1_s$2.v4.graph_sim.stdout ; \
		RES=$$$$? ; \
		if [[ $$$$RES -eq 124 ]] ; then \
			echo "timeout" > $$@ ; \
		elif grep _HANDLER_EXIT_FAIL_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.graph_sim.stderr &> /dev/null ; then \
			echo "fail" > $$@ ; \
		elif grep _HANDLER_EXIT_SUCCESS_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.graph_sim.stderr &> /dev/null ; then \
			echo "success" > $$@ ; \
		elif [[ $$$$RES -ne 0 ]] ; then \
			echo "error" > $$@ ; \
		else \
			echo "unknown" > $$@ ; \
		fi \
	fi


$(eval ifeq ($($1_ALL_SCALES),)
_PREV_SCALE_POEMS_SIM_V4_STATUS_$1_$2:=
else
_PREV_SCALE_POEMS_SIM_V4_STATUS_$1_$2:=$(OUTPUT_DIR)/$1/$1_s$(lastword $($1_ALL_SCALES)).v4.poems_sim.status
endif
)

$(OUTPUT_DIR)/$1/$1_s$2.v4.poems_sim.status : $(OUTPUT_DIR)/$1/$1_s$2.v4.xml.gz $(OUTPUT_DIR)/$1/$1.v4.poems.engine $(_PREV_SCALE_POEMS_SIM_V4_STATUS_$1_$2)
	@if [[ "$(_PREV_SCALE_POEMS_SIM_V4_STATUS_$1_$2)" != "" ]] && grep "timeout" "$(_PREV_SCALE_POEMS_SIM_V4_STATUS_$1_$2)" &> /dev/null ; then \
		echo "timeout(transitive)" > $$@ ; \
	else \
		/usr/bin/timeout --foreground $(POEMS_SIM_TIMEOUT) $(OUTPUT_DIR)/$1/$1.v4.poems.engine --threads 1 $$< 2> $(OUTPUT_DIR)/$1/$1_s$2.v4.poems_sim.stderr > $(OUTPUT_DIR)/$1/$1_s$2.v4.poems_sim.stdout ; \
		RES=$$$$? ; \
		if [[ $$$$RES -eq 124 ]] ; then \
			echo "timeout" > $$@ ; \
		elif grep _HANDLER_EXIT_FAIL_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.poems_sim.stderr &> /dev/null ; then \
			echo "fail" > $$@ ; \
		elif grep _HANDLER_EXIT_SUCCESS_9be65737_ $(OUTPUT_DIR)/$1/$1_s$2.v4.poems_sim.stderr &> /dev/null ; then \
			echo "success" > $$@ ; \
		elif [[ $$$$RES -ne 0 ]] ; then \
			echo "error" > $$@ ; \
		else \
			echo "unknown" > $$@ ; \
		fi \
	fi
	@>&2 echo "POEMS sim v4 $1_$2 status : $$$$(cat $$@)"



$1_ALL_V3_XML_GZ += $(OUTPUT_DIR)/$1/$1_s$2.v3.xml.gz
$1_ALL_V4_XML_GZ += $(OUTPUT_DIR)/$1/$1_s$2.v4.xml.gz

$1_ALL_V3_STATIC_PROPERTIES += $(OUTPUT_DIR)/$1/$1_s$2.v3.static_properties.json
$1_ALL_V3_EPOCH_SIM += $(OUTPUT_DIR)/$1/$1_s$2.v3.epoch_sim.status
$1_ALL_V3_GRAPH_SIM += $(OUTPUT_DIR)/$1/$1_s$2.v3.graph_sim.status
$1_ALL_V4_PIP0020_SIM += $(OUTPUT_DIR)/$1/$1_s$2.v4.pip0020_sim.status
$1_ALL_V4_EPOCH_SIM += $(OUTPUT_DIR)/$1/$1_s$2.v4.epoch_sim.status
$1_ALL_V4_GRAPH_SIM += $(OUTPUT_DIR)/$1/$1_s$2.v4.graph_sim.status
$1_ALL_V4_POEMS_SIM += $(OUTPUT_DIR)/$1/$1_s$2.v4.poems_sim.status

$1_ALL_V3_DISTRIB += $(DISTRIB_DIR)/$1/$1_s$2.v3.static_properties.json $(DISTRIB_DIR)/$1/$1_s$2.v3.xml.gz $(DISTRIB_DIR)/$1/$1_s$2.v3.epoch_sim.status $(DISTRIB_DIR)/$1/$1_s$2.v3.graph_sim.status
$1_ALL_V4_DISTRIB += $(DISTRIB_DIR)/$1/$1_s$2.v4.static_properties.json $(DISTRIB_DIR)/$1/$1_s$2.v4.xml.gz $(DISTRIB_DIR)/$1/$1_s$2.v4.pip0020_sim.status $(DISTRIB_DIR)/$1/$1_s$2.v4.epoch_sim.status $(DISTRIB_DIR)/$1/$1_s$2.v4.graph_sim.status

# TODO: Does this need to be eval'd? Can't remember why it is like this now.
$(eval $1_ALL_SCALES := $($1_ALL_SCALES) $2)

endef

########################################################################################

define class_template
# $1 : Name of the class
# $2 : One line description of the class and how it varies with scale
# It is assumed that all instances are already defined, i.e. this should be called last

$1_TITLE = $2

README_TITLE_LIST += - [$1]($1) : $2\n

#############################
## Compile poems simulator

$(eval $1_SMALLEST_SCALE := $(firstword $($1_ALL_SCALES)))

$(OUTPUT_DIR)/$1/$1.v3.poems.engine : $(OUTPUT_DIR)/$1/$1_s$($1_SMALLEST_SCALE).v3.xml.gz
	$(GS_ROOT)/tools/poems/compile_poems_sim.sh	 -o $$@ $$<

$(OUTPUT_DIR)/$1/$1.v4.poems.engine : $(OUTPUT_DIR)/$1/$1_s$($1_SMALLEST_SCALE).v4.xml.gz
	$(GS_ROOT)/tools/poems/compile_poems_sim.sh	 -o $$@ $$<

all_v3_poems_engines : $(OUTPUT_DIR)/$1/$1.v3.poems.engine
all_v4_poems_engines : $(OUTPUT_DIR)/$1/$1.v4.poems.engine

##############################
## general targets

$1_all_v3_xml_gz : $($1_ALL_V3_XML_GZ)
$1_all_v4_xml_gz : $($1_ALL_V4_XML_GZ)

$1_all_v3_static_properties : $($1_ALL_V3_STATIC_PROPERTIES)
$1_all_v3_graph_sim : $($1_ALL_V3_GRAPH_SIM)
$1_all_v3_epoch_sim : $($1_ALL_V3_EPOCH_SIM)
$1_all_v4_graph_sim : $($1_ALL_V4_GRAPH_SIM)
$1_all_v4_epoch_sim : $($1_ALL_V4_EPOCH_SIM)
$1_all_v4_pip0020_sim : $($1_ALL_V4_PIP0020_SIM)
$1_all_v4_poems_sim : $($1_ALL_V4_POEMS_SIM)

ALL_CLASSES += $1

all_v3_xml_gz : $1_all_v3_xml_gz
all_v4_xml_gz : $1_all_v4_xml_gz

all_v3_static_properties : $1_all_v3_static_properties
all_v3_epoch_sim : $1_all_v3_epoch_sim
all_v3_graph_sim : $1_all_v3_graph_sim
all_v4_epoch_sim : $1_all_v4_epoch_sim
all_v4_graph_sim : $1_all_v4_graph_sim
all_v4_pip0020_sim : $1_all_v4_pip0020_sim
all_v4_poems_sim : $1_all_v4_poems_sim

all_v3_distrib : $($1_ALL_V3_DISTRIB)
all_v4_distrib : $($1_ALL_V4_DISTRIB)

endef

# End of infrastructure setup
##################################################################################
##################################################################################

##################################################################################
##################################################################################
# Begin actual classes

CLASS=clock_tree_deep
SCALES=2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
COMMAND=$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance_stream.py __S__ 2 1000
TITLE=Deterministic clock tree with binary fanout and depth of S

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))


###########################################

CLASS=clock_tree_broad
SCALES=2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 32
COMMAND=$(GS_ROOT)/apps/clock_tree/create_clock_tree_instance_stream.py 4 __S__ 1000
TITLE=Deterministic clock tree with fixed depth of 4 and branching factor of S

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))

###########################################

CLASS=gals_heat_fix_noedge_square
SCALES=4 6 8 11 16 23 32 45 64 91 128 181 256 362 512 724 1024
COMMAND=$(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance_stream.py __S__ 1000 4095
TITLE=Heat equation with uniform global weights over a 2D domain of size S^2

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))

###########################################

CLASS=gals_heat_fix_noedge_cube
SCALES=3 4 5 6 7 9 10 12 14 16 18 22 25 29 34 40 46 54 63 74 86 100 # 117 136 158 185 215
COMMAND=$(GS_ROOT)/apps/gals_heat_fix_noedge/create_gals_heat_fix_noedge_instance_cube.py __S__ 1000 4095
TITLE=Heat equation with uniform global weights over a 3D domain of size S^3

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))

###########################################

CLASS=betweeness_centrality_dense
SCALES= 16 23 32 45 64 91 128 181 256 362 512 724 1024 1448 2048 2896 4096 # 5793 8192 11585 16384 23170 32768
COMMAND=$(GS_ROOT)/apps/betweeness_centrality/create_betweeness_centrality_instance_stream.py __S__ $$$$((__S__ / 2)) 1023 256
TITLE=Estimate betweeness_centrality using random walks over a dense graph with S degree-(S/2) nodes (so O(S^2) edges)

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))

###########################################

CLASS=betweeness_centrality_sparse
SCALES= 16 32 64 128 256 512 1024 2048 4096 16384 32768 65536 131072 262144 524288 1048576
COMMAND=$(GS_ROOT)/apps/betweeness_centrality/create_betweeness_centrality_instance_stream.py __S__ 8 1023 16
TITLE=Estimate betweeness_centrality using random walks over a sparse graph with S degree-8 nodes (so O(S) edges)

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))


###########################################

CLASS=ising_spin_fix_2d
SCALES= 4 6 8 11 16 23 32 45 64 91 128 181 256 362 512 724 1024
COMMAND=$(GS_ROOT)/apps/ising_spin_fix/create_ising_spin_fix_instance_stream.py __S__ 1 1000
TITLE=Discrete-event ising-spin model over wraparound 2D domain of size S^2


$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))


###########################################

CLASS=amg_poisson
SCALES= 4 6 8 11 16 23 32 33 34 36 38 40 42 45 64 91 128 181 256 362 512 # 724 1024
COMMAND=$(GS_ROOT)/apps/amg/make_poisson_graph_instance.py __S__ __S__
TITLE=Algebraic Multi-Grid (AMG) solver for A x = b over a 2D poisson system of size S^2

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))


###########################################

CLASS=relaxation_heat
SCALES= 4 6 8 11 16 23 32 33 34 36 38 40 42 45 64 91 128 181 256 362 512 # 724 1024
COMMAND=$(GS_ROOT)/apps/relaxation_heat/create_relaxation_heat_instance.py __S__ __S__
TITLE=Non-deterministic steady-state solver for heat equation over a 2D system of size S^2

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))

###########################################

$(OUTPUT_DIR)/airfoil/all_airfoil_meshes.xz.tar :
	mkdir -p $(OUTPUT_DIR)/airfoil
	wget -O $@ https://imperialcollegelondon.box.com/shared/static/6gfzp6ddwo576i5owi7bwi93gwjdovsh.tar > $@

$(OUTPUT_DIR)/airfoil/airfoil_%.hdf5 : $(OUTPUT_DIR)/airfoil/all_airfoil_meshes.xz.tar
	(cd $(OUTPUT_DIR)/airfoil && tar -xf all_airfoil_meshes.xz.tar  meshes/airfoil_$*.hdf5.xz)
	(cd $(OUTPUT_DIR)/airfoil && xz -d meshes/airfoil_$*.hdf5.xz  -c > airfoil_$*.hdf5 )

define airfoil_instance_template
$(eval $(call instance_template,airfoil,$1,$(GS_ROOT)/apps/nursery/airfoil/create_airfoil_instance.py $(OUTPUT_DIR)/airfoil/$2,$(OUTPUT_DIR)/airfoil/$2))
endef

CLASS=airfoil
TITLE=Finite-element solver for compressible flow over an airfoil over an irregular mesh of O(S^2) elements 

$(eval $(call airfoil_instance_template,16,airfoil_1.5625%.hdf5))
$(eval $(call airfoil_instance_template,22,airfoil_2.2097%.hdf5))
$(eval $(call airfoil_instance_template,31,airfoil_3.1250%.hdf5))
$(eval $(call airfoil_instance_template,62,airfoil_6.2500%.hdf5))
$(eval $(call airfoil_instance_template,88,airfoil_8.8388%.hdf5))
$(eval $(call airfoil_instance_template,125,airfoil_12.5000%.hdf5))
$(eval $(call airfoil_instance_template,177,airfoil_17.6777%.hdf5))
$(eval $(call airfoil_instance_template,250,airfoil_25.0000%.hdf5))
$(eval $(call airfoil_instance_template,354,airfoil_35.3553%.hdf5))
#$(eval $(call airfoil_instance_template,500,airfoil_50.0000%.hdf5))
#$(eval $(call airfoil_instance_template,707,airfoil_70.7107%.hdf5))
#$(eval $(call airfoil_instance_template,1000,airfoil_100.0000%.hdf5))
$(eval $(call class_template,$(CLASS),$(TITLE)))

###########################################

CLASS=apsp_vec_barrier_dense
SCALES= 16 23 32 45 64 91 128 181 256 362 512 724 1024 1448 2048 2896 4096 # 5793 8192 11585 16384 23170 32768
COMMAND=$(GS_ROOT)/apps/nursery/apsp_vec_barrier/create_apsp_instance.py __S__ $$$$((__S__ / 4))
TITLE=All-pairs Shortest-Path using vectorisation and hardware idle detection over a dense graph with S nodes of degree S/4 (so O(S^2) edges)

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))


###########################################

CLASS=apsp_vec_barrier_sparse
# TODO: convert this to a streaming generator for larger sizes
SCALES= 16 32 64 128 256 512 1024 2048 4096 16384 32768 65536 131072 262144 # 524288 1048576
COMMAND=$(GS_ROOT)/apps/nursery/apsp_vec_barrier/create_apsp_instance.py __S__ 16
TITLE=All-pairs Shortest-Path using vectorisation and hardware idle detection over a sparse graph with S nodes of degree 16 (so O(S) edges)

$(foreach SCALE,$(SCALES),$(eval $(call instance_template,$(CLASS),$(SCALE),$(COMMAND))))
$(eval $(call class_template,$(CLASS),$(TITLE)))


#######################################################
#######################################################

$(DISTRIB_DIR)/readme.md : readme.md
	mkdir -p $(DISTRIB_DIR)
	cp $< $@
	echo -e '$(README_TITLE_LIST)' >> $@


.DELETE_ON_ERROR : 
