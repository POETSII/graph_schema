load ../../tools/bats_helpers

function setup {
    make bin/epoch_sim bin/graph_sim amg_provider
}

function test_graph {
    run bin/epoch_sim $1
    [[ $status -eq 0 ]]
    echo $output | grep "exit_node:finished : _HANDLER_EXIT_SUCCESS_9be65737_"
    
    run bin/graph_sim --strategy FIFO $1
    [[ $status -eq 0 ]]
    echo $output | grep "exit_node : _HANDLER_EXIT_SUCCESS_9be65737_"

    run bin/graph_sim --strategy LIFO $1
    [[ $status -eq 0 ]]
    echo $output | grep "exit_node : _HANDLER_EXIT_SUCCESS_9be65737_"

    run bin/graph_sim --strategy Random $1
    [[ $status -eq 0 ]]
    echo $output | grep "exit_node : _HANDLER_EXIT_SUCCESS_9be65737_"
}

@test amg_check_run_graph_2x2 {
    WD=$(make_test_wd)
    apps/amg/make_poisson_graph_instance.py 2 2 > $WD/graph.xml
    
    test_graph $WD/graph.xml
}

@test amg_check_run_graph_3x4 {
    WD=$(make_test_wd)
    apps/amg/make_poisson_graph_instance.py 3 4 > $WD/graph.xml
    
    test_graph $WD/graph.xml
}

@test amg_check_run_graph_5x5 {
    WD=$(make_test_wd)
    apps/amg/make_poisson_graph_instance.py 5 5 > $WD/graph.xml
    
    test_graph $WD/graph.xml
}

@test amg_check_run_graph_8x8 {
    WD=$(make_test_wd)
    apps/amg/make_poisson_graph_instance.py 8 8 > $WD/graph.xml
    
    test_graph $WD/graph.xml
}

#TODO: There is some kind of bug at this size
#@test amg_check_run_graph_16x16 {
#    WD=$(make_test_wd)
#    apps/amg/make_poisson_graph_instance.py 16 16 > $WD/graph.xml
#    
#    test_graph $WD/graph.xml
#}


@test amg_check_run_graph_32x32 {
    WD=$(make_test_wd)
    apps/amg/make_poisson_graph_instance.py 32 32 > $WD/graph.xml
    
    test_graph $WD/graph.xml
}
