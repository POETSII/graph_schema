load bats_helpers

@test "Compile graph as provider exists and is executable" {
    [ -x tools/compile_graph_as_provider.sh ]
}

@test "Compile ising spin to provider" {
    WD=$(make_test_wd)
    cp apps/ising_spin/ising_spin_graph_type.xml $WD
    (cd $WD && ../../tools/compile_graph_as_provider.sh ising_spin_graph_type.xml)
    [ -f $WD/ising_spin.graph.so ]
}

@test "Compile ising spin instance to provider and simulate using it" {
    WD=$(make_test_wd)
    cp apps/ising_spin/ising_spin_8x8.xml $WD
    run "( POETS_PROVIDER_PATH=$WD  cd $WD && ../../bin/epoch_sim ising_spin_8x8.xml )"
    [ $status -ne 0 ]
    (cd $WD && ../../tools/compile_graph_as_provider.sh ising_spin_8x8.xml)
    ( POETS_PROVIDER_PATH=$WD  cd $WD && ../../bin/epoch_sim ising_spin_8x8.xml)
}
