load bats_helpers

setup() {
    make_target bin/queue_sim ising_spin_provider
}

# Note: this info is based on the specific pre-generated XML. It will need changing if the XML changes.
LAST_DEV="n_4_3"

@test "bin/queue_sim exists" {
    [ -x bin/queue_sim ]
}

@test "simulate ising_spin using queue_sim" {
    run bin/queue_sim apps/ising_spin/ising_spin_8x8.xml 
    [[ $status -eq 0 ]]
}

@test "simulate ising_spin using queue_sim on 1 thread" {
    run bin/queue_sim --threads 1 apps/ising_spin/ising_spin_8x8.xml 
    [[ $status -eq 0 ]]
}

@test "simulate ising_spin using queue_sim on 4 thread" {
    run bin/queue_sim --threads 4 apps/ising_spin/ising_spin_8x8.xml 
    [[ $status -eq 0 ]]
}

@test "simulate ising_spin using queue_sim and capture event log" {
    local GS=$(get_graph_schema_dir)
    local WD=$(make_test_wd)
    run bin/queue_sim --log-events $WD/event.log apps/ising_spin/ising_spin_8x8.xml
    cat $WD/event.log | grep '</GraphLog>'
    # Note: this line is based on the specific pre-generated XML. It will need changing if the XML changes.
    cat $WD/event.log | grep -E "\<RecvEvent sendEventId=\"[^\"]+\" pin=\"in\" dev=\"${LAST_DEV}\""
    (cd $WD && ${GS}/tools/render_event_log_as_dot.py event.log)
    # Don't render the event log to image as it is massive
}