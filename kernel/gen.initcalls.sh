#!/bin/sh

## Gather and dependency-sort the initcall list
initcalls=$(${1} -j ".discard.init" -s "${2}" | grep "^[ \t]*[0-9a-f]" | xxd -r | tr "\0" "\n" | tsort)

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <kaneru/initcall.h>\n"
printf "#include <stddef.h>\n"

for i in ${initcalls}; do
    printf "extern int __init_${i}(void);\n"
done

printf "const struct initcall __initcalls[] = {\n"
for i in ${initcalls}; do
    printf "{ .func = &__init_${i}, .name = \"${i:0:63}\" },\n"
done
printf "{ .func = NULL, .name = { 0 } }\n"
printf "};\n"
