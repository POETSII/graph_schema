load bats_helpers

function test_in_epoch_sim (){
    # $1 = path to xml
    run bin/epoch_sim --max-steps 10000 --log-level 0 $1
    echo $output | grep _HANDLER_EXIT_SUCCESS_9be65737_
}

setup() {
    make_target \
        bin/epoch_sim \
        ising_spin_provider \
         all_supervisor_test_providers \
         all_supervisor_test_instances
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

@test "epoch_sim test_supervisor graph_schema tests" {
    for i in demos/tests/supervisors/*.xml ; do
        >&3 echo "# $i"
        run bin/epoch_sim --max-steps 10000 --log-level 0 $i
        #>&3 echo "# $output"
        echo $output | grep "application_exit(0)"
    done
}

@test "epoch_sim --event-log test_supervisor graph_schema tests" {
    WD=$(make_test_wd)
    for i in demos/tests/supervisors/*.xml ; do
        >&3 echo "# $i"
        # We write to dev null for speed/space, and because test doesn't look at it yet
        run bin/epoch_sim --log-events /dev/null  --max-steps 10000 --log-level 0 $i
        #>&3 echo "# $output"
        echo $output | grep "application_exit(0)"
    done
}

@test "epoch_sim test_supervisor Orchestrator_examples tests" {
    for i in apps/tests/supervisors/instances/*.xml ; do
        >&3 echo "# $i"
        >&3 echo "# bin/epoch_sim --max-steps 10000 --log-level 0 $i"
        run bin/epoch_sim --max-steps 10000 --stats-delta 10000 --log-level 0 $i
        echo $output | grep "application_exit(0)"
    done
}
