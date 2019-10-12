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

    ( [ -d "$wd" ] && rm -rf "$wd" )    
    mkdir -p $wd

    echo $wd
}


# Looks for the xml v4 spec directory, and stores it in PIP0020_DIR.
# If the directory is not found, then PIP0020_DIR=="" and returns non-zero status
function find_PIP0020_DIR () {
    if [[ ! -d "$POETS_PIP_REPO" ]] ; then
        if [[ -d ../poets_improvement_proposals ]] ; then
            POETS_PIP_REPO="$(pwd)/../poets_improvement_proposals"
        fi
    fi

    PIP0020_DIR=""
    if [[ -d "$POETS_PIP_REPO/proposed/PIP-0020" ]] ; then
        PIP0020_DIR="$POETS_PIP_REPO/proposed/PIP-0020"
    elif [[ -d "$POETS_PIP_REPO/accepted/PIP-0020" ]] ; then
        PIP0020_DIR="$POETS_PIP_REPO/accepted/PIP-0020"
    fi

    if [[ ! -d "$PIP0020_DIR" ]] ; then
        >&3 echo "Couldn't find PIP0020 (xml v4 spec) directory ."
        PIP0020_DIR=""
        return 1
    else
        return 0
    fi
}
