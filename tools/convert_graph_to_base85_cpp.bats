load bats_helpers


@test "BinConvert base85 exists and is executable" {
    [ -x bin/convert_graph_to_v4 ]
}

@test "BinConvert v3 ising spin to base85" {
    run_no_stderr bin/convert_graph_to_base85 apps/ising_spin/ising_spin_8x8.xml
    [ $status -eq 0 ]
    echo $output | grep 'cn_0_0@e_b_e_c_e_d_e_e_e_f_e_g_e_h_gn_1_0'
    echo $output | grep 'hpd_b_gq_c_gc_d_gBbe_hfp_b_gs_c_ge_d_gDbe_hlB_b_gu_c_gg'
    echo $output | grep '</Graphs>'
}

@test "BinConvert compressed v3 ising spin to base85" {
    run_no_stderr bin/convert_graph_to_base85 apps/ising_spin/ising_spin_8x8.xml.gz
    [ $status -eq 0 ]
    echo $output | grep 'cn_0_0@e_b_e_c_e_d_e_e_e_f_e_g_e_h_gn_1_0'
    echo $output | grep 'hpd_b_gq_c_gc_d_gBbe_hfp_b_gs_c_ge_d_gDbe_hlB_b_gu_c_gg'
    echo $output | grep '</Graphs>'
}

@test "BinConvert compressed v3 ising spin to compressed base85" {
    WD=$(make_test_wd)
    bin/convert_graph_to_base85 apps/ising_spin/ising_spin_8x8.xml.gz $WD/graph.xml.gz
    [ -f $WD/graph.xml.gz ]
    gunzip $WD/graph.xml.gz
    [ -f $WD/graph.xml ]
    grep 'cn_0_0@e_b_e_c_e_d_e_e_e_f_e_g_e_h_gn_1_0' $WD/graph.xml
    grep 'hpd_b_gq_c_gc_d_gBbe_hfp_b_gs_c_ge_d_gDbe_hlB_b_gu_c_ggx' $WD/graph.xml
    grep '</Graphs>' $WD/graph.xml
}

@test "BinRound-tripping ising spin from v3 to base85 then back to v3 and then simulating" {
    find_PIP0020_DIR
    WD=$(make_test_wd)
    bin/convert_graph_to_base85 apps/ising_spin/ising_spin_8x8.xml $WD/graph.base85.xml
    bin/convert_graph_to_v3 $WD/graph.base85.xml  $WD/graph.v3.xml
    bin/epoch_sim $WD/graph.v3.xml --log-level 2
}


@test "BinRound-tripping just a graph type from v3 to base85 then back to v3 and the compiling as provider" {
    find_PIP0020_DIR
    WD=$(make_test_wd)
    GS=$(get_graph_schema_dir)
    bin/convert_graph_to_base85 apps/ising_spin/ising_spin_graph_type.xml $WD/ising_spin_graph_type.base85.xml
    bin/convert_graph_to_v3 $WD/ising_spin_graph_type.base85.xml > $WD/ising_spin_graph_type.v3.xml
    (cd $WD && ${GS}/tools/compile_graph_as_provider.sh ising_spin_graph_type.v3.xml) 
}