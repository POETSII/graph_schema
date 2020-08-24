load bats_helpers

@test "Compile graph as provider exists and is executable" {
    [ -x tools/compile_graph_as_provider.sh ]
}

@test "Compile ising spin to provider with specified working dir" {
    WD=$(make_test_wd)
    GS=$(get_graph_schema_dir)
    cp apps/ising_spin/ising_spin_graph_type.xml $WD
    (cd $WD && ${GS}/tools/compile_graph_as_provider.sh --working-dir working ising_spin_graph_type.xml)
    [ -f $WD/ising_spin.graph.so ]
    [ -d $WD/working ]
    [ -f $WD/working/ising_spin.graph.devices.cpp ]
    [ -f $WD/working/ising_spin.graph.hpp ]
}


@test "Compile ising spin to provider" {
    WD=$(make_test_wd)
    GS=$(get_graph_schema_dir)
    cp apps/ising_spin/ising_spin_graph_type.xml $WD
    (cd $WD && ${GS}/tools/compile_graph_as_provider.sh ising_spin_graph_type.xml)
    [ -f $WD/ising_spin.graph.so ]
}

@test "Compile ising spin to provider with specified output location" {
    WD=$(make_test_wd)
    GS=$(get_graph_schema_dir)
    mkdir -p ${WD}/out
    cp apps/ising_spin/ising_spin_graph_type.xml $WD
    (cd $WD && ${GS}/tools/compile_graph_as_provider.sh -o out/wibble.so ising_spin_graph_type.xml)
    [ -f $WD/out/wibble.so ]
}

@test "Compile ising spin instance to provider and simulate using it" {
    WD=$(make_test_wd)
    GS=$(get_graph_schema_dir)
    cp apps/ising_spin/ising_spin_8x8.xml $WD
    run "( POETS_PROVIDER_PATH=$WD  cd $WD && ../../bin/epoch_sim ising_spin_8x8.xml )"
    [ $status -ne 0 ]
    (cd $WD && ${GS}/tools/compile_graph_as_provider.sh ising_spin_8x8.xml)
    ( POETS_PROVIDER_PATH=$WD  cd $WD && ${GS}/bin/epoch_sim ising_spin_8x8.xml)
}
