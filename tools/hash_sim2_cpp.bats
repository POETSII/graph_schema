load bats_helpers

setup() {
    make_target bin/hash_sim2 ising_spin_provider
}

@test "bin/hash_sim2 exists" {
    [ -x bin/hash_sim2 ]
}

# TODO: Actually work out how to test this
