load bats_helpers

function setup_file {
    local WD=$(create_bats_file_wd)

    for i in $(seq 3 6) ; do
        python3 apps/clock_tree/create_clock_tree_instance_stream.py $i > ${WD}/ct_${i}.v3.xml
        python3 apps/ising_spin/create_ising_spin_instance_stream.py $i > ${WD}/is_${i}.v3.xml
        python3 apps/gals_heat/create_gals_heat_instance.py $i > ${WD}/gh_${i}.v3.xml
    done
    for i in ${WD}/*.v3.xml ; do
        bin/convert_graph_to_v4 $i ${i%.v3.xml}.v4.xml
    done
}

@test "check bin/topologically_diff_graph_instances exists" {
    [ -x bin/topologically_diff_graph_instances ]
}

@test "Check different v3 graphs are topologically different/same using diff" {
    local WD=$(get_bats_file_wd)

    for i in ${WD}/*.v3.xml ; do
        for j in ${WD}/*.v3.xml ; do
            run bin/topologically_diff_graph_instances $i $j
        
            if [[ "$i" == "$j" ]] ; then
                [ "$status" -eq 0 ]
            else
                [ "$status" -ne 0 ]
            fi
        done
    done
}

@test "Check different v4 graphs are topologically different/same using diff" {
    local WD=$(get_bats_file_wd)

    for i in ${WD}/*.v4.xml ; do
        for j in ${WD}/*.v4.xml ; do
            run bin/topologically_diff_graph_instances $i $j
        
            if [[ "$i" == "$j" ]] ; then
                [ "$status" -eq 0 ]
            else
                [ "$status" -ne 0 ]
            fi
        done
    done
}

@test "Check v3 graphs are topologically different/same as v4 using diff" {
    local WD=$(get_bats_file_wd)
    local bi
    local bj

    for i in ${WD}/*.v3.xml ; do
        bi="${i%.v3.xml}"
        for j in ${WD}/*.v4.xml ; do
            bj="${j%.v4.xml}"
            
            run bin/topologically_diff_graph_instances $i $j
            if [[ "$bi" == "$bj" ]] ; then
                if [[ "$status" -ne 0 ]] ; then
                    >&3 echo "$i vs $j"
                    >&3 echo "$output" 
                    exit 1
                fi
                [ "$status" -eq 0 ]
            else
                [ "$status" -ne 0 ]
            fi
        done
    done
}
