# These are helper functions used when running bats tests.

# This might be augmented in the future to work properly
# if we do parallel builds
make_target () {
    make $1
}

# Sets up the providers path in case we change directory
if [[ "$POETS_PROVIDER_PATH" == "" ]] ; then
    export POETS_PROVIDER_PATH="$(pwd)/providers"
fi

# Sets up python path in case it is not set, or we move directory
if [[ "$PYTHONPATH" == "" ]] ; then
    export PYTHONPATH="$(pwd)/tools"
fi


# This is modified from the standard bats run in order to strip stderr
run_no_stderr() {
  local origFlags="$-"
  set +eET
  local origIFS="$IFS"
  output="$("$@" 2> /dev/null)"
  status="$?"
  IFS=$'\n' lines=($output)
  IFS="$origIFS"
  set "-$origFlags"
}

# Create a working based on the file-name of the test and the test number
make_test_wd() {
    # inspired by https://github.com/ztombol/bats-file/blob/master/src/temp.bash

    local wd="testing/${BATS_TEST_FILENAME##*/}-${BATS_TEST_NUMBER}"
    
    mkdir -p $wd
    ( [ "$wd" != "" ] && rm $wd/* )

    echo $wd
}
