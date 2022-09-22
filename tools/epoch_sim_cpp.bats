load bats_helpers

function test_in_epoch_sim (){
    # $1 = path to xml
    run bin/epoch_sim --max-steps 10000 --log-level 0 $1
    echo $output | grep _HANDLER_EXIT_SUCCESS_9be65737_
}

setup() {
    make_target bin/epoch_sim ising_spin_provider
}

@test "bin/epoch_sim exists" {
    [ -x bin/epoch_sim ]
}

@test "simulate ising_spin using epoch_sim for 4 steps" {
    run bin/epoch_sim --max-steps 5 --stats-delta 1 apps/ising_spin/ising_spin_8x8.xml 
    echo $output | grep "Epoch 4"
    echo $output | grep "Done"
}

@test "simulate ising_spin using epoch_sim till the end and take snapshots" {
    WD=$(make_test_wd)
    run bin/epoch_sim --max-steps 5 --snapshots 1 $WD/out.snap apps/ising_spin/ising_spin_8x8.xml
    cat $WD/out.snap | grep "<GraphSnapshot"
    cat $WD/out.snap | grep "</GraphSnapshot>"
    cat $WD/out.snap | grep 'id="n_7_7"'
    cat $WD/out.snap | grep '</Graph>'
}

@test "epoch_sim test_supervisor" {
    for i in demos/tests/supervisors/*.xml ; do
        >&3 echo "# $i"
        run bin/epoch_sim --max-steps 10000 --log-level 0 $i
        echo $output | grep _HANDLER_EXIT_SUCCESS_9be65737_
    done
}
