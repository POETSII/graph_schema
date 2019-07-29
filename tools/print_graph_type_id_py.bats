load bats_helpers

@test "check tools/print_graph_type_id.py exists" {
    [ -f tools/print_graph_type_id.py ]
}

@test "use tools/print_graph_type_id.py on uncompressed data" {
    run_no_stderr tools/print_graph_type_id.py apps/ising_spin/ising_spin_8x8.xml
    [ "$output" == "ising_spin" ]
}

@test "use tools/print_graph_type_id.py on compressed data" {
    run_no_stderr tools/print_graph_type_id.py apps/ising_spin/ising_spin_8x8.xml.gz
    [ "$output" == "ising_spin" ]
}

@test "use tools/print_graph_type_id.py on stdin" {
    run_no_stderr tools/print_graph_type_id.py < apps/ising_spin/ising_spin_8x8.xml
    [ "$output" == "ising_spin" ]
}
