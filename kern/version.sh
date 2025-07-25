#!/bin/sh
# argv[1]: sysname
# argv[2]: release

printf "// GENERATED AUTOMATICALLY\n"
printf "// WARNING: changes will be lost\n"

printf "#include <kern/version.h>\n"

printf "const char sysname[] = \"${1}\";\n"
printf "const char release[] = \"${2}\";\n"
printf "const char version[] = \"$(LC_ALL=C date +'%Y-%m-%d %H:%M:%S%z')\";\n"
