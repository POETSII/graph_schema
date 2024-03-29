# These are helper functions used when running bats tests.

function get_graph_schema_dir {
    local this_path=${BASH_SOURCE[0]}
    local graph_schema_dir="$(realpath $(dirname ${this_path})/..)"
    if [[ ! -d "${graph_schema_dir}" ]] ; then
        exit 1;
    fi
    echo $graph_schema_dir
}

# This should be used from within bats test setups if you want
# to ensure anything is built.
# In principle this allows safe building of targets, as it only
# allows one root instance of make at a time, even as parallel
# tests continue
make_target () {
    local gsd=$(get_graph_schema_dir)
    local lock_file=${gsd}/testing/make_lock
    # Use a timeout of 1 minute. Nothing built within
    # tests should take this long, as either;
    # - We are in a full "make test", which does the full (slow) build up-front;
    # - We are building something small within a test, which should be fast.
    # - The makefile is running a make target as a test-case, which should be taking less than 1 minute even on a laptop
    flock --timeout 60 ${lock_file} make -j4 $*
}

run_make_target_as_test () {
    local gsd=$(get_graph_schema_dir)
    local lock_file=${gsd}/testing/make_lock
    # There should be exactly one target to make
    (cd ${gsd} && flock --timeout 60 ${lock_file} make -j4 $1)
    RES=$?
    return $RES
}

# Sets up the providers path in case we change directory
if [[ "$POETS_PROVIDER_PATH" == "" ]] ; then
    export POETS_PROVIDER_PATH="$(get_graph_schema_dir)/providers"
fi

# Sets up python path in case it is not set, or we move directory
if [[ "$PYTHONPATH" == "" ]] ; then
    export PYTHONPATH="$(get_graph_schema_dir)/tools"
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

function pppp () {
    >&3 echo "PWD=$(pwd)"
}

# Create a working based on the file-name of the test and the test number
function make_test_wd() {
    local wd
    local gsd=$(get_graph_schema_dir)

    if [[ ! -d "${gsd}" ]] ; then
        wd=$(mkdtemp -d)
    else
        # inspired by https://github.com/ztombol/bats-file/blob/master/src/temp.bash
        wd="${gsd}/testing/${BATS_TEST_FILENAME##*/}/${BATS_TEST_NUMBER}"

        ( [ -d "$wd" ] && rm -rf "$wd" )    
        mkdir -p $wd
    fi
    echo $wd
}

# Identify a working directory shared amongst all tests in file
function get_bats_file_wd() {
    local wd
    local gsd=$(get_graph_schema_dir)

    if [[ ! -d "${gsd}" ]] ; then
        >&3 echo "# ERROR: Couldnt find graph_schema_dir, dont know how to make shared dir"
        wd="/tmp/poets-graph_schema-testing/${BATS_TEST_FILENAME##*/}/_test_shared_dir"
    else
        wd="${gsd}/testing/${BATS_TEST_FILENAME##*/}/_test_shared_dir"
    fi
    echo $wd
}

# Create a working directory shared amongst all tests in a file
# Should be called from setup_file
function create_bats_file_wd() {
    local wd=$(get_bats_file_wd)

    if [[ "$wd" =~ ^.*/_test_shared_dir ]] ; then
        ( [ -d "$wd" ] && rm -rf "$wd" )
        mkdir -p $wd
    fi

    echo "${wd}"
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
