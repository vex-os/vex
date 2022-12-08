#!/bin/bash

# we want the symbol list to be a bash array of strings
# separated by a newline, so we have to explicitly state that
# elements within that array are separated, well... by a newline.
IFS=$'\n'

# gather and sort the list of kernel symbols
symbols=($("${1}" "${2}" | grep " [TtWw] " | sort))

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <kaneru/trace.h>\n"
printf "#include <stddef.h>\n"

printf "const struct symbol __kernel_symbols[] = {\n"
for sym in ${symbols[@]}
do
    IFS=$' '
    symw=(${sym})
    printf "{ .address = 0x${symw[0]}, .name = \"${symw[2]}\" },\n"
done
printf "{ .address = 0, .name = NULL }\n"
printf "};\n"
