@test "make bin/print_graph_properties" {
    make bin/print_graph_properties
}

@test "use bin/print_graph_properties on uncompressed graph" {
    run bin/print_graph_properties apps/ising_spin/ising_spin_8x8.xml
    # Check for a few expected lines
    echo $output | grep "onGraphType(ising_spin)"
    echo $output | grep "onEdgeInstance(n_7_7.in <- n_6_7.out)"
    echo $output | grep "Done"
}

@test "use bin/print_graph_properties on compressed graph" {
    run bin/print_graph_properties apps/ising_spin/ising_spin_8x8.xml.gz
    # Check for a few expected lines
    echo $output | grep "onGraphType(ising_spin)"
    echo $output | grep "onEdgeInstance(n_7_7.in <- n_6_7.out)"
    echo $output | grep "Done"
}

@test "use bin/print_graph_properties from stdin" {
    run bin/print_graph_properties < apps/ising_spin/ising_spin_8x8.xml
    # Check for a few expected lines
    echo $output | grep "onGraphType(ising_spin)"
    echo $output | grep "onEdgeInstance(n_7_7.in <- n_6_7.out)"
    echo $output | grep "Done"
}