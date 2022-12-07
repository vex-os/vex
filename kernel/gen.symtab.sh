#!/bin/bash

## Gather and sort the symbol list
IFS=$'\n' symbols=( $("${1}" -t "${2}" | grep ".text" | sort) )

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <kaneru/trace.h>\n"
printf "#include <stddef.h>\n"

printf "const struct symbol __kernel_symbols[] = {\n"
for i in ${symbols[@]}; do
    printf "{ .address = 0x${i%% *}, .name = \"${i##* }\" },\n"
done
printf "{ .address = 0, .name = NULL }\n"
printf "};"
