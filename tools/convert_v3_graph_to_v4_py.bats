load bats_helpers


@test "Convert v3 to v4 exists and is executable" {
    [ -x tools/convert_v3_graph_to_v4.py ]
}

@test "Convert v3 ising spin to v4" {
    run_no_stderr tools/convert_v3_graph_to_v4.py apps/ising_spin/ising_spin_8x8.xml
    [ $status -eq 0 ]
    echo $output | grep '<EdgeI path="n_7_7:in-n_6_7:out" P="{4}"/>'
    echo $output | grep '</Graphs>'
}

@test "Convert compressed v3 ising spin to v4" {
    run_no_stderr tools/convert_v3_graph_to_v4.py apps/ising_spin/ising_spin_8x8.xml.gz
    [ $status -eq 0 ]
    echo $output | grep '<EdgeI path="n_7_7:in-n_6_7:out" P="{4}"/>'
    echo $output | grep '</Graphs>'
}

@test "Convert compressed v3 ising spin to compressed v4" {
    WD=$(make_test_wd)
    tools/convert_v3_graph_to_v4.py apps/ising_spin/ising_spin_8x8.xml.gz $WD/graph.xml.gz
    [ -f $WD/graph.xml.gz ]
    gunzip $WD/graph.xml.gz
    [ -f $WD/graph.xml ]
    grep '<EdgeI path="n_7_7:in-n_6_7:out" P="{4}"/>' $WD/graph.xml
    grep '</Graphs>' $WD/graph.xml
}

@test "Checking PIP0020 validator rejects v3 file (sanity)" {
    find_PIP0020_DIR
    run $PIP0020_DIR/tools/pyparser/validator.py < apps/ising_spin/ising_spin_8x8.xml
    [ $status -ne 0 ]
}

@test "Convert ising spin instance and checking using PIP0020 v4 native validator" {
    find_PIP0020_DIR
    WD=$(make_test_wd)
    tools/convert_v3_graph_to_v4.py apps/ising_spin/ising_spin_8x8.xml.gz $WD/graph.xml
    $PIP0020_DIR/tools/pyparser/validator.py < $WD/graph.xml
}

@test "Convert ising spin graph type and checking using PIP0020 v4 native validator" {
    find_PIP0020_DIR
    WD=$(make_test_wd)
    tools/convert_v3_graph_to_v4.py apps/ising_spin/ising_spin_graph_type.xml $WD/graph.xml
    $PIP0020_DIR/tools/pyparser/validator.py < $WD/graph.xml
}

@test "Round-tripping ising spin from v3 to v4 then back to v3 and then simulating" {
    find_PIP0020_DIR
    [ -x tools/convert_v4_graph_to_v3.py ]
    WD=$(make_test_wd)
    tools/convert_v3_graph_to_v4.py apps/ising_spin/ising_spin_8x8.xml $WD/graph.v4.xml
    tools/convert_v4_graph_to_v3.py $WD/graph.v4.xml > $WD/graph.v3.xml
    bin/epoch_sim $WD/graph.v3.xml --log-level 2
}

