#!/bin/sh
# argv[1]: sysname
# argv[2]: release

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf "#include <core/version.h>\n"

printf "const char v_sysname[] = \"${1}\";\n"
printf "const char v_release[] = \"${2}\";\n"
printf "const char v_version[] = \"$(LC_ALL=C date +'%Y-%m-%d %H:%M:%S%z')\";\n"
