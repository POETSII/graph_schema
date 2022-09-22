#!/bin/bash

poems_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
graph_schema_dir="${poems_dir}/../.."
sprovider_dir="${graph_schema_dir}/tools/sprovider"

function usage ()
{
>&2 echo "compile_poems_sim.sh : [-o <ExeFile>] [--working-dir dir] [--debug|--release|--release-with-asserts] input-file
    -o : Name of the simulation execution. Default is ./poems_sim
    --working-dir dir : Where to place temporary files from compilation. Default comes from mktemp.
    --release : Attempt to create fastest possible executable with no safety (default).
    --release-with-asserts : Attempt to create fastest possible executable, but keep run-time checks.
    --debug : Debuggable executable with all run-time checks.
    --sanitizers : Add thread and undefined sanitisers
    --run : If the simulation compiles, then run it immediately
    input-file the XML graph type or graph instance to compile.
"
}

CPPFLAGS=""
LDFLAGS=""

output_file=poems_sim
input_file=""
working_dir=$(mktemp -d)
optimise=1
asserts=0
sanitizers=0
max_log_level=
run=0
while true; do
    case "$1" in
    --help ) usage ; exit 1 ;;
    -o | --output ) output_file=$2 ; shift 2 ;;
    --working-dir ) working_dir=$2 ; shift 2 ;;
    -I ) CPPFLAGS="$CPPFLAGS -I $2" ; shift 2 ;;
    -L ) LDFLAGS="$LDFLAGS -L $2" ; shift 2 ;;
    --release ) optimise=1 ; asserts=0 ; shift ;;
    --release-with-asserts ) optimise=1 ; asserts=1 ; shift ;;
    --debug ) optimise=0 ; asserts=1 ; shift ;;
    --sanitizers ) sanitizers=1 ; shift ;;
    --run ) run=1 ; shift ;;
    --max-log-level ) max_log_level=$2 ; shift 2 ;;
    -* ) >&2 echo "Unknown option $1" ; exit 1 ;;
    "" ) break ;;
    * ) if [[ "$input_file" != "" ]] ; then 
            >&2 echo "Received multiple input files (prev=\"$input_file\")" ; exit 1 ;
        else
            input_file="$1" ;
            shift ;
        fi
        ;;
  esac
done

if [[ "$max_log_level" == "" ]] ; then
    if [[ $optimize -eq 1 ]] ; then
        max_log_level=3
    else
        max_log_level=100
    fi
fi

if [[ "$input_file" == "" ]] ; then
    >&2 echo "No input file specified."
    exit 1
fi

>&2 echo "Rendering provider as sprovider"
${sprovider_dir}/render_graph_as_sprovider.py "${input_file}" > ${working_dir}/sprovider_impl.hpp || exit 1

>&2 echo "Compiling poems sim"

echo '#include "sprovider_impl.hpp"' > ${working_dir}/poems_sim.cpp
cat ${poems_dir}/generic_poems_loader.cpp >> ${working_dir}/poems_sim.cpp

# For some reason pkg-config stopped working on byron...
if pkg-config libxml++-2.6 ; then
LIBXML_PKG_CONFIG_CPPFLAGS=$(pkg-config --cflags libxml++-2.6)
LIBXML_PKG_CONFIG_LDLIBS=$(pkg-config --libs-only-l libxml++-2.6)
LIBXML_PKG_CONFIG_LDFLAGS=$(pkg-config --libs-only-L --libs-only-other libxml++-2.6)
else
>&2 echo "pkg config is broken"
LIBXML_PKG_CONFIG_CPPFLAGS="-I/usr/include/libxml++-2.6 -I/usr/lib/x86_64-linux-gnu/libxml++-2.6/include -I/usr/include/libxml2 -I/usr/include/glibmm-2.4 -I/usr/lib/x86_64-linux-gnu/glibmm-2.4/include -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/sigc++-2.0 -I/usr/lib/x86_64-linux-gnu/sigc++-2.0/include"
LIBXML_PKG_CONFIG_LDLIBS="-lxml++-2.6 -lxml2 -lglibmm-2.4 -lgobject-2.0 -lglib-2.0 -lsigc-2.0"
LIBXML_PKG_CONFIG_LDFLAGS=
fi

CPPFLAGS+=" -std=c++17"
CPPFLAGS+=" -I include -W -Wall -Wno-unused-parameter -Wno-unused-variable"
CPPFLAGS+=" -I include/include_cache"
CPPFLAGS+=" -I ~/local/include"
CPPFLAGS+=" ${LIBXML_PKG_CONFIG_CPPFLAGS}"
CPPFLAGS+=" -Wno-unused-local-typedefs"
CPPFLAGS+=" -Wno-unused-function"
# This is needed due to use of std::auto_ptr in libxml++
CPPFLAGS+=" -Wno-deprecated-declarations"
CPPFLAGS+=" -I ${graph_schema_dir}/include"
CPPFLAGS+=" -I ${graph_schema_dir}/external/robin_hood"
CPPFLAGS+=" -I ${sprovider_dir}"
CPPFLAGS+=" -I ${poems_dir}"
CPPFLAGS+=" -DRAPIDJSON_HAS_STDSTRING=1"
CPPFLAGS+=" -g"
#CPPFLAGS+=" -DNDEBUG=1"
if [[ $optimise -eq 1 ]] ; then
    CPPFLAGS+=" -O3 -fwhole-program"
fi
if [[ $asserts -eq 0 ]] ; then
    CPPFLAGS+=" -DNDEBUG=1"
fi
if [[ $sanitizers -eq 1 ]] ; then
    #CPPFLAGS+=" -fsanitize=thread -fsanitize=undefined"
    CPPFLAGS+=" -fsanitize=address -fsanitize=undefined"
fi

if [[ "${POETS_EXTERNAL_INTERFACE_SPEC}" == "" ]] ; then
    if [[ -d "${graph_schema_dir}/../external_interface_spec" ]] ; then
        POETS_EXTERNAL_INTERFACE_SPEC="${graph_schema_dir}/../external_interface_spec"
    fi
fi
if [[ "${POETS_EXTERNAL_INTERFACE_SPEC}" == "" ]] ; then
	HAVE_POETS_EXTERNAL_INTERFACE_SPEC=0
	CPPFLAGS+=" -I ${graph_schema_dir}/include/include_cache"
else
	HAVE_POETS_EXTERNAL_INTERFACE_SPEC=1
	CPPFLAGS+=" -I ${POETS_EXTERNAL_INTERFACE_SPEC}/include"
fi

LDLIBS+=" ${LIBXML_PKG_CONFIG_LDLIBS} -ltbb -lmetis -ldl"
LDFLAGS+=" ${LIBXML_PKG_CONFIG_LDFLAGS} -pthread"

>&2 echo "CPPFLAGS=${CPPFLAGS}"

g++ -c ${working_dir}/poems_sim.cpp -DSPROVIDER_MAX_LOG_LEVEL=${max_log_level} -o ${working_dir}/poems_sim.o ${CPPFLAGS} || exit 1
g++ ${working_dir}/poems_sim.o -o ${output_file} ${CPPFLAGS} ${LDFLAGS} ${LDLIBS} || exit 1

if [[ $run -eq 1 ]] ; then
    $(realpath ${output_file}) "${input_file}"
fi
