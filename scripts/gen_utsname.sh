#!/bin/sh
# argv[1]: release
# argv[2]: sysname

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf "#include <iserix/utsname.h>\n"

printf "const char sysname[] = \"${2}\";\n"
printf "const char release[] = \"${1}\";\n"
printf "const char version[] = \"$(LC_ALL=C date +'%Y-%m-%d %H:%M:%S%z')\";\n"
