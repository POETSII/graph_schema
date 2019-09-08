load bats_helpers

@test "check bin/structurally_compare_graph_types exists" {
    [ -x bin/structurally_compare_graph_types ]
}

@test "Check standard graph types structurally equal to themselves" {
    for i in apps/*/*_graph_type.xml ; do
        bin/structurally_compare_graph_types $i $i
    done
}

@test "Check standard graph types structurally equal to providers" {
    TO_CHECK="clock_tree ising_spin ising_spin_fix clocked_izhikevich clocked_izhikevich_fix gals_izhikevich\
           gals_heat gals_heat_fix gals_heat_fix_noedge storm amg apsp betweeness_centrality relaxation_heat"
    for i in ${TO_CHECK} ; do
        bin/structurally_compare_graph_types apps/${i}/${i}_graph_type.xml
    done
}

@test "Check standard graph types are _not_ structurally equal to each other" {
    for i in apps/*/*_graph_type.xml ; do
        for j in apps/*/*_graph_type.xml ; do
            if [[ "$i" != "$j" ]] ; then
                run bin/structurally_compare_graph_types $i $j
                [[ $status -ne 0 ]]
            fi
        done
    done
}

@test "Convert standard v3 graph types to v4 and check they are still structurally equal to themselves" {
    [[ -x tools/convert_v4_graph_to_v3.py ]]
    WD=$(make_test_wd)
    for i in apps/*/*_graph_type.xml ; do
        j=$(basename $i)
        tools/convert_v3_graph_to_v4.py $i $WD/$j
        bin/structurally_compare_graph_types $i $WD/$j
    done
}

@test "Round-trip standard v3 graph types to v4 and back to v3 and check they are still structurally equal to themselves" {
    [[ -x tools/convert_v4_graph_to_v3.py ]]
    WD=$(make_test_wd)
    for i in apps/*/*_graph_type.xml ; do
        j=$(basename $i)
        tools/convert_v3_graph_to_v4.py $i $WD/$j
        tools/convert_v4_graph_to_v3.py $WD/$j > $WD/$j.v4.xml
        bin/structurally_compare_graph_types $i $WD/$j.v4.xml
    done
}