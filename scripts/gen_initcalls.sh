#!/bin/sh
# argv[1]: object file to parse

initcalls=$(readelf -p .initcalls "${1}" | grep -P "^\s*\[" | cut -d "]" -f2 | tsort)

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <vex/initcall.h>\n"
printf "#include <stddef.h>\n"

for initcall in ${initcalls}; do
    printf "extern void _init_${initcall}(void);\n"
done

printf "const initcall_t initcalls[] = {\n"
for initcall in ${initcalls}; do
    printf "&_init_${initcall},\n"
done
printf "NULL\n"
printf "};\n"
