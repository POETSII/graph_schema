define apps_tests_supervisor_template
# $1 base name of test 

$(eval $(call provider_rules_template,$1,apps/tests/supervisors/$1_graph_type.xml))

all_supervisor_test_providers : $1_provider

endef

ALL_SUPERVISOR_TEST_GRAPH_TYPES_XML := $(wildcard apps/tests/supervisors/*_graph_type.xml)
ALL_SUPERVISOR_TEST_GRAPH_TYPES := $(patsubst apps/tests/supervisors/%_graph_type.xml,%,$(ALL_SUPERVISOR_TEST_GRAPH_TYPES_XML))

$(eval $(call provider_rules_template,orchestrator_examples_ring_test,apps/tests/supervisors/instances/orchestrator_examples_ring_test.xml))
$(eval $(call provider_rules_template,orchestrator_examples_test_ping_pong_timer,apps/tests/supervisors/instances/orchestrator_examples_test_ping_pong_timer.xml))

$(foreach x,$(ALL_SUPERVISOR_TEST_GRAPH_TYPES),$(eval $(call apps_tests_supervisor_template,$x)))

define apps_tests_supervisor_instance_template
# $1 graph type base name
# $2 instance suffix
# $3 command to generate to stdout

demos/tests/supervisors/$1_$2.xml : apps/tests/supervisors/$1_graph_type.xml
	mkdir -p demos/tests/supervisors
	$3 > $$@

all_supervisor_test_instances : demos/tests/supervisors/$1_$2.xml

endef

# General constraints on instances are:
# <= 10M messages sent
# <= 100K devices
# <= 100K steps on critical path

# Messages ~= devs * reps
# Critical path ~= devs * reps (everything goes through supervisor)
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_pulse,n1_r1,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_pulse_generator.py 1 1))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_pulse,n10_r10,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_pulse_generator.py 10 10))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_pulse,n100000_r1,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_pulse_generator.py 100000 1))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_pulse,n10000_r10,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_pulse_generator.py 10000 10))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_pulse,n1000_r100,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_pulse_generator.py 1000 100))

$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_reply,n1_r1,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_reply_generator.py 1 1))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_reply,n10_r10,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_reply_generator.py 10 10))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_reply,n100000_r10,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_reply_generator.py 100000 10))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_reply,n10000_r100,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_reply_generator.py 10000 100))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_reply,n1000_r1000,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_reply_generator.py 1000 1000))

$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n1_r1,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 1 1))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n10_r10,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 10 10))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n10000_r2,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 10000 2))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n1000_r10,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 1000 10))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n100_r1000,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 100 1000))

$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n10_r10_flood,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 10 10 10))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n1000_r10_flood,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 1000 10 1000))
$(eval $(call apps_tests_supervisor_instance_template,test_supervisor_Ndev_to_sup_relay_bcast,n10000_r2_flood,python3 apps/tests/supervisors/test_supervisor_Ndev_to_sup_relay_bcast_generator.py 10000 2 10))
