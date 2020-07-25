load bats_helpers.bash

@test "check tools/render_graph_as_dot.py exists" {
    [ -f tools/render_graph_as_dot.py ]
}

@test "use tools/render_graph_as_dot.py on basic graph and convert to svg" {
    local GS=$(get_graph_schema_dir)
    local WD=$(make_test_wd)
    ( cd $WD && ${GS}/tools/render_graph_as_dot.py ${GS}/apps/ising_spin/ising_spin_8x8.xml )
    [ -f $WD/graph.dot ]
    ( cd $WD && neato graph.dot -Tsvg -O)
    [ -s $WD/graph.dot.svg ]
}

@test "use tools/render_graph_as_dot.py on snaphots" {
    local GS=$(get_graph_schema_dir)
    local WD=$(make_test_wd)
    ( cd $WD && ${GS}/bin/epoch_sim ${GS}/apps/ising_spin/ising_spin_8x8.xml --snapshots 1 graph.snap --max-steps 3 --expect-idle-exit )
    ( cd $WD && ${GS}/tools/render_graph_as_dot.py ${GS}/apps/ising_spin/ising_spin_8x8.xml --snapshots graph.snap)
    [ -f $WD/graph.dot_000000.dot ]
    [ -f $WD/graph.dot_000003.dot ]
}

