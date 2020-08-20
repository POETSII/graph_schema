#!/bin/bash

set -eou pipefail

for i in output/*/*.v4.* output/*/*.base85.*  ; do
    B=$(basename $i)
    inst=${B%%.*}
    class=${inst%_*}
    scale=${inst##*_s}

    variant=${B#*.}
    size=$(stat -c%s $i)

    echo "$B,$inst,$variant,$class,$scale,$size"

    if [[ "$variant" == "v4.xml.gz" ]] ; then
        size=$(gunzip $i --stdout | wc -c - | sed -E -e "s/([0-9]+).*/\1/g")
        echo "$B,$inst,v4,$class,$scale,$size"
    fi
    if [[ "$variant" == "base85.xml.gz" ]] ; then
        size=$(gunzip $i --stdout | wc -c - | sed -E -e "s/([0-9]+).*/\1/g")
        echo "$B,$inst,base85,$class,$scale,$size"
    fi
done