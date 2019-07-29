# This provides an easy way of wrapping the older `make XXX_tests` style
# into bats. Over time it should be removed.

@test "testing application $(basename "$BATS_TEST_DIRNAME") (backwards compatibility wrapper for bats)" {
    make $(basename "$BATS_TEST_DIRNAME")_tests
}
