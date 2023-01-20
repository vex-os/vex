#!/bin/sh
# argv[1]: input file
# argv[2]: output file

truncate -s 0 ${2}
IFS=''
while read line
do
    line=$(echo ${line//char/uint32_t})
    line=$(echo ${line//str/wcs})
    line=$(echo ${line//wcsing.h/wchar.h})
    line=$(echo ${line//rewcsict/restrict})
    echo ${line} >> ${2}
done < ${1}
