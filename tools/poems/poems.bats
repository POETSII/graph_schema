load ../bats_helpers

$test "Compile and run POEMS with default args" {
    (cd tools/poems && make -B run_gals_heat)
    (cd tools/poems && ./run_gals_heat)
}

