load bats_helpers

setup() {
    make bin/hash_sim2 gals_heat_protocol_only_provider
}

@test "bin/hash_sim2 exists" {
    [ -x bin/hash_sim2 ]
}

@test "exhaustive sim of wide but shallow graph" {
    WD=$(make_test_wd)
    apps/gals_heat_protocol_only/create_gals_heat_protocol_only_instance.py 3 1 > $WD/gals_heat_po_ok.xml
    run bin/hash_sim2 $WD/gals_heat_po_ok.xml
    echo "$output" | grep 'Beginning depth 27, state size=1, explored=78731'
    echo "$output" | grep 'Completed exhaustive simulation of all states and transitions.'
    [[ $status -eq 0 ]]
}

@test "exhaustive sim of narrow but deep graph" {
    WD=$(make_test_wd)
    apps/gals_heat_protocol_only/create_gals_heat_protocol_only_instance.py 2 10 > $WD/gals_heat_po_ok.xml
    run bin/hash_sim2 $WD/gals_heat_po_ok.xml
    echo "$output" | grep 'Beginning depth 120, state size=1, explored=19487'
    echo "$output" | grep 'Completed exhaustive simulation of all states and transitions.'
    [[ $status -eq 0 ]]
}

@test "exhaustive sim of graph with obscure failure case not found by normal sim" {
    WD=$(make_test_wd)
    apps/gals_heat_protocol_only/create_gals_heat_protocol_only_instance.py 2 10 0 1 > $WD/gals_heat_mt.xml
    run bin/hash_sim2 $WD/gals_heat_mt.xml
    echo "$output" | grep 'onReceive Assertion'
    [[ $status -ne 0 ]]
}

