#!/bin/sh
# argv[1]: version string
# argv[2]: machine string

sys_build=$(LC_ALL=C date -u)
sys_machine=${2}
sys_revision=$(git rev-parse --short=8 HEAD || echo "00000000")
sys_version=${1}

printf "/* generated automatically */\n"
printf "/* warning: changes will be lost */\n"

printf "#include <sys/version.h>\n"
printf "const char sys_build[] = \"${sys_build}\";\n"
printf "const char sys_machine[] = \"${sys_machine}\";\n"
printf "const char sys_revision[] = \"${sys_revision}\";\n"
printf "const char sys_version[] = \"${sys_version}\";\n"
