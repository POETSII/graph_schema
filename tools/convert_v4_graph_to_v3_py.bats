load bats_helpers

function setup() {
    make bin/epoch_sim ising_spin_fix_provider
}

@test "Convert v4 to v3 exists and is executable" {
    [ -x tools/convert_v4_graph_to_v3.py ]
}

@test "Convert v4 ising spin fix from PIP0020 to v3" {
    find_PIP0020_DIR
    run_no_stderr tools/convert_v4_graph_to_v3.py  $PIP0020_DIR/xml/ic/apps/ising_spin_fix_16_2_v4.xml
    [ $status -eq 0 ]
    echo $output | grep '<EdgeI path="n_7_7:in-n_6_7:out">'
    echo $output | grep '</Graphs>'
}


@test "converting all the apps in PIP0020 from v4 to v3" {
    find_PIP0020_DIR
    WD=$(make_test_wd)
    for f in $PIP0020_DIR/xml/ic/apps/*.xml ; do
        tools/convert_v4_graph_to_v3.py  $f > $WD/$(basename $f .xml).xml
    done
}

@test "Convert v4 ising spin fix from PIP0020 to v3, compile as provider, and simulate" {
    find_PIP0020_DIR
    WD=$(make_test_wd)
    tools/convert_v4_graph_to_v3.py  $PIP0020_DIR/xml/ic/apps/ising_spin_fix_16_2_v4.xml > $WD/test.xml
    (cd $WD && ../../tools/compile_graph_as_provider.sh test.xml) 
    POETS_PROVIDER_PATH=$WD bin/epoch_sim --log-level 1 $WD/test.xml
}

@test "Convert v4 betweeness_centrality_16_16_20_20_v4 from PIP0020 to v3, compile as provider, and simulate" {
    find_PIP0020_DIR
    WD=$(make_test_wd)
    tools/convert_v4_graph_to_v3.py  $PIP0020_DIR/xml/ic/apps/betweeness_centrality_16_16_20_20_v4.xml > $WD/test.xml
    (cd $WD && ../../tools/compile_graph_as_provider.sh test.xml) 
    POETS_PROVIDER_PATH=$WD bin/epoch_sim --log-level 1 $WD/test.xml
}
