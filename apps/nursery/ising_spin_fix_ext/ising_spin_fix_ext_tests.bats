load ../../../tools/bats_helpers

@test ising_spin_fix_ext_create_default {
    WD=$(make_test_wd)
    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py > ${WD}/test.xml
}

@test ising_spin_fix_ext_create_8x8 {
    WD=$(make_test_wd)
    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py 8 > ${WD}/test.xml
}


@test ising_spin_fix_ext_run_8x8_csv {
    WD=$(make_test_wd)

    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py 8 > ${WD}/test.xml
    # run for 3 slices
    bin/epoch_sim ${WD}/test.xml --external PROVIDER 3 > $WD/out.csv

    # Should be completely determinstic
    diff $WD/out.csv apps/nursery/ising_spin_fix_ext/ref-out-8x8-steps3.csv
}

@test ising_spin_fix_ext_run_8x8_img_static {
    WD=$(make_test_wd)

    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py 8 > ${WD}/test.xml
    # run for 6 slices
    bin/epoch_sim ${WD}/test.xml --external INPROC:providers/ising_spin_fix_ext_img_static_external.so \
             6 > $WD/out.pgm_pipe

    # Should be completely determinstic
    diff $WD/out.pgm_pipe apps/nursery/ising_spin_fix_ext/ref-out-8x8-steps6.pgm_pipe
}
