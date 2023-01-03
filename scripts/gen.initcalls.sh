#!/bin/sh
# [1]: object file to parse

initcalls=$(llvm-readelf -p .discard.init "${1}" | grep -P "^\s*\[" | cut -d "]" -f2 | tsort)

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"
printf "#include <kan/initcall.h>\n"
printf "#include <stddef.h>\n"

# declare initcall functions
for initcall in ${initcalls}; do
    printf "extern int __init_${initcall}(void);\n"
done

printf "const initcall_t initcalls[] = {\n"
for initcall in ${initcalls}; do
    printf "{ .func = &__init_${initcall}, .name = \"${initcall}\" },\n"
done
printf "{ .func = NULL, .name = { 0 } }\n"
printf "};\n"
