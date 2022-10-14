#!/bin/sh

## Gather and dependency-sort the initcall list
initcalls=$(${1} -j ".discard.init" -s "${2}" | grep "^[ \t]*[0-9a-f]" | xxd -r | tr "\0" "\n" | tsort)

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <kaneru/initcall.h>\n"
printf "#include <stddef.h>\n"

for i in ${initcalls}; do
    printf "extern void __init_$i(void);\n"
done

printf "const initcall_t __initcalls[] = {\n"
for i in ${initcalls}; do
    printf "&__init_$i,\n"
done
printf "NULL\n"
printf "};\n"
