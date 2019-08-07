load ../bats_helpers

@test "Compile and run POEMS with manual gals heat with default args" {
    (cd tools/poems && make -B run_gals_heat)
    (cd tools/poems && ./run_gals_heat)
}

@test "Check compile_poems_sim rejects invalid options" {
    run tools/poems/compile_poems_sim.sh --blurble blibble path-to-xml
    [[ $status -ne 0 ]]
}

@test "Check compile_poems_sim rejects multiple paths" {
    run tools/poems/compile_poems_sim.sh apps/ising_spin/ising_spin_8x8.xml apps/ising_spin/ising_spin_8x8.xml 
    [[ $status -ne 0 ]]
}

@test "Check compile_poems_sim errors on invalid xml path" {
    run tools/poems/compile_poems_sim.sh i-dont-exist.xml 
    [[ $status -ne 0 ]]
}



@test "Compile and test standard clock tree for POEMS" {
    WD=$(make_test_wd)
    apps/clock_tree/create_clock_tree_instance.py 6 6 > $WD/wibble.xml
    tools/poems/compile_poems_sim.sh $WD/wibble.xml -o $WD/wibble.sim
    [[ -x $WD/wibble.sim ]]
    $WD/wibble.sim $WD/wibble.xml
}

@test "Compile and test standard gals heat for POEMS" {
    WD=$(make_test_wd)
    apps/gals_heat/create_gals_heat_instance.py 128 128  > $WD/wibble.xml
    tools/poems/compile_poems_sim.sh $WD/wibble.xml -o $WD/wibble.sim
    [[ -x $WD/wibble.sim ]]
    $WD/wibble.sim $WD/wibble.xml
}

@test "Compile and test standard amg for POEMS" {
    WD=$(make_test_wd)
    apps/amg/make_poisson_graph_instance.py 128 128  > $WD/wibble.xml
    tools/poems/compile_poems_sim.sh $WD/wibble.xml -o $WD/wibble.sim
    [[ -x $WD/wibble.sim ]]
    $WD/wibble.sim $WD/wibble.xml
}

@test "Compile and test clocked izhikevich for POEMS" {
    WD=$(make_test_wd)
    apps/clocked_izhikevich/create_sparse_instance.py 8000 2000 20 100  > $WD/wibble.xml
    tools/poems/compile_poems_sim.sh $WD/wibble.xml -o $WD/wibble.sim
    [[ -x $WD/wibble.sim ]]
    $WD/wibble.sim $WD/wibble.xml
}

