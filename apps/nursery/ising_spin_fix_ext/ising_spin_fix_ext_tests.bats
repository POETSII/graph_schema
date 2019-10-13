load ../../../tools/bats_helpers

function setup()
{
    set -o pipefail
}

@test ising_spin_fix_ext_create_default {
    WD=$(make_test_wd)
    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py > ${WD}/test.xml
}

@test ising_spin_fix_ext_create_8x8 {
    WD=$(make_test_wd)
    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py 8 > ${WD}/test.xml
}


@test ising_spin_fix_ext_run_32x32_csv {
    WD=$(make_test_wd)

    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py 32 > ${WD}/test.xml
    # run for 10 slices
    bin/epoch_sim ${WD}/test.xml --log-level 0 --stats-delta 1000000 --external PROVIDER \
             10 | gzip -9 > $WD/out.csv.gz

    # outputs can come out in unknown order, so need to sort
    diff <(gzip -d -c $WD/out.csv.gz | sort) <(gzip -c -d apps/nursery/ising_spin_fix_ext/ref-out-32x32-steps20.csv.gz | sort)
}

@test ising_spin_fix_ext_run_32x32_img_static {
    WD=$(make_test_wd)

    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py 32 > ${WD}/test.xml
    # run for 10 slices
    bin/epoch_sim ${WD}/test.xml --log-level 0 --stats-delta 1000000 \
            --external INPROC:providers/ising_spin_fix_ext_img_static_external.so \
             10 | gzip -9 > $WD/out.pgm_pipe.gz

    diff <(gzip -d -c $WD/out.pgm_pipe.gz ) <(gzip -c -d apps/nursery/ising_spin_fix_ext/ref-out-32x32-steps20.pgm_pipe.gz)
}

@test ising_spin_fix_ext_run_32x32_img_dynamic {
    WD=$(make_test_wd)

    apps/nursery/ising_spin_fix_ext/create_ising_spin_fix_ext_instance.py 32 > ${WD}/test.xml
    # run for 50 slices
    bin/epoch_sim ${WD}/test.xml --log-level 0 --stats-delta 1000000 \
            --external INPROC:providers/ising_spin_fix_ext_img_dynamic_external.so \
             50 | gzip -9 > $WD/out.pgm_pipe.gz

    diff <(gzip -d -c $WD/out.pgm_pipe.gz ) <(gzip -c -d apps/nursery/ising_spin_fix_ext/ref-out-32x32-steps20_dynamic.pgm_pipe.gz)
}

