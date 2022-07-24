#!/bin/bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
graph_schema_dir="${script_dir}/.."

input_file=""
output_file=""
output_dir=""
CPPFLAGS=""

function error {
    >&2 echo "$1"
    exit 1
}



while [[ $# -gt 0 ]] ; do    
    key="$1"
    case $key in
    -o)
        [[ "$output_file" == "" ]] || error "Duplicate output file option" 
        [[ $# -gt 1 ]] || error "Missing output file value"
        output_file="$2"
        shift 2
        ;;
    --output-dir)
        [[ "$output_dir" == "" ]] || error "Duplicate output dir option"
        [[ $# -gt 1 ]] || error "Missing output dir value"
        output_dir="$2"
        shift 2
        ;;
    --working-dir)
        [[ "$working" == "" ]] || error "Duplicate working dir option"
        [[ $# -gt 1 ]] || error "Missing working dir value"
        working_dir="$2"
        shift 2
        ;;
    -I)
        [[ $# -gt 1 ]] || error "Missing include path"
        CPPFLAGS="$CPPFLAGS -I $2"
        shift 2
        ;;
    --release)
        # Go for reasonably optimal
        CPPFLAGS="$CPPFLAGS  -O2 -DNDEBUG=1"
        shift
        ;;
    -std=c++17)
        CPPFLAGS="$CPPFLAGS  -std=c++17"
        shift
        ;;
    *)
        [[ "$input_file" == "" ]] || error "More than one input file"
        input_file="$1"
        shift
        ;;
    esac
done


[[ "$input_file" != "" ]] || error "Input file not specified." 
[[ -f "$input_file" ]] || error "Input file $input_file does not exist." 


input_file_base=${input_file%.gz}
input_file_base=${input_file_base%.xml}

# TODO: This has to parse the entire graph, so is inefficient when dealing with large graphs
>&2 echo "Getting graph type id"
name=$(${graph_schema_dir}/tools/print_graph_type_id.py ${input_file}) || error "Couldn't get graph type id" 

if [[ "$working_dir" == "" ]] ; then
    working_dir=$(mktemp -d)
else
    mkdir -p $working_dir
fi

[[ "" != "$output_dir" ]] && [[ "" != "$output_file" ]] && error "Both --output-dir and -o were given. At most one can be used."
if [[ "$output_dir" != "" ]] ; then
    [[ -d "${output_dir}" ]] || error "--output-dir ${output_dir} : directory does not exist."
    output_file="${output_dir}/${name}.graph.so"
fi
if [[ "$output_file" == "" ]] ; then
    output_file="${name}.graph.so"
fi


# Unfortunately this is replicated from the makefile, so the same
# info is now in two places
# TODO : Have a single source somehow?

JING="${graph_schema_dir}/external/jing-20081028/bin/jing.jar"

if [[ ! -f ${JING} ]] ; then
    >&2 echo "Extracting jing"
    (cd ${graph_schema_dir} && make jing)
fi

export PYTHONPATH=${graph_schema_dir}/tools

CPPFLAGS+=" -g -I ${graph_schema_dir}/include -W -Wall -Wno-unused-parameter -Wno-unused-variable"
CPPFLAGS+=" -I ${graph_schema_dir}/external/robin_hood"
CPPFLAGS+=" -DPOETS_COMPILING_AS_PROVIDER=1"

# For some reason pkg-config stopped working on byron...
if pkg-config libxml++-2.6 ; then
CPPFLAGS="$CPPFLAGS $(pkg-config --cflags libxml++-2.6)"
LDLIBS="$LDLIBS $(pkg-config --libs-only-l libxml++-2.6)"
LDFLAGS+="$LDFLAGS $(pkg-config --libs-only-L --libs-only-other libxml++-2.6)"
else
CPPFLAGS="$CPPFLAGS -I/usr/include/libxml++-2.6 -I/usr/lib/x86_64-linux-gnu/libxml++-2.6/include -I/usr/include/libxml2 -I/usr/include/glibmm-2.4 -I/usr/lib/x86_64-linux-gnu/glibmm-2.4/include -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/sigc++-2.0 -I/usr/lib/x86_64-linux-gnu/sigc++-2.0/include"
LDLIBS="$LDLIBS -lxml++-2.6 -lxml2 -lglibmm-2.4 -lgobject-2.0 -lglib-2.0 -lsigc-2.0"
fi

CPPFLAGS+=" -Wno-unused-local-typedefs -Wno-unused-but-set-variable"

CPPFLAGS+=" -I ~/local/include"

CPPFLAGS+=" -mfpmath=sse -msse2"
CPPFLAGS+=" -frounding-math -fsignaling-nans -fmax-errors=1"

SO_CPPFLAGS+=" -shared -fPIC"
LDFLAGS+=" -pthread"
LDLIBS+=" -ldl -fPIC"

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



if grep "https://poets-project.org/schemas/virtual-graph-schema-v3" $input_file > /dev/null ; then
    java -jar ${JING} -c ${graph_schema_dir}/master/virtual-graph-schema-v3.rnc ${input_file} || exit 1
fi

python3 ${graph_schema_dir}/tools/render_graph_as_cpp.py ${input_file} ${working_dir}/${name}.graph.cpp || exit 1

python3 ${graph_schema_dir}/tools/render_graph_as_cpp.py --header < ${input_file} > ${working_dir}/${name}.graph.hpp || exit 1

OBJS=""

search="${input_file_base}.external.cpp"
>&2 echo "Looking for in-proc external at ${search}"
if [[ -f "${search}" ]] ; then
    inproc_external_path="${search}"
    2>&1 echo "Compiling provider ${name} external using inproc source ${inproc_external_path}"

    CPPFLAGS+=" -DPOETS_HAVE_IN_PROC_EXTERNAL_MAIN=1"

    g++ -c ${CPPFLAGS} ${SO_CPPFLAGS} ${inproc_external_path} -o ${working_dir}/${name}.external.o || exit 1
     OBJS+=" ${working_dir}/${name}.external.o"
fi

2>&1 echo "Compiling provider ${name} devices"
X="g++ -c ${CPPFLAGS} ${SO_CPPFLAGS} ${working_dir}/${name}.graph.cpp -o ${working_dir}/${name}.graph.o"
2>&1 echo "  $X"
$X || exit 1
OBJS+=" ${working_dir}/${name}.graph.o"

2>&1 echo "Linking provider ${name}"
g++ ${CPPFLAGS} ${SO_CPPFLAGS} ${OBJS} -o ${output_file} ${LDFLAGS} ${LDLIBS} || exit 1

>&2 echo "Provider is at ${output_file}"
