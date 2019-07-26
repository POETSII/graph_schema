load bats_helpers

# Note: this info is based on the specific pre-generated XML. It will need changing if the XML changes.
LAST_DEV="n_4_3"

setup() {
    make_target bin/graph_sim ising_spin_provider
}

@test "bin/graph_sim exists" {
    [ -x bin/graph_sim ]
}

@test "simulate ising_spin using graph_sim, default strategy" {
    run bin/graph_sim apps/ising_spin/ising_spin_8x8.xml 
    # Note: this line is based on the specific pre-generated XML. It will need changing if the XML changes.
    echo "$output" | grep "${LAST_DEV} : _HANDLER_EXIT_SUCCESS_9be65737_"
}

# It's difficult to prove that these are doing the chosen order with
# creating really tuned apps. For now just check that options work.

@test "simulate ising_spin using graph_sim, random strategy" {
    run bin/graph_sim --strategy Random apps/ising_spin/ising_spin_8x8.xml 
    echo "$output" | grep "${LAST_DEV} : _HANDLER_EXIT_SUCCESS_9be65737_"
}

@test "simulate ising_spin using graph_sim, FIFO strategy" {
    run bin/graph_sim --strategy FIFO apps/ising_spin/ising_spin_8x8.xml 
    # Note: this line is based on the specific pre-generated XML. It will need changing if the XML changes.
    echo "$output" | grep "${LAST_DEV} : _HANDLER_EXIT_SUCCESS_9be65737_"
}

@test "simulate ising_spin using graph_sim, LIFO strategy" {
    run bin/graph_sim --strategy LIFO apps/ising_spin/ising_spin_8x8.xml 
    # Note: this line is based on the specific pre-generated XML. It will need changing if the XML changes.
    echo "$output" | grep "${LAST_DEV} : _HANDLER_EXIT_SUCCESS_9be65737_"
}

@test "simulate ising_spin using graph_sim, changing probability" {
    run bin/graph_sim --prob-send 0.1 apps/ising_spin/ising_spin_8x8.xml 
    # Note: this line is based on the specific pre-generated XML. It will need changing if the XML changes.
    echo "$output" | grep "${LAST_DEV} : _HANDLER_EXIT_SUCCESS_9be65737_"
}

@test "simulate ising_spin using graph_sim and capture event log" {
    WD=$(make_test_wd)
    run bin/graph_sim --log-events $WD/event.log apps/ising_spin/ising_spin_8x8.xml
    cat $WD/event.log | grep '</GraphLog>'
    # Note: this line is based on the specific pre-generated XML. It will need changing if the XML changes.
    cat $WD/event.log | grep -E "\<RecvEvent sendEventId=\"[^\"]+\" pin=\"in\" dev=\"${LAST_DEV}\""
    (cd $WD && ../../tools/render_event_log_as_dot.py event.log)
    # Don't render the event log as it is massive
}

@test "simulate ising_spin using graph_sim and capture truncated event log" {
    WD=$(make_test_wd)
    run bin/graph_sim --max-events 10 --log-events $WD/event.log apps/ising_spin/ising_spin_8x8.xml
    OO=$(cat $WD/event.log | grep -E -c '<SendEvent|<RecvEvent')
    [ "${OO}" == "11" ]
    (cd $WD && ../../tools/render_event_log_as_dot.py event.log)
    (cd $WD && dot -Tsvg -O graph.dot)    
}

