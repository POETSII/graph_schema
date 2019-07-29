@test "bin/print_graph_properties exists" {
    [ -f tools/print_graph_properties.py ]
}

@test "use tools/print_graph_properties.py on uncompressed graph" {
    run tools/print_graph_properties.py apps/ising_spin/ising_spin_8x8.xml
    # Check for a few expected lines
    echo $output | grep "edge instance count = 256"
    echo $output | grep "device instance count = 64"
}

@test "use tools/print_graph_properties.py on compressed graph" {
    run tools/print_graph_properties.py apps/ising_spin/ising_spin_8x8.xml.gz
    # Check for a few expected lines
    echo $output | grep "edge instance count = 256"
    echo $output | grep "device instance count = 64"
}

@test "use tools/print_graph_properties.py on stdin" {
    run tools/print_graph_properties.py < apps/ising_spin/ising_spin_8x8.xml
    # Check for a few expected lines
    echo $output | grep "edge instance count = 256"
    echo $output | grep "device instance count = 64"
}
