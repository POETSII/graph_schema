load ../bats_helpers

# Warning: this test is flaky. Sometimes it crashes under bats, but
# it cannot be reproduced in mnay tens of back-to-back runs outside bats.
@test "Compile and run POEMS with manual gals heat with default args" {
    (cd tools/poems && make -B run_gals_heat)
    #(cd tools/poems && gdb -batch --eval-command run --eval-command bt --eval-command quit ./run_gals_heat)
    (cd tools/poems && ./run_gals_heat)
}

@test "Check compile_poems_sim accepts --release" {
    run tools/poems/compile_poems_sim.sh --release apps/ising_spin/ising_spin_8x8.xml
    [[ $status -eq 0 ]]
}

@test "Check compile_poems_sim accepts --debug" {
    run tools/poems/compile_poems_sim.sh --debug apps/ising_spin/ising_spin_8x8.xml
    [[ $status -eq 0 ]]
}

@test "Check compile_poems_sim accepts --release-with-asserts" {
    run tools/poems/compile_poems_sim.sh --release-with-asserts apps/ising_spin/ising_spin_8x8.xml
    [[ $status -eq 0 ]]
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

#TODO: Waiting till bug is fixed in AMG. poems is quite good at triggering it.
#@test "Compile and test standard amg for POEMS" {
#    WD=$(make_test_wd)
#    apps/amg/make_poisson_graph_instance.py 128 128  > $WD/wibble.xml
#    tools/poems/compile_poems_sim.sh $WD/wibble.xml -o $WD/wibble.sim
#    [[ -x $WD/wibble.sim ]]
#    $WD/wibble.sim $WD/wibble.xml
#}

@test "Compile and test clocked izhikevich for POEMS" {
    WD=$(make_test_wd)
    apps/clocked_izhikevich/create_sparse_instance.py 8000 2000 20 100  > $WD/wibble.xml
    tools/poems/compile_poems_sim.sh $WD/wibble.xml -o $WD/wibble.sim
    [[ -x $WD/wibble.sim ]]
    $WD/wibble.sim $WD/wibble.xml
}

#TODO: Unclear if poems makes this really slow (quite possible), or if there is a bug in poems
#@test "Compile and test standard storm for POEMS" {
#    WD=$(make_test_wd)
#    # Warning: this app can create huge numbers of in-flight non-local messages,
#    # which is completely legal, but means it takes a long time to settle.
#    apps/storm/create_storm_instance.py 200 10 100 > $WD/wibble.xml
#    tools/poems/compile_poems_sim.sh  $WD/wibble.xml -o $WD/wibble.sim
#    [[ -x $WD/wibble.sim ]]
#    $WD/wibble.sim --cluster-size 32 $WD/wibble.xml
#}
