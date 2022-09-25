# This provides an easy way of wrapping the older `make XXX_tests` style
# into bats. Over time it should be removed.
load "$( dirname "$BATS_TEST_FILENAME" )/../../tools/bats_helpers"

@test "testing application $(basename "$BATS_TEST_DIRNAME") (backwards compatibility wrapper for bats)" {
    run_make_target_as_test $(basename "$BATS_TEST_DIRNAME")_tests
}
