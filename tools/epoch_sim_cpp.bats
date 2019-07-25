load bats_helpers

setup() {
    make_target bin/epoch_sim ising_spin_provider
}

@test "bin/epoch_sim exists" {
    [ -x bin/epoch_sim ]
}

@test "simulate ising_spin graph for 4 steps" {
    run bin/epoch_sim --max-steps 5 apps/ising_spin/ising_spin_8x8.xml 
    echo $output | grep "Epoch 4"
    echo $output | grep "Done"
}

@test "simulate ising_spin graph till the end" {
    WD=$(make_test_wd)
    run bin/epoch_sim --max-steps 5 --snapshots 1 $WD/out.snap apps/ising_spin/ising_spin_8x8.xml
    cat $WD/out.snap | grep "<GraphSnapshot"
    cat $WD/out.snap | grep "</GraphSnapshot>"
    cat $WD/out.snap | grep 'id="n_7_7"'
    cat $WD/out.snap | grep '</Graph>'
}


