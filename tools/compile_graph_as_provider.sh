#!/bin/bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
graph_schema_dir="${script_dir}/.."

working_dir=$(mktemp -d)

input_file=$1

# Unfortunately this is replicated from the makefile, so the same
# info is now in two places
# TODO : Have a single source somehow?

JING="${graph_schema_dir}/external/jing-20081028/bin/jing.jar"

export PYTHONPATH=${graph_schema_dir}/tools

CPPFLAGS=" -I ${graph_schema_dir}/include -W -Wall -Wno-unused-parameter -Wno-unused-variable"

CPPFLAGS+=" $(pkg-config --cflags libxml++-2.6)"
CPPFLAGS+=" -Wno-unused-local-typedefs"

LDLIBS="$(pkg-config --libs-only-l libxml++-2.6)"
LDFLAGS+="$(pkg-config --libs-only-L --libs-only-other libxml++-2.6)"

SO_CPPFLAGS+=" -shared -fPIC"
LDFLAGS+=" -pthread"
LDLIBS+=" -ldl -fPIC"

#name=$(basename ${input_file} .xml)
name=$(${graph_schema_dir}/tools/print_graph_type_id.py ${input_file}) || exit 1

java -jar ${JING} -c ${graph_schema_dir}/master/virtual-graph-schema-v2.rnc ${input_file} || exit 1

python3 ${graph_schema_dir}/tools/render_graph_as_cpp.py ${input_file} ${working_dir}/${name}.graph.cpp || exit 1

python3 ${graph_schema_dir}/tools/render_graph_as_cpp.py --header < ${input_file} > ${working_dir}/${name}.graph.hpp || exit 1

g++ ${CPPFLAGS} -Wno-unused-but-set-variable ${SO_CPPFLAGS} ${working_dir}/${name}.graph.cpp -o ${working_dir}/${name}.graph.so ${LDFLAGS} ${LDLIBS}

echo ${working_dir} 
cp ${working_dir}/${name}.graph.so .
echo "Provider is at ${name}.graph.so"
