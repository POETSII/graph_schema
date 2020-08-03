#!/usr/bin/env bats

#function version_gt() { test "$(printf '%s\n' "$@" | sort -V | head -n 1)" != "$1"; }
#version_gt "${BATS_VERSION}" 1.2.1 || { >&3 echo "Bats version too low." ; exit 1 }

load ../../../tools/bats_helpers

function setup_file
{
    >&3 echo "SNN setup_file begin."
    >&3 echo "# Cleaning SNN"
    ( cd $BATS_TEST_DIRNAME && make clean )
    >&3 echo "# Building SNN"
    ( cd $BATS_TEST_DIRNAME && make RELEASE_WITH_ASSERTS=-O3 -j all_generators all_programs all_tests )
    >&3 echo "SNN setup_file done."
}

@test "test_source_sink_inmem." {
    ( cd $BATS_TEST_DIRNAME && bin/test_source_sink_inmem )
}

@test "run_generate_izhikevich_sparse_and_check." {
    WD=$(make_test_wd)
    ( cd $BATS_TEST_DIRNAME &&
        bin/generate_izhikevich_sparse  |
        tee >(gzip - > ${WD}/net.txt.gz ) |
         bin/test_sink_snn > /dev/null
    )
}

@test "run_generate_izhikevich_sparse_with_params_and_check." {
    WD=$(make_test_wd)
    ( cd $BATS_TEST_DIRNAME &&
        bin/generate_izhikevich_sparse 80 20 10 0.1 100  |
        tee >(gzip - > ${WD}/net.txt.gz ) |
         bin/test_sink_snn > /dev/null
    )
}

@test "create_izhikevich_instance_and_simulate." {
    WD=$(make_test_wd)
    (cd ${BATS_TEST_DIRNAME} &&
        bin/generate_izhikevich_sparse  |
            tee >(gzip - > ${WD}/net.txt.gz ) |
            bin/create_graph_instance_v2 > ${WD}/net.xml.gz
    )
    >&3 echo "pwd=$(pwd)"
    POETS_PROVIDER_PATH=${BATS_TEST_DIRNAME}/providers $(get_graph_schema_dir)/bin/epoch_sim --max-contiguous-idle-steps 1000000 ${WD}/net.xml.gz --external PROVIDER > ${WD}/out.txt
}

@test "run_generate_CUBA_sparse_and_check." {
    WD=$(make_test_wd)
    ( cd $BATS_TEST_DIRNAME &&
        bin/generate_CUBA  |
        tee >(gzip - > ${WD}/net.txt.gz ) |
         bin/test_sink_snn > /dev/null
    )
}

@test "run_generate_CUBA_sparse_tiny_and_check." {
    WD=$(make_test_wd)
    ( cd $BATS_TEST_DIRNAME &&
        bin/generate_CUBA 80 100  |
        tee >(gzip - > ${WD}/net.txt.gz ) |
         bin/test_sink_snn > /dev/null
    )
}

@test "run_generate_CUBA_sparse_large_and_check." {
    skip "TODO : too slow without memory optimisations"
    WD=$(make_test_wd)
    ( cd $BATS_TEST_DIRNAME &&
        bin/generate_CUBA 6000 1000  |
        tee >(gzip - > ${WD}/net.txt.gz ) |
         bin/test_sink_snn > /dev/null
    )
}

@test "create_small_CUBA_instance_and_simulate." {
    >&3 echo "BATS_TEST_DIRNAME=${BATS_TEST_DIRNAME}"
    WD=$(make_test_wd)
    (cd ${BATS_TEST_DIRNAME} && {
            ${BATS_TEST_DIRNAME}/bin/generate_CUBA 1000  |
            tee >(gzip - > ${WD}/net.txt.gz ) |
            ${BATS_TEST_DIRNAME}/bin/create_graph_instance_v2 | gzip - > ${WD}/net.xml.gz
        }
    )
    >&3 echo "pwd=$(pwd)"
    echo "POETS_PROVIDER_PATH=${BATS_TEST_DIRNAME}/providers $(get_graph_schema_dir)/bin/epoch_sim --max-contiguous-idle-steps 1000000 ${WD}/net.xml.gz --external PROVIDER > ${WD}/out.txt"
    POETS_PROVIDER_PATH=${BATS_TEST_DIRNAME}/providers $(get_graph_schema_dir)/bin/epoch_sim --max-contiguous-idle-steps 1000000 ${WD}/net.xml.gz --external PROVIDER > ${WD}/out.txt
}


@test "create_iz_and_compare_epoch_vs_ref." {
    skip "TODO"
    WD=$(make_test_wd)
    GS=$(get_graph_schema_dir)

    (cd $BATS_TEST_DIRNAME &&
        tests/create_iz_and_compare_epoch_vs_ref.sh "${WD}" "${GS}"
    )
}